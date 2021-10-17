#!/bin/bash
mkdir -p bin
FILES="lib/configureSndcard.c audioc/audioc.c audioc/audiocArgs.c"
FLAGS="-Wall -Wextra -lm -g -std=gnu99 -DAC_ASSERTS"
gcc $FLAGS $FILES -o bin/audioc