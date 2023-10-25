/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DebuggerGUI.h"

#include "DebuggerGUIController.h"
#include "GBAApp.h"

#include "mgba/internal/arm/arm.h"

#include <QKeyEvent>
#include <qt5/QtWidgets/qlineedit.h>

using namespace QGBA;

DebuggerGUI::DebuggerGUI(DebuggerGUIController *controller, QWidget* parent)
	: QWidget(parent)
	, m_guiController(controller) { 
	
	m_ui.setupUi(this);
	connect(controller, &DebuggerGUIController::log, m_ui.log, &LogWidget::log);

	char buffer[32];


	QString* registersStr = new QString("Registers:\n");

	auto coreController = controller->getCoreController();
	if (coreController) {
		ARMRegisterFile* arf = controller->getGbaRegisters();

		for (int i = 0; i < 16; i++) {
			sprintf_s(buffer, sizeof(buffer), "r%d: 0x%08X\n", i, arf->gprs[i]);
			registersStr->append(buffer);
		}
	}

	m_ui.registers->setText(*registersStr);

	PrintCode(0x080000FC);

}

void DebuggerGUI::PrintCode(quint32 startAddress) {
	// Test disassembling instructions
	bool printAsm = true;
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
