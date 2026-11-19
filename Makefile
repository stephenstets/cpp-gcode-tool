CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra

all: gcode-tool

gcode-tool: gcode-tool.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

test: gcode-tool
	./gcode-tool sample.gcode

clean:
	rm -f gcode-tool

.PHONY: all test clean
