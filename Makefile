# Makefile for EthervoxAI
# Supports native build, Raspberry Pi cross-compilation, Windows cross-compilation, and ESP32
# Usage: make help for available targets
	
.PHONY: help setup-venv install-deps build test clean
.PHONY: configure configure-rpi configure-windows configure-all
.PHONY: build-rpi build-windows build-esp32 build-voice-assistant build-all
.PHONY: clean-rpi clean-windows clean-esp32 clean-voice-assistant clean-all
.PHONY: setup-esp32 flash-esp32 monitor-esp32
.PHONY: run-voice-assistant

help:
	@echo "=========================================="
	@echo "  EthervoxAI Build System"
	@echo "=========================================="
	@echo ""
	@echo "Usage: make <target>"
	@echo ""
	@echo "General Targets:"
	@echo "  help            - Show this help message"
	@echo "  install-deps    - Install all dependencies"
	@echo "  setup-venv      - Set up Python virtual environment"
	@echo "  clean           - Clean native build artifacts"
	@echo "  clean-all       - Clean ALL platform build artifacts"
	@echo ""
	@echo "Multi-Platform Targets:"
	@echo "  configure-all   - Configure for all platforms"
	@echo "  build-all       - Build for all platforms"
	@echo ""
	@echo "Linux/Desktop Build:"
	@echo "  configure       - Configure for native Linux build"
	@echo "  build           - Build for native platform"
	@echo "  test            - Run unit tests"
	@echo ""
	@echo "Raspberry Pi Cross-Compilation:"
	@echo "  configure-rpi   - Configure for Raspberry Pi"
	@echo "  build-rpi       - Cross-compile for Raspberry Pi"
	@echo "  clean-rpi       - Clean Raspberry Pi build artifacts"
	@echo ""
	@echo "Windows Cross-Compilation:"
	@echo "  configure-windows - Configure for Windows"
	@echo "  build-windows   - Cross-compile for Windows"
	@echo "  clean-windows   - Clean Windows build artifacts"
	@echo ""
	@echo "ESP32 Development:"
	@echo "  setup-esp32     - Set up ESP32 toolchain (one-time)"
	@echo "  build-esp32     - Build for ESP32"
	@echo "  flash-esp32     - Flash ESP32 device (ESP_PORT=/dev/ttyUSB0)"
	@echo "  monitor-esp32   - Open serial monitor"
	@echo "  clean-esp32     - Clean ESP32 build artifacts"
	@echo ""
	@echo "Examples:"
	@echo "  make build-all              # Build for all platforms"
	@echo "  make build-rpi              # Build for Raspberry Pi only"
	@echo "  make clean-all              # Clean all builds"
	@echo "  make flash-esp32 ESP_PORT=/dev/ttyUSB1"
	@echo "=========================================="

# Default ESP32 port (override with: make flash-esp32 ESP_PORT=/dev/ttyUSB1)
ESP_PORT ?= /dev/ttyUSB0

setup-venv:
	@test -d .venv || python3 -m venv .venv
	@. .venv/bin/activate && pip install -U pip setuptools wheel

install-deps: setup-venv
	@if [ -f requirements.txt ]; then \
    	. .venv/bin/activate && pip install -r requirements.txt; \
	elif [ -f pyproject.toml ]; then \
    	. .venv/bin/activate && pip install -e .; \
	fi
	@if [ -f package.json ]; then \
    	echo "Installing root dependencies..."; \
    	npm install; \
    fi
	@if [ -d dashboard ] && [ -f dashboard/package.json ]; then \
    	echo "Installing dashboard dependencies..."; \
    	cd dashboard && npm install; \
    fi

configure:
	@echo "Configuring CMake build..."
	@mkdir -p build
	cmake -B build -S . -DTARGET_PLATFORM=$(or $(TARGET_PLATFORM),LINUX)

configure-rpi:
	@echo "Configuring CMake for Raspberry Pi cross-compilation..."
	@mkdir -p build-rpi
	cmake -B build-rpi -S . \
        -DTARGET_PLATFORM=RPI \
        -DCMAKE_TOOLCHAIN_FILE=cmake/rpi-toolchain.cmake

