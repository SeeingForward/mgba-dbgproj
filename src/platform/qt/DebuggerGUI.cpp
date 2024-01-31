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
    , m_codeAddress(GBA_ROM_BASE) {
	m_ui.setupUi(this);

	connect(m_ui.registers, &QTableWidget::cellChanged, this, &DebuggerGUI::HandleChangedRegisterCell);
	connect(m_ui.btnGoTo,   &QPushButton::clicked,      this, &DebuggerGUI::HandleAddressButtonClicked);
	connect(m_ui.chkThumb,  &QAbstractButton::clicked,  this, &DebuggerGUI::HandleThumbCheckboxClicked);

	connect(m_ui.txtAddressLine, &QLineEdit::returnPressed, this, &DebuggerGUI::HandleAddressLineReturnPressed);

	if (coreController) {
		auto coreCnt = coreController.get();
		connect(coreCnt, &CoreController::started,  this, &DebuggerGUI::UpdateWidgets);
		connect(coreCnt, &CoreController::paused,   this, &DebuggerGUI::HandleGamePause);
		connect(coreCnt, &CoreController::unpaused, this, &DebuggerGUI::HandleGameResume);
		connect(coreCnt, &CoreController::frameAvailable, this, &DebuggerGUI::UpdateWidgets);

		// TODO: This makes the Resume-connect pointless, since we enable editing by clicking the Table
		//connect(m_ui.registers, &QTableView::clicked, this, &DebuggerGUI::HandleGamePause);

		// Find the target of the very first instruction, which has got to be a branch in official games.
		struct ARMInstructionInfo info;
		struct ARMCore* cpu = (struct ARMCore*)coreCnt->thread()->core->cpu;
		uint32_t opcode     = GBALoad32(cpu, m_codeAddress, NULL);

		ARMDecodeARM(opcode, &info);

		if (info.mnemonic == ARM_MN_B || info.mnemonic == ARM_MN_BL) {
			int offset = info.op1.reg + 0x8;
			m_codeAddress += offset;
		}
	}

	QString hex("0x");
	hex.append(QString("%1").arg(m_codeAddress, 8, 16, QLatin1Char('0')).toUpper());
	m_ui.txtAddressLine->setText(hex);
	
	PrintCode(m_codeAddress);
}

void DebuggerGUI::HandleThumbCheckboxClicked(bool checked) {
	m_isThumb = checked;
	PrintCode(m_codeAddress);
}

void DebuggerGUI::HandleAddressLineReturnPressed() {
	HandleAddressButtonClicked(true);
}

void DebuggerGUI::HandleAddressButtonClicked(bool checked) {
	// Get address value from textbox
	QString line     = m_ui.txtAddressLine->text();
	uint32_t address = 0;

	if (line.contains("0x")) {
		address = line.remove("0x", Qt::CaseInsensitive).toInt(nullptr, 16);
	} else if (!line.isEmpty()) {
		address = line.toInt(0, 16);
	} else {
		return;
	}

	if (address != 0) {
		int linesCount      = m_ui.listCode->count();
		int wordSize        = (m_isThumb) ? WORD_SIZE_THUMB : WORD_SIZE_ARM;
		uint32_t endAddress = m_codeAddress + linesCount * wordSize;
		
		// If the target line is already visible, just select it.
		if (address >= m_codeAddress && address < endAddress) {
			int selected = (address - m_codeAddress) / wordSize;
			m_ui.listCode->setItemSelected(m_ui.listCode->item(selected), true);
		} else {
			m_codeAddress = address;
			PrintCode(m_codeAddress);
		}
	} else {
		m_codeAddress = address;
		PrintCode(m_codeAddress);
	}
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

// Find out how many code lines to display
int DebuggerGUI::getVisibleCodeLinesCount() {
	int listHeight		  = m_ui.listCode->height();
	int listFontHeight	  = m_ui.listCode->font().pointSize();
	int visibleLinesCount = (listHeight / (1.8f * listFontHeight)) - 1;

	return visibleLinesCount;
}

void DebuggerGUI::PrintCode(quint32 startAddress) {
	m_ui.listCode->clear();

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
			const bool isThumb = m_isThumb;

			int visibleLinesCount = getVisibleCodeLinesCount();

			const int instrSize   = (isThumb) ? WORD_SIZE_THUMB : WORD_SIZE_ARM;

			char instrBuffer[128] = { 0 };

			QString instr = "";
			struct ARMInstructionInfo info;
			for (int i = 0, instrIndex = 0; i < visibleLinesCount; i++, instrIndex++) {
				uint32_t address = startAddress + (instrIndex * instrSize);

				if (isThumb) {
					uint16_t opcode = core->busRead16(core, address);
					ARMDecodeThumb(opcode, &info);

					if (info.mnemonic == ARM_MN_BL) {
						struct ARMInstructionInfo info2, combined;

						uint16_t opcode2 = core->busRead16(core, address + WORD_SIZE_THUMB);
						ARMDecodeThumb(opcode2, &info2);

						if (ARMDecodeThumbCombine(&info, &info2, &combined)) {
							memcpy(&info, &combined, sizeof(info));
							instrIndex++;
						}
					}
				} else {
					uint32_t opcode = core->busRead32(core, address);
					ARMDecodeARM(opcode, &info);
				}

				// TODO: PUSH/POP
				ARMDisassemble(&info, cpu, NULL, address + (2 * instrSize), instrBuffer, sizeof(instrBuffer));
				instr.sprintf("0x%08X: %s", address, instrBuffer);
				m_ui.listCode->addItem(instr);
			}
		}
	}
}
