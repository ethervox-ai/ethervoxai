# Minimal Linux build config for mixed Python/Node projects
.PHONY: help setup-venv install-deps build test clean configure configure-rpi build-rpi

help:
	@echo "Usage: make <target>"
	@echo "Targets:"
	@echo "  configure       - Configure for native build"
	@echo "  configure-rpi   - Configure for Raspberry Pi cross-compilation"
	@echo "  build           - Build for native platform"
	@echo "  build-rpi       - Cross-compile for Raspberry Pi"
	@echo "  test            - Run tests"
	@echo "  clean           - Clean build artifacts"
	@echo "  clean-rpi       - Clean Raspberry Pi build artifacts"

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
	@cd build && cmake .. -DCMAKE_BUILD_TYPE=Release

configure-rpi:
	@echo "Configuring CMake for Raspberry Pi cross-compilation..."
	@mkdir -p build-rpi
	@cd build-rpi && cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE=../cmake/rpi-toolchain.cmake

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