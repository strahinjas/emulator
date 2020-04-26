#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <regex>
#include <cstdlib>
#include <fstream>
#include <unordered_set>

#include "loader.h"
#include "symbol.h"
#include "section.h"
#include "relocation.h"
#include "exceptions.h"
#include "types.h"
#include "emulator.h"

using namespace std;


void Loader::ObjectFile::readELF(const std::string& file) {
	if (!std::regex_search(file, objectFile))
		throw LinkingException("Invalid input file type -> object file (.o) expected!");

	ifstream input(file, ifstream::in | ifstream::binary);

	if (!input.is_open())
		throw LinkingException("Can't open file " + file + "!");

	size_t size;
	input.read((char*)& size, sizeof(size_t));

	for (int i = 0; i < size; i++) {
		Symbol symbol;
		symbol.deserialize(input);

		symbols.insert({ symbol.name, symbol });
	}

	input.read((char*)& size, sizeof(size_t));

	for (int i = 0; i < size; i++) {
		Section section;
		section.deserialize(input);

		sections.insert({ section.name, section });
	}

	input.read((char*)& size, sizeof(size_t));

	for (int i = 0; i < size; i++) {
		Relocation relocation;
		relocation.deserialize(input);

		relocations.push_back(relocation);
	}
}


uint8_t* Loader::load(const vector<string>& arguments) {
	const regex option("^-place=([\\.\\w]+)@([0-9]+|0x[0-9abcdefABCDEF]+)$");

	vector<string> files;
	map<uint16_t, string> startAddresses;

	for (const auto& argument : arguments) {
		smatch matches;

		if (regex_match(argument, matches, option)) {
			string section = matches[1];
			uint16_t address = strtol(matches[2].str().c_str(), NULL, 0);

			startAddresses.insert({ address, section });
		}
		else
			files.push_back(argument);
	}
	
	if (files.empty())
		throw LinkingException("You should supply at least one object file!");

	linkFiles(files);

	loadIntoMemory(startAddresses);

	uint8_t* program = new uint8_t[UINT16_MAX + 1]();

	for (const auto& section : sections) {
		const vector<uint8_t>& bytes = section.second.bytes;
		uint16_t start = symbols[section.first].value;

		for (int i = 0; i < bytes.size(); i++)
			program[start + i] = bytes[i];
	}

	return program;
}


void Loader::linkFiles(const vector<string>& files) {
	for (const string& file : files) {
		ObjectFile objectFile;

		objectFile.readELF(file);

		unordered_map<string, uint16_t> offset;

		for (auto& symbol : objectFile.symbols) {
			if (symbol.second.type == SymbolType::SECTION) {
				Section& section = objectFile.sections[symbol.first];
				offset[symbol.first] = addSection(symbol.second, section);
			}
		}

		for (auto& symbol : objectFile.symbols) {
			if (symbol.second.type != SymbolType::SECTION &&
				symbol.second.scope == GLOBAL) {

				if (symbol.second.type != SymbolType::CONSTANT &&
					symbol.second.defined)
					symbol.second.value += offset[symbol.second.section];

				addSymbol(symbol.second);
			}
		}

		for (auto& relocation : objectFile.relocations) {
			Symbol& symbol = objectFile.symbols[relocation.symbol];

			relocation.offset += offset[relocation.section];

			if (symbol.type == SymbolType::SECTION) {
				Section& section = sections[relocation.section];

				uint16_t value = offset[symbol.name];
				if (relocation.type == R_386_8 || relocation.type == R_386_SUB_8) {
					section.bytes[relocation.offset] += value;
				}
				else {
					uint8_t lower  = section.bytes[relocation.offset];
					uint8_t higher = section.bytes[relocation.offset + 1];

					value += lower | (higher << 8);

					lower  =  value & 0x00FF;
					higher = (value & 0xFF00) >> 8;

					section.bytes[relocation.offset] = lower;
					section.bytes[relocation.offset + 1] = higher;
				}
			}

			relocations.push_back(relocation);
		}
	}

	bool startFound = false;

	for (const auto& symbol : symbols) {
		if (!symbol.second.defined)
			throw LinkingException("Symbol \"" + symbol.first + "\" remained unresolved!");

		if (symbol.second.type == SymbolType::LABEL && symbol.second.name == START)
			startFound = true;
	}

	if (!startFound)
		throw LinkingException("Starting entry point is never defined!");
}


