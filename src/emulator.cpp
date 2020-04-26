#include <bitset>
#include <thread>
#include <mutex>
#include <chrono>

#include "emulator.h"
#include "instruction.h"
#include "exceptions.h"


uint16_t Emulator::periods[] = {
	500,
	1000,
	1500,
	2000,
	5000,
	10000,
	30000,
	60000
};


Emulator::~Emulator() {
	delete[] memory;
}


void Emulator::emulate(uint8_t* program) {
	memory = program;

	registers[PC] = memory[INITIALIZATION] | memory[INITIALIZATION + 1] << 8;

	running = true;
	instructionError = false;

	std::thread timer(tick, this);
	std::thread terminal(type, this);

	while (running) {
		fetch();

		execute();

		interrupt();
	}

	timer.join();
	terminal.join();
}


void Emulator::tick(Emulator* emulator) {
	while (true) {
		emulator->mem.lock();

		uint16_t period = periods[emulator->memory[TIMER_CFG] & 0b111];

		emulator->mem.unlock();

		std::this_thread::sleep_for(std::chrono::milliseconds(period));

		emulator->mtx.lock();

		if (!emulator->running) {
			emulator->mtx.unlock();
			break;
		}

		emulator->mtx.unlock();

		emulator->interruptRequests.push(TIMER);
	}
}


void Emulator::type(Emulator* emulator) {
	while (true) {
		char c = std::getchar();

		emulator->mtx.lock();

		if (!emulator->running) {
			emulator->mtx.unlock();
			break;
		}

		emulator->mtx.unlock();

		emulator->mem.lock();

		while (emulator->memory[DATA_IN] != 0) {
			emulator->mem.unlock();
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
			emulator->mem.lock();
		}

		emulator->memory[DATA_IN] = c;

		emulator->mem.unlock();

		emulator->interruptRequests.push(TERMINAL);
	}
}


void Emulator::fetch() {
	instruction = Instruction::extract(memory, registers[PC]);

	registers[PC] += instruction->size;
}


void Emulator::execute() {
	Instruction::CodedOperand* destination = dynamic_cast<Instruction::CodedOperand*>(instruction->destination);
	Instruction::CodedOperand* source	   = dynamic_cast<Instruction::CodedOperand*>(instruction->source);

	if ((!destination && instruction->destination) ||
		(!source && instruction->source))
		throw EmulatingException("Dynamic cast failed!");

	switch (instruction->code) {
	case HALT:
		running = false;
		break;
	case XCHG: {
		int16_t dst = getOperand(destination);
		int16_t src = getOperand(source);

		if (instructionError) break;

		storeResult(dst, source);
		storeResult(src, destination);

		break;
	}
	case INT: {
		push(PSW.to_ulong());

		int16_t dst = getOperand(destination);

		if (instructionError) break;

		uint8_t entry = dst % 8 * 2;
		registers[PC] = memory[entry] | memory[entry + 1] << 8;

		break;
	}
	case MOV: {
		int16_t src = getOperand(source);

		storeResult(src, destination);

		PSW.set(Z, src == 0);
		PSW.set(N, src < 0);

		break;
	}
	case ADD:
	case SUB:
	case MUL:
	case DIV:
	case CMP:
	case AND:
	case OR:
	case XOR:
	case TEST: {
		int16_t dst = getOperand(destination);
		int16_t src = getOperand(source);

		if (instructionError) break;

		int16_t result;

		switch (instruction->code) {
		case ADD:
			result = dst + src;
			PSW.set(O, (dst < 0 && src < 0 && result >= 0) || (dst >= 0 && src >= 0 && result < 0));
			PSW.set(C, (dst < 0 && src < 0 && result >= 0) || (dst < 0 && src >= 0 && result >= 0) || (dst >= 0 && src < 0 && result >= 0));
			break;
		case SUB:
		case CMP:
			result = dst - src;
			PSW.set(O, (dst >= 0 && src < 0 && result < 0)  || (dst < 0 && src >= 0 && result >= 0));
			PSW.set(C, (dst >= 0 && src < 0 && result >= 0) || (dst >= 0 && src < 0 && result < 0) || (dst >= 0 && src >= 0 && result < 0) || (dst < 0 && src < 0 && result < 0));
			break;
		case MUL:
			result = dst * src;
			break;
		case DIV:
			result = dst / src;
			registers[5] = dst % src;
			break;
		case AND:
		case TEST:
			result = dst & src;
			break;
		case OR:
			result = dst | src;
			break;
		case XOR:
			result = dst ^ src;
			break;
		}

		if (instruction->code != CMP && instruction->code != TEST)
			storeResult(result, destination);

		PSW.set(Z, result == 0);
		PSW.set(N, result < 0);

		break;
	}
	case NOT: {
		int16_t dst = getOperand(destination);

		if (instructionError) break;

		dst = ~dst;

		storeResult(dst, destination);

		PSW.set(Z, dst == 0);
		PSW.set(N, dst < 0);

		break;
	}
	case SHL:
	case SHR: {
		int16_t dst = getOperand(destination);
		int16_t src = getOperand(source);

		if (instructionError) break;

		if (src > 16) src = 16;
		uint16_t mask = 0x8000 >> (src - 1);

		int16_t result;
		if (instruction->code == SHL)
			result = dst << src;
		else
			result = dst >> src;

		storeResult(result, destination);

		PSW.set(Z, result == 0);
		PSW.set(C, dst & mask);
		PSW.set(N, result < 0);

		break;
	}
	case PUSH:
		push(getOperand(destination));
		break;
	case POP:
		storeResult(pop(), destination);
		break;
	case JMP:
		registers[PC] = getOperand(destination);
		break;
	case JEQ:
		if (PSW.test(Z))
			registers[PC] = getOperand(destination);
		break;
	case JNE:
		if (!PSW.test(Z))
			registers[PC] = getOperand(destination);
		break;
	case JGT:
		if (!((PSW.test(N) ^ PSW.test(O)) | PSW.test(Z)))
			registers[PC] = getOperand(destination);
		break;
	case CALL:
		push(registers[PC]);
		registers[PC] = getOperand(destination);
		break;
	case RET:
		registers[PC] = pop();
		break;
	case IRET: {
		std::bitset<16> oldPSW(pop());

		PSW = oldPSW;
		registers[PC] = pop();

		break;
	}
	default:
		instructionError = true;
		break;
	}

	delete instruction;
}


