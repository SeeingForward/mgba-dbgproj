/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// File created by MSgitstudent and SeeingForward

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

static int getRegisterIndex(QString addressLine) {
	int index = -1;
	QString token = addressLine.trimmed().toLower();

	if (token.startsWith('r', Qt::CaseInsensitive)) {
		index = token.remove(0, 1).toInt(0, 10);

		if (index < 0 || index >= GBA_GPR_COUNT) {
			index = -1;
		}
	} else if (token == "sp") {
		index = 13;
	} else if (token == "lr") {
		index = 14;
	} else if (token == "pc") {
		index = 15;
	}

	return index;
}

// Get address value from address line textbox
static uint32_t getLineAddress(mCore* core, QString line) {
	uint32_t address = -1;
	
	if (line.contains("0x")) {
		address = line.remove("0x", Qt::CaseInsensitive).toInt(nullptr, 16);
	} else if (!line.isEmpty()) {
		int regIndex = getRegisterIndex(line);

		if (regIndex != -1) {
			if (core) {
				auto cpu = (struct ARMCore*) core->cpu;

				if (cpu) {
					address = (uint32_t) cpu->gprs[regIndex];
				}
			}
		} else {
			address = line.toInt(0, 16);
		}
	}

	return address;
}

void DebuggerGUI::HandleAddressButtonClicked(bool checked) {
	auto core        = m_CoreController.get()->thread()->core;
	QString line     = m_ui.txtAddressLine->text();
	uint32_t address = getLineAddress(core, line);

	if (address != -1) {
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
		m_codeAddress = 0;
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

static void DecodeThumbBranch(mCore *core, ARMInstructionInfo *info, uint32_t address, int *instrIndex) {
	struct ARMInstructionInfo info2, combined;

	// Decode immediately following instruction
	uint16_t opcode2 = core->busRead16(core, address + WORD_SIZE_THUMB);
	ARMDecodeThumb(opcode2, &info2);

	// If this is a proper BL instruction, find out the target address
	if (ARMDecodeThumbCombine(info, &info2, &combined)) {
		memcpy(info, &combined, sizeof(*info));
		*instrIndex += 1;
	}
}

static void DecodeInstruction(mCore* core, ARMInstructionInfo* info, QString* instr, quint32 startAddress, int i,
                                    int* instrIndex, int instrSize, bool isThumb) {
	char instrBuffer[128] = { 0 };
	uint32_t address = startAddress + (*instrIndex * instrSize);
	auto cpu = (struct ARMCore*) core->cpu;

	if (isThumb) {
		// Clear least-significant bit, so we don't decode unaligned addresses
		address &= ~0x1;

		uint16_t opcode = core->busRead16(core, address);
		ARMDecodeThumb(opcode, info);

		// Handle instructions with BL mnemonic (2x 2-bytes)
		if (info->mnemonic == ARM_MN_BL) {
			DecodeThumbBranch(core, info, address, instrIndex);
		}
	} else {
		// Clear least-significant bits, so we don't decode unaligned addresses
		address &= ~0x3;

		uint32_t opcode = core->busRead32(core, address);
		ARMDecodeARM(opcode, info);
	}

	// TODO: PUSH/POP
	ARMDisassemble(info, cpu, NULL, address + (2 * instrSize), instrBuffer, sizeof(instrBuffer));
	instr->sprintf("0x%08X: %s", address, instrBuffer);
}

void DebuggerGUI::PrintCode(quint32 startAddress) {
	m_ui.listCode->clear();

	if (m_CoreController)
	{
		auto core	 = m_CoreController.get()->thread()->core;

		int visibleLinesCount = getVisibleCodeLinesCount();

		const int instrSize = (m_isThumb) ? WORD_SIZE_THUMB : WORD_SIZE_ARM;

		struct ARMInstructionInfo info;
		QString instr = "";
		for (int i = 0, instrIndex = 0; i < visibleLinesCount; i++, instrIndex++) {
			DecodeInstruction(core, &info, &instr, startAddress, i, &instrIndex, instrSize, m_isThumb);
			m_ui.listCode->addItem(instr);
		}
	}
}
