#!/bin/bash
./build.sh
bin/audioc 239.0.1.1 $1 -k300 -y0 -c
