#!/bin/bash
# Build script for EthervoxAI cross-platform compilation
# Usage: ./scripts/build.sh [platform] [options]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_ROOT"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to show usage
show_usage() {
    echo "EthervoxAI Cross-Platform Build Script"
    echo "Usage: $0 [platform] [options]"
    echo ""
    echo "Platforms:"
    echo "  desktop         - Build for current desktop platform (default)"
    echo "  debug           - Build with debug symbols and coverage"
    echo "  esp32           - Build for original ESP32"
    echo "  esp32-s3        - Build for ESP32-S3 (default ESP32 variant)"
    echo "  esp32-c3        - Build for ESP32-C3 RISC-V"
    echo "  rpi             - Build for Raspberry Pi 4/5"
    echo "  rpi-zero        - Build for Raspberry Pi Zero"
    echo "  all-esp32       - Build for all ESP32 variants"
    echo "  all-rpi         - Build for all Raspberry Pi variants"
    echo "  all             - Build for all supported platforms"
    echo ""
    echo "Options:"
    echo "  --clean         - Clean build directories before building"
    echo "  --test          - Run tests after building (desktop/rpi only)"
    echo "  --verbose       - Enable verbose output"
    echo "  --help          - Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0                    # Build for desktop"
    echo "  $0 esp32-s3          # Build for ESP32-S3"
    echo "  $0 all-esp32 --clean # Clean build all ESP32 variants"
    echo "  $0 debug --test      # Debug build with tests"
}

# Parse command line arguments
PLATFORM="desktop"
CLEAN_BUILD=false
RUN_TESTS=false
VERBOSE=false

while [[ $# -gt 0 ]]; do
    case $1 in
        desktop|debug|esp32|esp32-s3|esp32-c3|rpi|rpi-zero|all-esp32|all-rpi|all)
            PLATFORM="$1"
            shift
            ;;
        --clean)
            CLEAN_BUILD=true
            shift
            ;;
        --test)
            RUN_TESTS=true
            shift
            ;;
        --verbose)
            VERBOSE=true
            shift
            ;;
        --help|-h)
            show_usage
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            show_usage
            exit 1
            ;;
    esac
done

# Function to build a specific preset
build_preset() {
    local preset="$1"
    local display_name="$2"
    
    print_status "Building $display_name..."
    
    # Configure
    if cmake --preset "$preset"; then
        print_success "Configuration completed for $display_name"
    else
        print_error "Configuration failed for $display_name"
        return 1
    fi
    
    # Build
    if cmake --build --preset "$preset"; then
        print_success "Build completed for $display_name"
    else
        print_error "Build failed for $display_name"
        return 1
    fi
    
    # Run tests if requested and supported
    if [[ "$RUN_TESTS" == true ]] && [[ "$preset" != esp32* ]]; then
        print_status "Running tests for $display_name..."
        if ctest --preset "$preset"; then
            print_success "Tests passed for $display_name"
        else
            print_warning "Some tests failed for $display_name"
        fi
    fi
}

# Clean build directories if requested
if [[ "$CLEAN_BUILD" == true ]]; then
    print_status "Cleaning build directories..."
    rm -rf build*
    print_success "Build directories cleaned"
fi

# Check for required tools
check_requirements() {
    if ! command -v cmake &> /dev/null; then
        print_error "CMake is required but not installed"
        exit 1
    fi
    
    if ! command -v ninja &> /dev/null; then
        print_warning "Ninja is recommended for faster builds"
    fi
}

check_requirements

# Main build logic
case $PLATFORM in
    desktop)
        build_preset "default" "Desktop (Release)"
        ;;
    debug)
        build_preset "debug" "Desktop (Debug)"
        ;;
    esp32)
        if [[ -z "$IDF_PATH" ]]; then
            print_error "ESP-IDF not found. Please run: source \$IDF_PATH/export.sh"
            exit 1
        fi
        build_preset "esp32-original" "ESP32 Original"
        ;;
    esp32-s3)
        if [[ -z "$IDF_PATH" ]]; then
            print_error "ESP-IDF not found. Please run: source \$IDF_PATH/export.sh"
            exit 1
        fi
        build_preset "esp32-s3" "ESP32-S3"
        ;;
    esp32-c3)
        if [[ -z "$IDF_PATH" ]]; then
            print_error "ESP-IDF not found. Please run: source \$IDF_PATH/export.sh"
            exit 1
        fi
        build_preset "esp32-c3" "ESP32-C3 RISC-V"
        ;;
    rpi)
        build_preset "rpi-4" "Raspberry Pi 4/5"
        ;;
    rpi-zero)
        build_preset "rpi-zero" "Raspberry Pi Zero"
        ;;
    all-esp32)
        if [[ -z "$IDF_PATH" ]]; then
            print_error "ESP-IDF not found. Please run: source \$IDF_PATH/export.sh"
            exit 1
        fi
        build_preset "esp32-original" "ESP32 Original"
        build_preset "esp32-s3" "ESP32-S3"
        build_preset "esp32-c3" "ESP32-C3 RISC-V"
        ;;
    all-rpi)
        build_preset "rpi-zero" "Raspberry Pi Zero"
        build_preset "rpi-4" "Raspberry Pi 4/5"
        ;;
    all)
        build_preset "default" "Desktop (Release)"
        
        if [[ -n "$IDF_PATH" ]]; then
            build_preset "esp32-original" "ESP32 Original"
            build_preset "esp32-s3" "ESP32-S3"
            build_preset "esp32-c3" "ESP32-C3 RISC-V"
        else
            print_warning "ESP-IDF not found, skipping ESP32 builds"
        fi
        
        build_preset "rpi-zero" "Raspberry Pi Zero"
        build_preset "rpi-4" "Raspberry Pi 4/5"
        ;;
esac

print_success "Build process completed!"