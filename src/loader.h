#ifndef _LOADER_H_
#define _LOADER_H_

#include <string>
#include <vector>
#include <map>
#include <unordered_map>

#include "symbol.h"
#include "section.h"
#include "relocation.h"
#include "emulator.h"

class Loader {
public:
	uint8_t* load(const std::vector<std::string>& arguments);
private:
	class ObjectFile {
	public:
		void readELF(const std::string& file);

		std::unordered_map<std::string, Symbol>  symbols;
		std::unordered_map<std::string, Section> sections;

		std::vector<Relocation> relocations;
	};

	void linkFiles(const std::vector<std::string>& files);

	void loadIntoMemory(const std::map<uint16_t, std::string>& startAddresses);

	void addSymbol(Symbol& symbol);

	uint16_t addSection(Symbol& symbol, Section& section);

	std::unordered_map<std::string, Symbol>  symbols;
	std::unordered_map<std::string, Section> sections;

	std::vector<Relocation> relocations;
};

#endif