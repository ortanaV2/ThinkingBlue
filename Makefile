# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
LIBS = -lmingw32 -lSDL2main -lSDL2 -lm

# Python integration for MSYS2/MinGW64 - WORKING VERSION
PYTHON_VERSION = 3.12
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

# Simplified Python check that works
check-python:
	@echo "Checking Python installation..."
	@python --version
	@echo "Testing Python modules..."
	@python -c "import math, random; print('✓ Basic modules work')"
	@echo "Checking Python headers..."
	@test -f /mingw64/include/python$(PYTHON_VERSION)/Python.h && echo "✓ Python headers found" || echo "⚠ Python headers missing"

# Create object directory if it doesn't exist
$(OBJDIR):
	@mkdir -p $(OBJDIR)

# Build target executable
$(TARGET): $(OBJDIR) $(OBJECTS)
	@echo "Linking executable..."
	$(CC) $(OBJECTS) -o $(TARGET) $(ALL_LIBS)
	@echo "✓ Build complete: $(TARGET)"

# Compile source files to object files
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Clean build files
clean:
	@echo "Cleaning build files..."
	rm -rf $(OBJDIR) $(TARGET)

# Rebuild everything
rebuild: clean all

# Debug build with debug symbols
debug: CFLAGS += -g -DDEBUG
debug: $(TARGET)

# Install Python development headers (correct package name)
install-python-headers:
	@echo "Installing Python development headers..."
	@if ! test -f /mingw64/include/python$(PYTHON_VERSION)/Python.h; then \
		echo "Python headers not found, checking available packages..."; \
		pacman -Ss python | grep devel || echo "No python-devel package found"; \
		echo "Trying alternative installation..."; \
		pacman -S --needed mingw-w64-x86_64-python; \
	else \
		echo "✓ Python headers already installed"; \
	fi

# Install all dependencies
install-deps:
	@echo "Installing dependencies for MSYS2/MinGW64..."
	pacman -S --needed --noconfirm \
		mingw-w64-x86_64-SDL2 \
		mingw-w64-x86_64-python \
		mingw-w64-x86_64-python-pip

# Build without Python check (if having issues)
build-no-check: $(TARGET)

# Run the program
run: $(TARGET)
	@echo "Starting $(TARGET)..."
	./$(TARGET)

# NEW: Test ecosystem stats independently
test-stats:
	@echo "Testing ecosystem statistics monitor..."
	@if [ -f ecosystem_stats.py ]; then \
		python ecosystem_stats.py; \
	else \
		echo "Error: ecosystem_stats.py not found"; \
	fi

# NEW: Check Python dependencies
check-stats-deps:
	@echo "Checking ecosystem statistics dependencies..."
	@python -c "import tkinter; print('✓ tkinter available')" || echo "✗ tkinter missing"
	@python -c "import threading; print('✓ threading available')" || echo "✗ threading missing"
	@python -c "import time; print('✓ time available')" || echo "✗ time missing"
	@python -c "import struct; print('✓ struct available')" || echo "✗ struct missing"
	@python -c "import os; print('✓ os available')" || echo "✗ os missing"
	@python -c "import collections; print('✓ collections available')" || echo "✗ collections missing"
	@echo "All dependencies use Python standard library only"

# Show help
help:
	@echo "Available targets:"
	@echo "  all              - Build with Python check (default)"
	@echo "  build-no-check   - Build without Python check"
	@echo "  clean            - Remove build files"
	@echo "  rebuild          - Clean and build"
	@echo "  debug            - Build with debug symbols"
	@echo "  run              - Build and run the program"
	@echo "  test-stats       - Test ecosystem statistics monitor"
	@echo "  check-stats-deps - Check statistics dependencies"
	@echo "  install-deps     - Install dependencies"
	@echo "  install-python-headers - Install Python development files"
	@echo "  help             - Show this help"
	@echo ""
	@echo "Live Statistics:"
	@echo "  - Press 'TAB' in simulation to open statistics window"
	@echo "  - Press 'Shift+TAB' to toggle plant/fish spawn mode"
	@echo "  - Shows live plots: nutrition balance, fish count, plant count"
	@echo "  - Updates every second automatically"
	@echo "  - Uses shared file communication (simulation_stats.tmp)"
	@echo "  - Single ecosystem_stats.py script handles everything"

.PHONY: all build-no-check clean rebuild debug install-deps run test-stats check-stats-deps help check-python install-python-headers