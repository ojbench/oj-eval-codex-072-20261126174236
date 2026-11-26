CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -Wpedantic

.PHONY: all test clean
all: code

code: src/main.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

test: code
	python3 tests/test_simulator.py ./code

clean:
	rm -f code
