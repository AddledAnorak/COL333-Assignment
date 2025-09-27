# Compiler
CXX := g++
CXXFLAGS := -Wall -std=c++17

# Source files
SRC := $(wildcard src/*.cpp) main.cpp
OBJ := $(SRC:.cpp=.o)

# Output binary
TARGET := main

# Default rule
all: $(TARGET)

# Link object files
$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Compile each .cpp into .o
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Run the program
run: $(TARGET)
	./$(TARGET)

# Clean build files
clean:
	rm -f $(OBJ) $(TARGET) massif.*
