CXX = g++
CXXFLAGS = -std=c++14 -O2 -Wall -Wextra -Wpedantic

.PHONY: all test clean
all: code

code: main.cpp src/main.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

test: code
	python3 tests/test_simulator.py ./code

clean:
	rm -f code
