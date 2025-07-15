# Compiler and flags
CXX = clang++ # Fixed to clang++
CXXFLAGS ?= -O3 -std=c++20 -Wall -Wextra -DNDEBUG -march=native -flto -fuse-ld=lld # Default compiler flags

# Automatically find all source files in the correct folder
SRC = $(wildcard Laminar/*.cpp)

# Generate object file names from source files
OBJ = $(SRC:.cpp=.o)

# Output binary (default)
EXE ?= Laminar.exe

RM := rm -f
RMDIR := "Laminar/"*.o
ifeq ($(OS),Windows_NT)
    RM := del /F /Q
    RMDIR := "Laminar\*.o"
endif

# Default target
all: $(EXE)

# Rule to build the executable
$(EXE): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Rule to build object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up build files
clean:
	$(RM) $(RMDIR)
	$(RM) "$(EXE)"

# Phony targets
.PHONY: all clean
