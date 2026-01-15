# Makefile for thermo-cli
# MCC 134 Thermocouple Interface and Data Fuser

CC = gcc
CFLAGS = -Wall -Wextra -O2 -I./include -I./vendor
LDFLAGS = -ldaqhats -lyaml -lm

# Source files
SOURCES = src/main.c \
          src/commands/list.c \
          src/commands/get.c \
          src/commands/set.c \
          src/commands/init_config.c \
          src/hardware.c \
          src/config.c \
          src/bridge.c \
          src/utils.c \
          vendor/cJSON.c

OBJECTS = $(SOURCES:.c=.o)
TARGET = thermo-cli

# Build target
all: $(TARGET)

$(TARGET): $(OBJECTS)
	@echo "Linking $(TARGET)..."
	$(CC) -o $@ $^ $(LDFLAGS)
	@echo "Build complete: $(TARGET)"

# Compile source files
%.o: %.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	rm -f $(OBJECTS) $(TARGET)
	@echo "Clean complete"

# Install to system
install: $(TARGET)
	@echo "Installing $(TARGET) to /usr/local/bin/..."
	install -m 755 $(TARGET) /usr/local/bin/
	@echo "Installation complete"

# Uninstall from system
uninstall:
	@echo "Uninstalling $(TARGET) from /usr/local/bin/..."
	rm -f /usr/local/bin/$(TARGET)
	@echo "Uninstall complete"

# Test build (compile without linking)
test-compile:
	@echo "Testing compilation..."
	$(CC) $(CFLAGS) -c $(SOURCES)
	@echo "Compilation test passed"
	rm -f $(OBJECTS)

# Show help
help:
	@echo "Thermo CLI Build System"
	@echo ""
	@echo "Available targets:"
	@echo "  all           Build thermo-cli (default)"
	@echo "  clean         Remove build artifacts"
	@echo "  install       Install to /usr/local/bin (requires sudo)"
	@echo "  uninstall     Remove from /usr/local/bin (requires sudo)"
	@echo "  test-compile  Test compilation without linking"
	@echo "  help          Show this help message"
	@echo ""
	@echo "Usage examples:"
	@echo "  make                # Build the project"
	@echo "  make clean          # Clean build files"
	@echo "  sudo make install   # Install to system"

.PHONY: all clean install uninstall test-compile help
