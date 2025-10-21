# Compiler and flags
CXX := g++
CXXFLAGS := -std=c++17 -Wall -O2

# Target binary
TARGET := hsnw-test

# Directories
SRC_DIR := src
OBJ_DIR := obj

# Source, object, and header files
SRCS := $(SRC_DIR)/HSNW.cpp $(SRC_DIR)/AriTest.cpp
OBJS := $(SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
HDRS := $(SRC_DIR)/HNSW.h

# Default target
all:
	@echo "No default build target yet."

# Link object files into the final executable
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(TARGET)

# Compile each .cpp into .o (auto-make obj directory if needed)
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(HDRS)
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up build artifacts
clean:
	rm -rf $(OBJ_DIR) $(TARGET)
