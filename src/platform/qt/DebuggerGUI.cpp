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

DebuggerGUI::DebuggerGUI(DebuggerGUIController* controller,
						 std::shared_ptr<CoreController> coreController,
                         QWidget* parent)
	: QWidget(parent)
	, m_guiController(controller) { 
	
	m_ui.setupUi(this);
	connect(controller, &DebuggerGUIController::log, m_ui.log, &LogWidget::log);
	
//	connect(m_ui.watchWindow, &QTableWidget::cellChanged, this, &DebuggerGUI::Something);
	connect(m_ui.registers, &QTableWidget::cellChanged, this, &DebuggerGUI::HandleChangedRegisterCell);

	char buffer[32];

	if (coreController) {
		connect(coreController.get(), &CoreController::started, this, &DebuggerGUI::UpdateWidgets);
		connect(coreController.get(), &CoreController::stopping, this, &DebuggerGUI::HandleGamePause);
		connect(coreController.get(), &CoreController::paused, this, &DebuggerGUI::HandleGamePause);
		connect(coreController.get(), &CoreController::unpaused, this, &DebuggerGUI::HandleGameResume);
		connect(coreController.get(), &CoreController::frameAvailable, this, &DebuggerGUI::UpdateWidgets);

		// TODO: This makes the Resume-connect pointless, since we enable editing by clicking the Table
		connect(m_ui.registers, &QTableView::clicked, this, &DebuggerGUI::HandleGamePause);
	}

	PrintCode(0x080000FC);
}

void DebuggerGUI::HandleChangedRegisterCell(int row, int column) {
	ARMRegisterFile* arf = m_guiController->getGbaRegisters();

	if (column == 0 && row < GBA_GPR_COUNT) {

	}
}

void DebuggerGUI::UpdateRegisters() {
	ARMRegisterFile* arf = m_guiController->getGbaRegisters();

	for (int i = 0; i < GBA_GPR_COUNT; i++) {
		auto item = m_ui.registers->item(i, 0);
		if (item) {
			// Since 0x should not be capitalized, it´s created separately.
			QString hex("0x");
			hex.append(QString("%1").arg(arf->gprs[i], 8, 16, QLatin1Char('0')).toUpper());
			item->setText(hex);
		}
	}
}

void DebuggerGUI::HandleGamePause() {
	auto triggers = m_ui.registers->editTriggers();
	triggers.setFlag(QAbstractItemView::EditTrigger::NoEditTriggers, false);
	triggers.setFlag(QAbstractItemView::EditTrigger::DoubleClicked, true);
	triggers.setFlag(QAbstractItemView::EditTrigger::EditKeyPressed, true);
	triggers.setFlag(QAbstractItemView::EditTrigger::AnyKeyPressed, true);
	m_ui.registers->setEditTriggers(triggers);
	
	UpdateWidgets();
}

void DebuggerGUI::HandleGameResume() {
	QAbstractItemView::EditTriggers(triggers);
	triggers.setFlag(QAbstractItemView::EditTrigger::NoEditTriggers, true);
	m_ui.registers->setEditTriggers(triggers);

	UpdateWidgets();
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

	if (printAsm) {
		m_guiController->attach();

		QString cpuMode = (isThumb) ? "t" : "a";

		// TOOD: Handle bl instructions
		int instrSize   = (isThumb) ? 2 : 4;

		for (int i = 0; i < GBA_GPR_COUNT; i++) {
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
