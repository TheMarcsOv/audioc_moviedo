#!/bin/bash
mkdir -p bin
FILES="lib/*.c audioc/*.c"
FLAGS="-Wall -Wextra -std=gnu99"
FLAGS="$FLAGS -ggdb -O0"
FLAGS="$FLAGS -fsanitize=address -fno-omit-frame-pointer -fsanitize=undefined"
gcc $FLAGS $FILES -o bin/audioc -lm