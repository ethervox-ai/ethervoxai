# EtherVox SDK Makefile
# Build examples for the EtherVox SDK

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -g
INCLUDES = -I..
LDFLAGS = -lm

# Source files
SDK_SRC = ../ethervox_sdk.c
EXAMPLES = intent_plugin_example model_router_example device_profile_example

# Default target
all: $(EXAMPLES)

# Intent plugin example
intent_plugin_example: intent_plugin_example.c $(SDK_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS)

# Model router example  
model_router_example: model_router_example.c $(SDK_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS)

# Device profile example
device_profile_example: device_profile_example.c $(SDK_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS)

# Clean build artifacts
clean:
	rm -f $(EXAMPLES) *.o *.conf

# Install examples to system
install: all
	mkdir -p /usr/local/share/ethervox/examples
	cp $(EXAMPLES) /usr/local/share/ethervox/examples/
	cp *.c /usr/local/share/ethervox/examples/
	cp README.md /usr/local/share/ethervox/examples/

# Run all examples
test: all
	@echo "Running Intent Plugin Example..."
	./intent_plugin_example
	@echo "\nRunning Model Router Example..."
	./model_router_example  
	@echo "\nRunning Device Profile Example..."
	./device_profile_example

# Cross-compilation targets
esp32: CC = xtensa-esp32-elf-gcc
esp32: CFLAGS += -DETHERVOX_PLATFORM_ESP32
esp32: all

rpi: CC = arm-linux-gnueabihf-gcc
rpi: CFLAGS += -DETHERVOX_PLATFORM_RPI
rpi: all

.PHONY: all clean install test esp32 rpi