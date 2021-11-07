#!/bin/bash
mkdir -p bin
FILES="lib/*.c audioc/audioc.c audioc/common.c audioc/audiocArgs.c"
FLAGS="-Wall -Wextra -lm -std=gnu99"
FLAGS="$FLAGS -g -O0 -DAC_DEBUG -DAC_ASSERTS"
FLAGS="$FLAGS -fsanitize=address"
FLAGS="$FLAGS -fno-omit-frame-pointer -fsanitize=undefined"
gcc $FLAGS $FILES -o bin/audioc
