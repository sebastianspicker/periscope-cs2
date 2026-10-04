# Makefile — T4 PCIe DMA firmware example
#
# Targets:
#   all          Build host_sim
#   sim          Build host_sim only
#   run          Build + run host_sim
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

SIM_OBJ   = host_sim.o page_table_walker.o cs2_dma_reader.o

SIM_TARGET   = host_sim

.PHONY: all sim run clean

all: sim

sim: $(SIM_TARGET)

host_sim.o: host_sim.cpp fpga_dma.h obf.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ host_sim.cpp

page_table_walker.o: page_table_walker.cpp fpga_dma.h
	$(CXX) $(CXXFLAGS) -c -o $@ page_table_walker.cpp

cs2_dma_reader.o: cs2_dma_reader.cpp fpga_dma.h
	$(CXX) $(CXXFLAGS) -c -o $@ cs2_dma_reader.cpp

$(SIM_TARGET): $(SIM_OBJ)
	$(CXX) $(LDFLAGS) -o $@ $(SIM_OBJ)

run: $(SIM_TARGET)
	./$(SIM_TARGET)

clean:
	rm -f $(SIM_OBJ) $(SIM_TARGET)
