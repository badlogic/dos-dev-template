#!/bin/bash
touch src/*.c
touch src/*.cpp
cmake --build build --target main keyboard debugger
./tools/dosbox-x/dosbox-x -conf tools/dosbox-x.conf -fastlaunch build/keyboard.exe & sleep 1 && node examples/debugger-sim.js