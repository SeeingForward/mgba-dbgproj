/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include "ui_DebuggerGUI.h"

#include "CoreController.h"

namespace QGBA {

class DebuggerGUIController;

class DebuggerGUI : public QWidget {
Q_OBJECT

public:
DebuggerGUI(DebuggerGUIController* controller, QWidget* parent = nullptr);
DebuggerGUI(DebuggerGUIController* controller,
			std::shared_ptr<CoreController> coreController,
	        QWidget* parent = nullptr);
	void DebuggerGUI::PrintCode(uint32_t startAddress = 0);

public slots:
	void DebuggerGUI::HandleGamePause();
	void DebuggerGUI::HandleGameResume();
	void DebuggerGUI::UpdateWidgets();
	void DebuggerGUI::UpdateRegister(int index);
	void DebuggerGUI::UpdateRegisters();
	void DebuggerGUI::HandleChangedRegisterCell(int row, int column);

signals:

private:
	Ui::DebuggerGUI m_ui;

	DebuggerGUIController* m_guiController;
};

}
