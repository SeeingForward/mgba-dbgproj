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
	m_backend.printf = printf;
	m_backend.init = init;
	m_backend.deinit = deinit;
	m_backend.poll = poll;
	m_backend.readline = readLine;
	m_backend.lineAppend = lineAppend;
	m_backend.historyLast = historyLast;
	m_backend.historyAppend = historyAppend;
	m_backend.interrupt = interrupt;
	m_backend.self = this;

	CLIDebuggerCreate(&m_cliDebugger);
	CLIDebuggerAttachBackend(&m_cliDebugger, &m_backend);
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

void DebuggerGUIController::enterLine(const QString& line) {
	CoreController::Interrupter interrupter(m_gameController);
	QMutexLocker lock(&m_mutex);
	m_lines.append(line);
	if (m_cliDebugger.d.p && m_cliDebugger.d.p->state == DEBUGGER_RUNNING) {
		mDebuggerEnter(m_cliDebugger.d.p, DEBUGGER_ENTER_MANUAL, nullptr);
	}
	m_cond.wakeOne();
}

void DebuggerGUIController::detach() {
	{
		CoreController::Interrupter interrupter(m_gameController);
		QMutexLocker lock(&m_mutex);
		if (m_cliDebugger.d.p && m_cliDebugger.d.p->state != DEBUGGER_SHUTDOWN) {
			m_lines.append(QString());
			m_cond.wakeOne();
		}
	}
	DebuggerController::detach();
	historySave();
}

void DebuggerGUIController::attachInternal() {
	CoreController::Interrupter interrupter(m_gameController);
	QMutexLocker lock(&m_mutex);
	mCore* core = m_gameController->thread()->core;
	CLIDebuggerAttachBackend(&m_cliDebugger, &m_backend);
	CLIDebuggerAttachSystem(&m_cliDebugger, core->cliDebuggerSystem(core));
}

void DebuggerGUIController::printf(struct CLIDebuggerBackend* be, const char* fmt, ...) {
	Backend* consoleBe = reinterpret_cast<Backend*>(be);
	DebuggerGUIController* self = consoleBe->self;
	va_list args;
	va_start(args, fmt);
	self->log(QString::vasprintf(fmt, args));
	va_end(args);
}

void DebuggerGUIController::init(struct CLIDebuggerBackend* be) {
	Backend* consoleBe = reinterpret_cast<Backend*>(be);
	DebuggerGUIController* self = consoleBe->self;
	UNUSED(self);
}

void DebuggerGUIController::deinit(struct CLIDebuggerBackend* be) {
	Backend* consoleBe = reinterpret_cast<Backend*>(be);
	DebuggerGUIController* self = consoleBe->self;
	if (QThread::currentThread() == self->thread() && be->p->d.p->state != DEBUGGER_SHUTDOWN) {
		self->m_lines.append(QString());
		self->m_cond.wakeOne();
	}
}

int DebuggerGUIController::poll(struct CLIDebuggerBackend* be, int32_t timeoutMs) {
	Backend* consoleBe = reinterpret_cast<Backend*>(be);
	DebuggerGUIController* self = consoleBe->self;
	QMutexLocker lock(&self->m_mutex);
	self->m_cond.wait(&self->m_mutex, timeoutMs < 0 ? ULONG_MAX : static_cast<unsigned long>(timeoutMs));
	return !self->m_lines.isEmpty();
}

const char* DebuggerGUIController::readLine(struct CLIDebuggerBackend* be, size_t* len) {
	Backend* consoleBe = reinterpret_cast<Backend*>(be);
	DebuggerGUIController* self = consoleBe->self;
	QMutexLocker lock(&self->m_mutex);
	while (self->m_lines.isEmpty()) {
		self->m_cond.wait(&self->m_mutex);
	}
	QString last = self->m_lines.takeFirst();
	if (last.isNull()) {
		return nullptr;
	}
	self->m_last = last.toUtf8();
	*len = self->m_last.size();
	return self->m_last.constData();

}

void DebuggerGUIController::lineAppend(struct CLIDebuggerBackend* be, const char* line) {
	Backend* consoleBe = reinterpret_cast<Backend*>(be);
	DebuggerGUIController* self = consoleBe->self;
	self->lineAppend(QString::fromUtf8(line));
}

const char* DebuggerGUIController::historyLast(struct CLIDebuggerBackend* be, size_t* len) {
	Backend* consoleBe = reinterpret_cast<Backend*>(be);
	DebuggerGUIController* self = consoleBe->self;
	CoreController::Interrupter interrupter(self->m_gameController);
	QMutexLocker lock(&self->m_mutex);
	if (self->m_history.isEmpty()) {
		return "i";
	}
	self->m_last = self->m_history.last().toUtf8();
	*len = self->m_last.size();
	return self->m_last.constData();
}

void DebuggerGUIController::historyAppend(struct CLIDebuggerBackend* be, const char* line) {
	Backend* consoleBe = reinterpret_cast<Backend*>(be);
	DebuggerGUIController* self = consoleBe->self;
	CoreController::Interrupter interrupter(self->m_gameController);
	QMutexLocker lock(&self->m_mutex);
	self->m_history.append(QString::fromUtf8(line));
}

void DebuggerGUIController::interrupt(struct CLIDebuggerBackend* be) {
	Backend* consoleBe = reinterpret_cast<Backend*>(be);
	DebuggerGUIController* self = consoleBe->self;
	QMutexLocker lock(&self->m_mutex);
	self->m_cond.wakeOne();
}

void DebuggerGUIController::historyLoad() {
	QFile log(ConfigController::configDir() + "/cli_history.log");
	QStringList history;
	if (!log.open(QIODevice::ReadOnly | QIODevice::Text)) {
		return;
	}
	while (true) {
		QByteArray line = log.readLine();
		if (line.isEmpty()) {
			break;
		}
		if (line.endsWith("\r\n")) {
			line.chop(2);
		} else if (line.endsWith("\n")) {
			line.chop(1);			
		}
		history.append(QString::fromUtf8(line));
	}
	QMutexLocker lock(&m_mutex);
	m_history = history;
}

void DebuggerGUIController::historySave() {
	QFile log(ConfigController::configDir() + "/cli_history.log");
	if (!log.open(QIODevice::WriteOnly | QIODevice::Text)) {
		LOG(QT, WARN) << tr("Could not open CLI history for writing");
		return;
	}
	for (const QString& line : m_history) {
		log.write(line.toUtf8());
		log.write("\n");
	}
}
