# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
LIBS = -lmingw32 -lSDL2main -lSDL2 -lm

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
INCLUDES = -I$(INCDIR)

# Default target
all: $(TARGET)

# Create object directory if it doesn't exist
$(OBJDIR):
	mkdir -p $(OBJDIR)

# Build target executable
$(TARGET): $(OBJDIR) $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LIBS)

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

# Install dependencies (MSYS2/MinGW64)
install-deps:
	pacman -S mingw-w64-x86_64-SDL2

# Run the program
run: $(TARGET)
	./$(TARGET)

# Show help
help:
	@echo "Available targets:"
	@echo "  all        - Build the program (default)"
	@echo "  clean      - Remove build files"
	@echo "  rebuild    - Clean and build"
	@echo "  debug      - Build with debug symbols"
	@echo "  run        - Build and run the program"
	@echo "  install-deps - Install SDL2 dependencies (MSYS2)"
	@echo "  help       - Show this help"

.PHONY: all clean rebuild debug install-deps run help