configure-windows:
	@echo "Configuring CMake for Windows cross-compilation..."
	@mkdir -p build-windows
	cmake -B build-windows -S . \
        -DTARGET_PLATFORM=WINDOWS \
        -DCMAKE_TOOLCHAIN_FILE=cmake/windows-toolchain.cmake

build-windows: install-deps configure-windows
	@echo "Building for Windows..."
	cmake --build build-windows

clean-windows:
	@echo "Cleaning Windows build..."
	rm -rf build-windows

build-rpi: install-deps configure-rpi
	@echo "Cross-compiling for Raspberry Pi..."
	@cd build-rpi && $(MAKE)
	@echo "Raspberry Pi build complete! Binary: build-rpi/ethervoxai"

clean-rpi:
	@echo "Cleaning Raspberry Pi build artifacts..."
	@rm -rf build-rpi

build: install-deps configure
	@echo "Installing dependencies for build"
	@if [ -f setup.py ]; then \
	  . .venv/bin/activate && python setup.py build; \
	elif [ -f pyproject.toml ]; then \
	  . .venv/bin/activate && pip wheel . -w dist || true; \
	elif [ -f package.json ]; then \
	  npm run build || true; \
	else \
	  echo "No recognized build manifest found."; exit 1; \
	fi

test: install-deps
	@echo "Installing dependencies for tests..."
	@. .venv/bin/activate && if command -v pytest >/dev/null 2>&1; then pytest -q; else echo "pytest not installed; skipping tests"; fi

clean:
	@echo "Cleaning build artifacts..."
	@rm -rf .venv build build-rpi dist *.egg-info node_modules

setup-esp32:
	@./scripts/setup-esp32-toolchain.sh
	@mkdir -p esp32-project/main
	@[ -L esp32-project/src ] || ln -s ../src esp32-project/src
	@[ -L esp32-project/include ] || ln -s ../include esp32-project/include

build-esp32:
	@echo "Building for ESP32..."
	@cd esp32-project && . $(HOME)/esp/esp-idf/export.sh && idf.py build

flash-esp32:
	@echo "Flashing ESP32..."
	@cd esp32-project && . $(HOME)/esp/esp-idf/export.sh && idf.py -p $(ESP_PORT) flash

monitor-esp32:
	@echo "Opening serial monitor..."
	@cd esp32-project && . $(HOME)/esp/esp-idf/export.sh && idf.py -p $(ESP_PORT) monitor

clean-esp32:
	@echo "Cleaning ESP32 build..."
	@if [ -d "esp32-project/build" ]; then \
		cd esp32-project && . $(HOME)/esp/esp-idf/export.sh && idf.py fullclean 2>/dev/null || rm -rf build; \
	else \
		echo "No ESP32 build directory to clean"; \
	fi
	@rm -rf esp32-project/sdkconfig esp32-project/sdkconfig.old

build-voice-assistant: $(TARGET)
	@echo "Building voice assistant example..."
	@$(MAKE) -C examples/voice_assistant

run-voice-assistant: build-voice-assistant
	@echo "Running voice assistant..."
	@$(MAKE) -C examples/voice_assistant run

clean-voice-assistant:
	@echo "Cleaning voice assistant example..."
	@$(MAKE) -C examples/voice_assistant clean

# Multi-platform targets
configure-all: configure configure-rpi configure-windows
	@echo "All platforms configured!"

build-all: build build-rpi build-windows build-esp32 build-voice-assistant
	@echo "=========================================="
	@echo "  All Platform Builds Complete!"
	@echo "=========================================="
	@echo "Linux binary:   build/ethervoxai"
	@echo "RPI binary:     build-rpi/ethervoxai"
	@echo "Windows binary: build-windows/ethervoxai.exe"
	@echo "ESP32 binary:   build/ethervoxai.bin"
	@echo "Voice Assistant Example: examples/voice_assistant/voice_assistant"
	@echo "=========================================="

clean-all: clean clean-rpi clean-windows clean-esp32 clean-voice-assistant
	@echo "All build artifacts cleaned!"
