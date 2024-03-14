/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include "DebuggerController.h"

#include <QMutex>
#include <QStringList>
#include <QWaitCondition>

#include <mgba/internal/debugger/cli-debugger.h>
#include "mgba/internal/arm/arm.h"

namespace QGBA {

class CoreController;

class DebuggerGUIController : public DebuggerController {
Q_OBJECT

public:
	DebuggerGUIController(QObject* parent = nullptr);

	struct ARMRegisterFile* getGbaRegisters();
	void DebuggerGUIController::setGbaRegister(uint8_t regId, uint32_t value);


	CLIDebugger m_cliDebugger{};

	QMutex m_mutex;
	QWaitCondition m_cond;
	QStringList m_history;
	QStringList m_lines;
	QByteArray m_last;

	uint32_t m_addr;

	struct Backend : public CLIDebuggerBackend {
		DebuggerGUIController* self;
	} m_backend;
};

}
