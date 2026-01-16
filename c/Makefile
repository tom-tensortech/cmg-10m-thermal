# Makefile for thermo-cli
# MCC 134 Thermocouple Interface and Data Fuser

CC = gcc
CFLAGS = -Wall -Wextra -I./include -I./vendor
LDFLAGS = -ldaqhats -lyaml -lm

# Dependency generation flags
DEPFLAGS = -MMD -MP

# Debug mode: make DEBUG=1
ifeq ($(DEBUG),1)
    CFLAGS += -g -DDEBUG -O0
    # Uncomment for memory debugging (requires libasan):
    # CFLAGS += -fsanitize=address -fsanitize=undefined
    # LDFLAGS += -fsanitize=address -fsanitize=undefined
    $(info Building in DEBUG mode)
else
    CFLAGS += -O2
endif

# Add dependency flags to CFLAGS
CFLAGS += $(DEPFLAGS)

# Source files
SOURCES = src/main.c \
          src/commands/list.c \
          src/commands/get.c \
          src/commands/set.c \
          src/commands/init_config.c \
          src/hardware.c \
          src/common.c \
          src/bridge.c \
          src/board_manager.c \
          src/json_utils.c \
          src/utils.c \
          src/signals.c \
          vendor/cJSON.c

OBJECTS = $(SOURCES:.c=.o)
DEPS = $(OBJECTS:.o=.d)
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

# Include generated dependency files
-include $(DEPS)

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	rm -f $(OBJECTS) $(DEPS) $(TARGET)
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
	rm -f $(OBJECTS) $(DEPS)

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
	@echo "Build options:"
	@echo "  DEBUG=1       Build with debug symbols and enable DEBUG_PRINT"
	@echo "                (Optionally enable sanitizers by editing Makefile)"
	@echo ""
	@echo "Usage examples:"
	@echo "  make                # Build the project (release)"
	@echo "  make DEBUG=1        # Build with debug mode"
	@echo "  make clean          # Clean build files"
	@echo "  sudo make install   # Install to system"

.PHONY: all clean install uninstall test-compile help
