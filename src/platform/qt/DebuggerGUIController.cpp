/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "DebuggerGUIController.h"

#include "ConfigController.h"
#include "CoreController.h"
#include "LogController.h"

#include <QMutexLocker>
#include <QThread>

#include <mgba/internal/debugger/cli-debugger.h>
#include <mgba/internal/gba/gba.h>
#include <mgba/internal/arm/arm.h>

using namespace QGBA;

DebuggerGUIController::DebuggerGUIController(QObject* parent)
	: DebuggerController(&m_cliDebugger.d, parent)
{
	m_backend.self = this;
}

struct ARMRegisterFile *DebuggerGUIController::getGbaRegisters() {
	// Interrupt the emulation loop
	CoreController::Interrupter interrupter(m_gameController);
	QMutexLocker lock(&m_mutex);

	if (m_gameController) {
		GBA *gba = static_cast<GBA*>(m_gameController->thread()->core->board);
		return &gba->cpu->regs;
	}

	return NULL;
}

void DebuggerGUIController::setGbaRegister(uint8_t regId, uint32_t value) {
	CoreController::Interrupter interrupter(m_gameController);
	QMutexLocker lock(&m_mutex);

	if (regId < 16) {
		GBA* gba = static_cast<GBA*>(m_gameController->thread()->core->board);
		gba->cpu->regs.gprs[regId] = value;
	}
}
