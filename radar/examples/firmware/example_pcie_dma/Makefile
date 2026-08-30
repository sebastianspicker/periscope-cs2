# Makefile — T4 PCIe DMA firmware example
#
# Targets:
#   all          Build host_sim + dma_tests
#   sim          Build host_sim only
#   tests        Build dma_tests only
#   run          Build + run host_sim
#   check        Build + run dma_tests
#   clean        Remove build artifacts
#
# Builders:
#   make CXX=clang++          (default)
#   make CXX=g++
#   MSVC: build via CMake (see CMakeLists.txt)

CXX      ?= clang++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wpedantic -Wconversion -Wshadow
LDFLAGS  ?=

SIM_SRC   = host_sim.cpp page_table_walker.cpp cs2_dma_reader.cpp
TEST_SRC  = tests/dma_tests.cpp page_table_walker.cpp cs2_dma_reader.cpp

SIM_OBJ   = host_sim.o page_table_walker.o cs2_dma_reader.o
TEST_OBJ  = dma_tests.o page_table_walker.o cs2_dma_reader.o

SIM_TARGET   = host_sim
TEST_TARGET  = dma_tests

.PHONY: all sim tests run check clean

all: sim tests

sim: $(SIM_TARGET)

tests: $(TEST_TARGET)

# Separate object dirs so sim + test objects don't collide.
host_sim.o: host_sim.cpp fpga_dma.h obf.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ host_sim.cpp

dma_tests.o: tests/dma_tests.cpp fpga_dma.h
	$(CXX) $(CXXFLAGS) -c -o $@ tests/dma_tests.cpp

page_table_walker.o: page_table_walker.cpp fpga_dma.h
	$(CXX) $(CXXFLAGS) -c -o $@ page_table_walker.cpp

cs2_dma_reader.o: cs2_dma_reader.cpp fpga_dma.h
	$(CXX) $(CXXFLAGS) -c -o $@ cs2_dma_reader.cpp

$(SIM_TARGET): $(SIM_OBJ)
	$(CXX) $(LDFLAGS) -o $@ $(SIM_OBJ)

$(TEST_TARGET): $(TEST_OBJ)
	$(CXX) $(LDFLAGS) -o $@ $(TEST_OBJ)

run: $(SIM_TARGET)
	./$(SIM_TARGET)

check: $(TEST_TARGET)
	./$(TEST_TARGET)

clean:
	rm -f $(SIM_OBJ) $(TEST_OBJ) $(SIM_TARGET) $(TEST_TARGET)
