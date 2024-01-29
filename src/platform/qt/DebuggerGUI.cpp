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
#include "mgba/internal/gba/memory.h" // GBALoad8/16/32

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
    , m_codeAddress(GBA_ROM_BASE + 0xC0) {
	m_ui.setupUi(this);

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
	bool printAsm = false;
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
		if (m_CoreController) {
			auto core	 = m_CoreController.get()->thread()->core;
			auto cpu	 = (struct ARMCore*) core->cpu;
			const bool isThumb = FALSE; // (cpu->executionMode == MODE_THUMB);

			// Find out how many code lines to display
			int listHeight        = m_ui.listWidget->height();
			int listFontHeight    = m_ui.listWidget->font().pointSize();
			int visibleLinesCount = (listHeight / (1.8f * listFontHeight)) - 1;

			const int instrSize   = (isThumb) ? WORD_SIZE_THUMB : WORD_SIZE_ARM;

			char instrBuffer[128] = { 0 };

			QString instr = "";
			struct ARMInstructionInfo info;
			for (int i = 0; i < visibleLinesCount; i++) {
				uint32_t address = startAddress + i * instrSize;
				// uint32_t address = cpu->regs.gprs[15] + i * 2;

				if (isThumb) {
					uint16_t opcode = GBALoad16(cpu, address, NULL);
					ARMDecodeThumb(opcode, &info);
				} else {
					uint32_t opcode = GBALoad32(cpu, address, NULL);
					ARMDecodeARM(opcode, &info);
				}

				ARMDisassemble(&info, cpu, NULL, address + (2 * instrSize), instrBuffer, sizeof(instrBuffer));
				instr.sprintf("0x%08X: %s", address, instrBuffer);
				m_ui.listWidget->addItem(instr);
			}
		}
	}
}