int16_t Emulator::getOperand(Instruction::CodedOperand* operand) {
	int16_t value;
	uint16_t address;

	switch (operand->addressing) {
	case IMMED:
		value = operand->value;
		break;
	case REG_DIR:
		if (operand->value == PSW_CODE)
			value = PSW.to_ulong();
		else
			value = registers[operand->value];

		if (instruction->operandSize == BYTE) {
			if (operand->displacement)
				value = (value & 0xFF00) >> 8;
			else
				value &= 0x00FF;
		}

		break;
	case REG_IND:
		address = registers[operand->value];
		break;
	case REG_IND_8:
	case REG_IND_16:
		address = registers[operand->value] + operand->displacement;
		break;
	case MEMORY:
		address = operand->value;
		break;
	default:
		instructionError = true;
		return -1;
	}

	if (operand->addressing != IMMED &&
		operand->addressing != REG_DIR) {
		mem.lock();

		value = memory[address];

		if (instruction->operandSize == WORD)
			value |= memory[address + 1] << 8;

		mem.unlock();
	}

	return value;
}


void Emulator::storeResult(int16_t result, Instruction::CodedOperand* operand) {
	uint16_t address;

	switch (operand->addressing) {
	case REG_DIR:
		if (operand->value == PSW_CODE) {
			std::bitset<16> newPSW(result);
			PSW = newPSW;
		}
		else
			registers[operand->value] = result;

		break;
	case REG_IND:
		address = registers[operand->value];
		break;
	case REG_IND_8:
	case REG_IND_16:
		address = registers[operand->value] + operand->displacement;
		break;
	case MEMORY:
		address = operand->value;
		break;
	default:
		instructionError = true;
		return;
	}

	if (operand->addressing != REG_DIR) {
		mem.lock();

		memory[address] = result & 0x00FF;

		if (instruction->operandSize == WORD)
			memory[address + 1] = (result & 0xFF00) >> 8;

		mem.unlock();

		if (address == DATA_OUT)
			std::cout << (char)result << std::flush;
	}
}


void Emulator::interrupt() {
	InterruptType type;

	if (!instructionError) {
		if (!PSW.test(I)) return;

		mtx.lock();

		if (interruptRequests.empty()) {
			mtx.unlock();
			return;
		}

		type = interruptRequests.front();
		interruptRequests.pop();

		mtx.unlock();
	}
	else
		type = INSTRUCTION_ERROR;

	if ((type == TIMER && !PSW.test(TR)) ||
		(type == TERMINAL && !PSW.test(TL))) return;

	push(registers[PC]);
	push(PSW.to_ulong());

	mem.lock();

	registers[PC] = memory[type * 2] | memory[type * 2 + 1] << 8;

	mem.unlock();

	PSW.reset(I);
}


void Emulator::push(int16_t value) {
	if (registers[SP] == STACK_START - STACK_SIZE)
		throw EmulatingException("Stack overflow!");

	uint8_t lower  =  value & 0x00FF;
	uint8_t higher = (value & 0xFF00) >> 8;

	mem.lock();

	memory[(uint16_t)(--registers[SP])] = higher;
	memory[(uint16_t)(--registers[SP])] = lower;

	mem.unlock();
}


int16_t Emulator::pop() {
	if (registers[SP] == STACK_START)
		throw EmulatingException("Stack underflow!");

	mem.lock();

	uint8_t lower  = memory[(uint16_t)(registers[SP]++)];
	uint8_t higher = memory[(uint16_t)(registers[SP]++)];

	mem.unlock();

	return higher << 8 | lower;
}