void Loader::loadIntoMemory(const map<uint16_t, string>& startAddresses) {
	for (auto& section : sections)
		if (section.second.flags[A] == '0')
			section.second.writeValue(0, section.second.size, 0);

	const uint16_t MAX = STACK_START - STACK_SIZE - 1;

	if (!sections.count("iv_table"))
		throw LinkingException("Missing iv_table section!");

	uint32_t last = 0;
	unordered_set<string> positioned;

	bool IVT = false;
	for (const auto& address : startAddresses)
		if (address.second == "iv_table") {
			IVT = true;
			break;
		}

	if (!IVT) {
		Symbol& symbol = symbols["iv_table"];

		symbol.value = IVT_START;
		last = IVT_SIZE;

		positioned.insert("iv_table");
	}

	for (const auto& address : startAddresses) {
		if (sections.count(address.second)) {
			Symbol& symbol = symbols[address.second];
			Section& section = sections[address.second];

			symbol.value = address.first;

			if (address.first < last)
				throw LinkingException("Overlapping sections!");

			last = address.first + section.size;

			if (last > MAX)
				throw LinkingException("Out of memory address space!");

			positioned.insert(section.name);
		}
	}

	for (const auto& section : sections) {
		if (!positioned.count(section.first)) {
			Symbol& symbol = symbols[section.first];

			symbol.value = last;
			last += section.second.size;

			if (last > MAX)
				throw LinkingException("Out of memory address space!");
		}
	}

	for (auto& symbol : symbols) {
		if (symbol.second.type == SymbolType::LABEL ||
			symbol.second.type == SymbolType::ALIAS) {

			Symbol& section = symbols[symbol.second.section];

			symbol.second.value += section.value;
		}
	}

	for (const auto& relocation : relocations) {
		Symbol& symbol = symbols[relocation.symbol];
		Symbol& sectionSymbol = symbols[relocation.section];
		Section& section = sections[relocation.section];

		switch (relocation.type) {
		case R_386_8:
		case R_386_SUB_8: {
			uint8_t value = symbol.value;

			if (relocation.type == R_386_8)
				section.bytes[relocation.offset] += value;
			else
				section.bytes[relocation.offset] -= value;

			break;
		}
		case R_386_16:
		case R_386_SUB_16: {
			uint8_t lower  = section.bytes[relocation.offset];
			uint8_t higher = section.bytes[relocation.offset + 1];

			uint16_t value = lower | (higher << 8);

			if (relocation.type == R_386_16)
				value += symbol.value;
			else
				value -= symbol.value;

			lower  =  value & 0x00FF;
			higher = (value & 0xFF00) >> 8;

			section.bytes[relocation.offset] = lower;
			section.bytes[relocation.offset + 1] = higher;

			break;
		}
		case R_386_PC16:
		case R_386_SUB_PC16: {
			uint16_t value = symbol.value - sectionSymbol.value - relocation.offset;

			uint8_t lower  = section.bytes[relocation.offset];
			uint8_t higher = section.bytes[relocation.offset + 1];

			value += lower | (higher << 8);

			lower  =  value & 0x00FF;
			higher = (value & 0xFF00) >> 8;

			section.bytes[relocation.offset] = lower;
			section.bytes[relocation.offset + 1] = higher;

			break;
		}
		default:
			throw LinkingException("Unexpected error!");
			break;
		}
	}
}


void Loader::addSymbol(Symbol& symbol) {
	if (symbols.count(symbol.name)) {
		Symbol& alias = symbols[symbol.name];

		if (symbol.defined) {
			if (alias.defined)
				throw LinkingException("Symbol \"" + symbol.name + "\" has multiple definitions!");

			alias.section = symbol.section;
			alias.value = symbol.value;
			alias.type = symbol.type;
			alias.defined = symbol.defined;
		}
	}
	else {
		symbols.insert({ symbol.name, symbol });
	}
}

uint16_t Loader::addSection(Symbol& symbol, Section& section) {
	uint16_t offset;

	if (sections.count(section.name)) {
		Section& linked = sections[section.name];

		offset = linked.size;
		linked.size += section.size;

		if (!section.bytes.empty())
			linked.write(offset, section.bytes);
	}
	else {
		offset = 0;

		symbols.insert({ symbol.name, symbol });
		sections.insert({ section.name, section });
	}

	return offset;
}