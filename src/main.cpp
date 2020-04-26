#include <string>
#include <vector>
#include <iostream>
#include <exception>

#include "loader.h"
#include "emulator.h"

int main(int argc, char* argv[]) {
	if (argc < 2) {
		std::cerr << "ERROR: You should supply at least one object file!\n";
		std::cout << "Program should be called as: emulator [-place=<section>@<address>] input_file [input_file]\n\n";
		return 1;
	}

	std::vector<std::string> arguments;
	
	for (int i = 1; i < argc; i++)
		arguments.push_back(argv[i]);

	try {
		Loader loader;
		uint8_t* program = loader.load(arguments);

		Emulator emulator;
		emulator.emulate(program);
	}
	catch (const std::exception& e) {
		std::cerr << e.what() << std::endl << std::endl;
		return 1;
	}

	return 0;
}