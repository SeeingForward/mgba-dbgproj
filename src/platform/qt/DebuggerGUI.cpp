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

using namespace QGBA;

DebuggerGUI::DebuggerGUI(DebuggerGUIController *controller, QWidget* parent)
	: QWidget(parent)
	, m_guiController(controller) { 
	
	m_ui.setupUi(this);

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
}
