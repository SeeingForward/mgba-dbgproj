/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DebuggerGUI.h"

#include "CoreController.h"
#include "DebuggerGUIController.h"
#include "GBAApp.h"

#include "mgba/internal/arm/arm.h"

#include <QKeyEvent>
#include <qt5/QtWidgets/qlineedit.h>

using namespace QGBA;

#define GBA_GPR_COUNT 16
#define GBA_ROM_BASE  0x08000000

DebuggerGUI::DebuggerGUI(DebuggerGUIController* controller,
						 std::shared_ptr<CoreController> coreController,
                         QWidget* parent)
	: QWidget(parent)
	, m_guiController(controller)
    , m_CoreController(coreController)
    , m_codeAddress(GBA_ROM_BASE) {
	m_ui.setupUi(this);

	connect(controller, &DebuggerGUIController::log, m_ui.log, &LogWidget::log);

	connect(m_ui.registers, &QTableWidget::cellChanged, this, &DebuggerGUI::HandleChangedRegisterCell);

	if (coreController) {
		auto coreCnt = coreController.get();
		connect(coreCnt, &CoreController::started,  this, &DebuggerGUI::UpdateWidgets);
		connect(coreCnt, &CoreController::stopping, this, &DebuggerGUI::HandleGamePause);
		connect(coreCnt, &CoreController::paused,   this, &DebuggerGUI::HandleGamePause);
		connect(coreCnt, &CoreController::unpaused, this, &DebuggerGUI::HandleGameResume);
		connect(coreCnt, &CoreController::frameAvailable, this, &DebuggerGUI::UpdateWidgets);

		// TODO: This makes the Resume-connect pointless, since we enable editing by clicking the Table
		//connect(m_ui.registers, &QTableView::clicked, this, &DebuggerGUI::HandleGamePause);
	}

	PrintCode(m_codeAddress);
}

void DebuggerGUI::HandleChangedRegisterCell(int row, int column) {
	if (m_CoreController) {
		//ARMRegisterFile* arf = m_guiController->getGbaRegisters();
		bool userCanEdit = !m_ui.registers->editTriggers().testFlag(QAbstractItemView::EditTrigger::NoEditTriggers);


		if (m_paused && userCanEdit && column == 0 && row < GBA_GPR_COUNT) {
			m_guiController->log(QString("(%1, %2) changed\n").arg(row).arg(column));
			
			QString regValue = m_ui.registers->item(row, column)->text();

			uint32_t value;
			if (regValue.startsWith("0x", Qt::CaseInsensitive)) {
				value = regValue.remove("0x", Qt::CaseInsensitive).toInt(nullptr, 16);
			} else {
				value = regValue.toInt(nullptr, 10);
			}

			m_guiController->setGbaRegister(row, value);
		}
	}
}

void DebuggerGUI::UpdateRegisters() {
	ARMRegisterFile* arf = m_guiController->getGbaRegisters();

	for (int i = 0; i < GBA_GPR_COUNT; i++) {
		auto item = m_ui.registers->item(i, 0);
		if (item) {
			// Since 0x should not be capitalized, it´s created separately.
			QString hex("0x");
			hex.append(QString("%1").arg((uint32_t)arf->gprs[i], 8, 16, QLatin1Char('0')).toUpper());
			item->setText(hex);
		}
	}
}

void DebuggerGUI::HandleGamePause() {
	UpdateWidgets();

	auto triggers = m_ui.registers->editTriggers();
	triggers.setFlag(QAbstractItemView::EditTrigger::NoEditTriggers, false);
	triggers.setFlag(QAbstractItemView::EditTrigger::DoubleClicked, true);
	triggers.setFlag(QAbstractItemView::EditTrigger::EditKeyPressed, true);
	triggers.setFlag(QAbstractItemView::EditTrigger::AnyKeyPressed, true);
	m_ui.registers->setEditTriggers(triggers);
	
	// Pause only after Widgets display the correct value
	m_paused = true;
}

void DebuggerGUI::HandleGameResume() {
	// Unpause before widgets get updated,
	// because the game thread is already running,
	// so displayed values might be invalid already.
	m_paused = false;

	UpdateWidgets();

	QAbstractItemView::EditTriggers(triggers);
	triggers.setFlag(QAbstractItemView::EditTrigger::NoEditTriggers, true);
	m_ui.registers->setEditTriggers(triggers);
}

void DebuggerGUI::UpdateWidgets() {
	UpdateRegisters();
}

void DebuggerGUI::PrintCode(quint32 startAddress) {
	// Test disassembling instructions
	bool printAsm = false;
	bool isThumb = true;
	bool debugInfoLoaded = false;

	if (!printAsm) {
		// TODO: Load and display source code

		if (debugInfoLoaded) {
			m_guiController->log("TODO:  Displaying source code not yet implemented\n");
		} else {
			m_guiController->log("ERROR: No source code loaded.\n");
			m_guiController->log("       Defaulting to Assembly view.\n");
			printAsm = true;
		}
	}

	if (printAsm)
	{
		m_guiController->attach();

		QString cpuMode = (isThumb) ? "t" : "a";

		// TOOD: Handle bl instructions
		int instrSize   = (isThumb) ? 2 : 4;

		for (int i = 0; i < 16; i++) {
			QString instr = "disassemble/";
			instr.append(cpuMode);
			instr.append(" ");
			instr.append(QStringLiteral("0x%1").arg(startAddress + i*instrSize, 0, 16));
			m_guiController->enterLine(instr);
		}

		// Continue emulator execution
		m_guiController->enterLine("c");

		m_guiController->detach();
	}
}
