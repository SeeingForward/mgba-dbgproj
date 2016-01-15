/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "lr35902.h"

#include "isa-lr35902.h"

void LR35902Init(struct LR35902Core* cpu) {
	cpu->master->init(cpu, cpu->master);
	size_t i;
	for (i = 0; i < cpu->numComponents; ++i) {
		if (cpu->components[i] && cpu->components[i]->init) {
			cpu->components[i]->init(cpu, cpu->components[i]);
		}
	}
}

void LR35902Deinit(struct LR35902Core* cpu) {
	if (cpu->master->deinit) {
		cpu->master->deinit(cpu->master);
	}
	size_t i;
	for (i = 0; i < cpu->numComponents; ++i) {
		if (cpu->components[i] && cpu->components[i]->deinit) {
			cpu->components[i]->deinit(cpu->components[i]);
		}
	}
}

void LR35902SetComponents(struct LR35902Core* cpu, struct LR35902Component* master, int extra, struct LR35902Component** extras) {
	cpu->master = master;
	cpu->numComponents = extra;
	cpu->components = extras;
}


void LR35902HotplugAttach(struct LR35902Core* cpu, size_t slot) {
	if (slot >= cpu->numComponents) {
		return;
	}
	cpu->components[slot]->init(cpu, cpu->components[slot]);
}

void LR35902HotplugDetach(struct LR35902Core* cpu, size_t slot) {
	if (slot >= cpu->numComponents) {
		return;
	}
	cpu->components[slot]->deinit(cpu->components[slot]);
}

void LR35902Reset(struct LR35902Core* cpu) {
	cpu->af = 0;
	cpu->bc = 0;
	cpu->de = 0;
	cpu->hl = 0;

	cpu->sp = 0;
	cpu->pc = 0;

	cpu->instruction = 0;

	cpu->cycles = 0;
	cpu->nextEvent = 0;
	cpu->executionState = LR35902_CORE_FETCH;
	cpu->halted = 0;

	cpu->irqh.reset(cpu);
}

void LR35902RaiseIRQ(struct LR35902Core* cpu, uint8_t vector) {
	cpu->irqPending = true;
	cpu->irqVector = vector;
}

void LR35902Tick(struct LR35902Core* cpu) {
	++cpu->cycles;
	enum LR35902ExecutionState state = cpu->executionState;
	++cpu->executionState;
	cpu->executionState &= 3;
	switch (state) {
	case LR35902_CORE_FETCH:
		if (cpu->irqPending) {
			cpu->pc = cpu->irqVector;
			cpu->irqPending = false;
			cpu->irqh.setInterrupts(cpu, false);
			// TODO: stall
		}
		cpu->bus = cpu->memory.load8(cpu, cpu->pc);
		break;
	case LR35902_CORE_DECODE:
		cpu->instruction = _lr35902InstructionTable[cpu->bus];
		++cpu->pc;
		break;
	case LR35902_CORE_EXECUTE:
		cpu->instruction(cpu);
		break;
	case LR35902_CORE_MEMORY_LOAD:
		cpu->bus = cpu->memory.load8(cpu, cpu->index);
		break;
	case LR35902_CORE_MEMORY_STORE:
		cpu->memory.store8(cpu, cpu->index, cpu->bus);
		break;
	case LR35902_CORE_READ_PC:
		cpu->bus = cpu->memory.load8(cpu, cpu->pc);
		++cpu->pc;
		cpu->executionState = LR35902_CORE_READ_PC_STALL;
		break;
	case LR35902_CORE_MEMORY_MOVE_INDEX_LOAD:
		cpu->executionState = LR35902_CORE_MEMORY_LOAD;
		break;
	case LR35902_CORE_MEMORY_MOVE_INDEX_STORE:
		cpu->executionState = LR35902_CORE_MEMORY_STORE;
		break;
	case LR35902_CORE_READ_PC_STALL:
	case LR35902_CORE_STALL:
		break;
	}
	if (cpu->cycles >= cpu->nextEvent) {
		cpu->irqh.processEvents(cpu);
	}
}
