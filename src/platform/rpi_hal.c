#ifdef ETHERVOX_PLATFORM_RPI

#include "ethervox/platform.h"
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <wiringPiSPI.h>
#include <unistd.h>
#include <time.h>
#include <sys/sysinfo.h>
#include <stdio.h>
#include <stdlib.h>

// Raspberry Pi-specific HAL implementation
static int wiringpi_initialized = 0;
static int i2c_handle = -1;

static int rpi_init(ethervox_platform_info_t* info) {
    printf("Initializing Raspberry Pi platform\n");
    
    // Initialize WiringPi
    if (wiringPiSetupGpio() == -1) {
        printf("Failed to initialize WiringPi\n");
        return -1;
    }
    wiringpi_initialized = 1;
    
    // Initialize I2C (device 1 is typically available)
    i2c_handle = wiringPiI2CSetup(0x48);  // Default I2C device address
    if (i2c_handle < 0) {
        printf("Warning: I2C initialization failed\n");
    }
    
    // Initialize SPI (channel 0, speed 1MHz)
    if (wiringPiSPISetup(0, 1000000) < 0) {
        printf("Warning: SPI initialization failed\n");
    }
    
    return 0;
}

static void rpi_cleanup(ethervox_platform_info_t* info) {
    printf("Cleaning up Raspberry Pi platform\n");
    wiringpi_initialized = 0;
}

static int rpi_gpio_configure(uint32_t pin, ethervox_gpio_mode_t mode) {
    if (!wiringpi_initialized) return -1;
    
    switch (mode) {
        case ETHERVOX_GPIO_INPUT:
            pinMode(pin, INPUT);
            break;
        case ETHERVOX_GPIO_OUTPUT:
            pinMode(pin, OUTPUT);
            break;
        case ETHERVOX_GPIO_INPUT_PULLUP:
            pinMode(pin, INPUT);
            pullUpDnControl(pin, PUD_UP);
            break;
        case ETHERVOX_GPIO_INPUT_PULLDOWN:
            pinMode(pin, INPUT);
            pullUpDnControl(pin, PUD_DOWN);
            break;
        default:
            return -1;
    }
    
    return 0;
}

static int rpi_gpio_write(uint32_t pin, bool state) {
    if (!wiringpi_initialized) return -1;
    
    digitalWrite(pin, state ? HIGH : LOW);
    return 0;
}

static bool rpi_gpio_read(uint32_t pin) {
    if (!wiringpi_initialized) return false;
    
    return digitalRead(pin) == HIGH;
}

static int rpi_i2c_write(uint8_t device_addr, uint8_t reg_addr, const uint8_t* data, size_t len) {
    // Create new I2C handle for specific device
    int handle = wiringPiI2CSetup(device_addr);
    if (handle < 0) return -1;
    
    // Write register address followed by data
    if (wiringPiI2CWrite(handle, reg_addr) < 0) {
        return -1;
    }
    
    for (size_t i = 0; i < len; i++) {
        if (wiringPiI2CWrite(handle, data[i]) < 0) {
            return -1;
        }
    }
    
    return 0;
}

static int rpi_i2c_read(uint8_t device_addr, uint8_t reg_addr, uint8_t* data, size_t len) {
    // Create new I2C handle for specific device
    int handle = wiringPiI2CSetup(device_addr);
    if (handle < 0) return -1;
    
    // Write register address
    if (wiringPiI2CWrite(handle, reg_addr) < 0) {
        return -1;
    }
    
    // Read data
    for (size_t i = 0; i < len; i++) {
        int byte = wiringPiI2CRead(handle);
        if (byte < 0) return -1;
        data[i] = (uint8_t)byte;
    }
    
    return 0;
}

static int rpi_spi_transfer(const uint8_t* tx_data, uint8_t* rx_data, size_t len) {
    if (!tx_data || len == 0) return -1;
    
    // Copy tx_data to rx_data buffer for in-place transfer
    if (rx_data && tx_data != rx_data) {
        memcpy(rx_data, tx_data, len);
    }
    
    uint8_t* buffer = rx_data ? rx_data : (uint8_t*)tx_data;
    
    return wiringPiSPIDataRW(0, buffer, len);
}

static void rpi_delay_ms(uint32_t ms) {
    delay(ms);  // WiringPi delay function
}

static void rpi_delay_us(uint32_t us) {
    delayMicroseconds(us);  // WiringPi microsecond delay
}

static uint64_t rpi_get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

static void rpi_reset(void) {
    // Raspberry Pi reset via system call
    system("sudo shutdown -r now");
}

static void rpi_enter_sleep_mode(ethervox_sleep_mode_t mode) {
    // Raspberry Pi doesn't have hardware sleep modes like microcontrollers
    // Implement power management through system calls
    switch (mode) {
        case ETHERVOX_SLEEP_LIGHT:
            // Reduce CPU frequency
            system("echo powersave | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor");
            break;
        case ETHERVOX_SLEEP_DEEP:
            // Suspend to RAM
            system("sudo systemctl suspend");
            break;
        default:
            break;
    }
}

static uint32_t rpi_get_free_heap_size(void) {
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        return (uint32_t)(info.freeram * info.mem_unit / 1024);  // Return KB
    }
    return 0;
}

static float rpi_get_cpu_temperature(void) {
    FILE* temp_file = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (temp_file == NULL) {
        return 25.0f;  // Default temperature
    }
    
    int temp_millicelsius;
    if (fscanf(temp_file, "%d", &temp_millicelsius) == 1) {
        fclose(temp_file);
        return temp_millicelsius / 1000.0f;
    }
    
    fclose(temp_file);
    return 25.0f;
}

// Register Raspberry Pi-specific HAL functions
int ethervox_platform_register_hal(ethervox_platform_t* platform) {
    if (!platform) return -1;
    
    platform->hal.init = rpi_init;
    platform->hal.cleanup = rpi_cleanup;
    
    platform->hal.gpio_configure = rpi_gpio_configure;
    platform->hal.gpio_write = rpi_gpio_write;
    platform->hal.gpio_read = rpi_gpio_read;
    
    platform->hal.i2c_write = rpi_i2c_write;
    platform->hal.i2c_read = rpi_i2c_read;
    
    platform->hal.spi_transfer = rpi_spi_transfer;
    
    platform->hal.delay_ms = rpi_delay_ms;
    platform->hal.delay_us = rpi_delay_us;
    platform->hal.get_timestamp_us = rpi_get_timestamp_us;
    
    platform->hal.reset = rpi_reset;
    platform->hal.enter_sleep_mode = rpi_enter_sleep_mode;
    platform->hal.get_free_heap_size = rpi_get_free_heap_size;
    platform->hal.get_cpu_temperature = rpi_get_cpu_temperature;
    
    return 0;
}

#endif // ETHERVOX_PLATFORM_RPI