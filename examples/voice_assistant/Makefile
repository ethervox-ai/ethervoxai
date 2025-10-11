# Voice Assistant Example Makefile

CC = gcc
CFLAGS = -Wall -Wextra -I../../include -std=c11 -g
LDFLAGS = -L../../build -lethervoxai -lm -lpthread

# Platform-specific audio libraries
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    LDFLAGS += -lportaudio -framework CoreAudio -framework AudioToolbox -framework AudioUnit -framework CoreFoundation
else ifeq ($(UNAME_S),Linux)
    LDFLAGS += -lportaudio -lasound
endif

# Source files
SRCS = main.c

# Object files
OBJS = $(SRCS:.c=.o)

# Output binary
TARGET = voice_assistant

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
    $(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.c
    $(CC) $(CFLAGS) -c $< -o $@

clean:
    rm -f $(OBJS) $(TARGET)

run: $(TARGET)
    ./$(TARGET)

.PHONY: help
help:
    @echo "Voice Assistant Example"
    @echo ""
    @echo "Targets:"
    @echo "  all     - Build the voice assistant"
    @echo "  clean   - Remove build artifacts"
    @echo "  run     - Build and run the assistant"
    @echo ""
    @echo "Usage:"
    @echo "  make"
    @echo "  make run"