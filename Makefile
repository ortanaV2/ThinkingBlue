# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
LIBS = -lmingw32 -lSDL2main -lSDL2 -lm

# Python integration for MSYS2/MinGW64
# Auto-detect Python version
PYTHON_VERSION := $(shell python -c "import sys; print(f'{sys.version_info.major}.{sys.version_info.minor}')")
PYTHON_INCLUDE = -I/mingw64/include/python$(PYTHON_VERSION)
PYTHON_LIBS = -L/mingw64/lib -lpython$(PYTHON_VERSION)

# Directories
SRCDIR = src
INCDIR = include
OBJDIR = obj

# Target executable
TARGET = main.exe

# Source files
SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

# Include path
INCLUDES = -I$(INCDIR) $(PYTHON_INCLUDE)

# Updated libs with Python
ALL_LIBS = $(LIBS) $(PYTHON_LIBS)

# Default target
all: check-python $(TARGET)

# Check if Python is properly installed
check-python:
	@echo "Checking Python installation..."
	@python --version || (echo "Python not found! Install with: pacman -S mingw-w64-x86_64-python" && exit 1)
	@echo "Python version: $(PYTHON_VERSION)"
	@test -f /mingw64/include/python$(PYTHON_VERSION)/Python.h || (echo "Python headers not found! Install with: pacman -S mingw-w64-x86_64-python-devel" && exit 1)
	@echo "Python headers found"

# Create object directory if it doesn't exist
$(OBJDIR):
	mkdir -p $(OBJDIR)

# Build target executable
$(TARGET): $(OBJDIR) $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(ALL_LIBS)

# Compile source files to object files
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Clean build files
clean:
	rm -rf $(OBJDIR) $(TARGET)

# Rebuild everything
rebuild: clean all

# Debug build with debug symbols
debug: CFLAGS += -g -DDEBUG
debug: $(TARGET)

# Install all dependencies (MSYS2/MinGW64)
install-deps:
	pacman -S --needed mingw-w64-x86_64-SDL2 mingw-w64-x86_64-python mingw-w64-x86_64-python-pip mingw-w64-x86_64-python-devel

# Run the program
run: $(TARGET)
	./$(TARGET)

# Show help
help:
	@echo "Available targets:"
	@echo "  all          - Build the program (default)"
	@echo "  clean        - Remove build files"
	@echo "  rebuild      - Clean and build"
	@echo "  debug        - Build with debug symbols"
	@echo "  run          - Build and run the program"
	@echo "  install-deps - Install all dependencies (MSYS2)"
	@echo "  check-python - Check Python installation"
	@echo "  help         - Show this help"

.PHONY: all clean rebuild debug install-deps run help check-python