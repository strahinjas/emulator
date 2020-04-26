#ifndef _EMULATOR_H_
#define _EMULATOR_H_

#include <queue>
#include <bitset>
#include <mutex>

#include "types.h"
#include "instruction.h"

constexpr uint8_t  REGISTER_NUMBER = 8;

constexpr uint16_t STACK_START = 0xFF00;
constexpr uint16_t STACK_SIZE  = 1024;

constexpr uint16_t IVT_START = 0x000;
constexpr uint16_t IVT_SIZE  = 16;

constexpr auto START = "_start";

class Emulator {
public:
	~Emulator();

	void emulate(uint8_t* program);

	static void tick(Emulator* emulator);
	static void type(Emulator* emulator);

	static uint16_t periods[];
private:
	void fetch();

	void execute();
	int16_t getOperand(Instruction::CodedOperand* operand);
	void storeResult(int16_t result, Instruction::CodedOperand* operand);

	void interrupt();

	void push(int16_t value);
	int16_t pop();

	static const uint8_t SP = 6;
	static const uint8_t PC = 7;

	static const uint8_t Z  = 0;
	static const uint8_t O  = 1;
	static const uint8_t C  = 2;
	static const uint8_t N  = 3;
	static const uint8_t TR = 13;
	static const uint8_t TL = 14;
	static const uint8_t I  = 15;

	static const uint16_t DATA_OUT  = 0xFF00;
	static const uint16_t DATA_IN   = 0xFF02;

	static const uint16_t TIMER_CFG = 0xFF10;

	uint8_t* memory;

	std::mutex mtx;
	std::mutex mem;

	std::bitset<16> PSW;
	int16_t registers[REGISTER_NUMBER];

	bool running;
	bool instructionError;

	Instruction* instruction;

	std::queue<InterruptType> interruptRequests;
};

#endif