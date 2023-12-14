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

	QStringList history() const { return m_history; }
	struct ARMRegisterFile* getGbaRegisters();
	void DebuggerGUIController::setGbaRegister(uint8_t regId, uint32_t value);

signals:
	void log(const QString&);
	void lineAppend(const QString&);

public slots:
	void enterLine(const QString&);
	virtual void detach() override;
	void historyLoad();
	void historySave();

protected:
	virtual void attachInternal() override;

private:
	static void printf(struct CLIDebuggerBackend* be, const char* fmt, ...);
	static void init(struct CLIDebuggerBackend* be);
	static void deinit(struct CLIDebuggerBackend* be);
	static int poll(struct CLIDebuggerBackend* be, int32_t timeoutMs);
	static const char* readLine(struct CLIDebuggerBackend* be, size_t* len);
	static void lineAppend(struct CLIDebuggerBackend* be, const char* line);
	static const char* historyLast(struct CLIDebuggerBackend* be, size_t* len);
	static void historyAppend(struct CLIDebuggerBackend* be, const char* line);
	static void interrupt(struct CLIDebuggerBackend* be);


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
