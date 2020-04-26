#!/bin/bash

echo array.s
../assembler/bin/assembler -o tests/array.o tests/array.s
echo print.s
../assembler/bin/assembler -o tests/print.o tests/print.s
echo digit_sum.s
../assembler/bin/assembler -o tests/digit_sum.o tests/digit_sum.s
echo sort.s
../assembler/bin/assembler -o tests/sort.o tests/sort.s
echo main.s
../assembler/bin/assembler -o tests/main.o tests/main.s
echo setup.s
../assembler/bin/assembler -o tests/setup.o tests/setup.s

echo Loading program...
echo Starting emulator...
bin/emulator -place=iv_table@0x0000 tests/array.o tests/print.o tests/digit_sum.o tests/sort.o tests/main.o tests/setup.o

echo Test program ended. 