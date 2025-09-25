#ifdef ETHERVOX_PLATFORM_DESKTOP

#include "ethervox/platform.h"
#include <time.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef ETHERVOX_PLATFORM_WINDOWS
    #include <windows.h>
    #include <powrprof.h>
    #pragma comment(lib, "powrprof.lib")
#else
    #include <unistd.h>
    #include <sys/sysinfo.h>
    #include <sys/time.h>
#endif

// Desktop platform HAL implementation (Windows/Linux/macOS)
// Note: Desktop platforms don't typically have GPIO/SPI/I2C access

static int desktop_init(ethervox_platform_info_t* info) {
    printf("Initializing desktop platform\n");
    
    // Desktop platforms are already initialized by the OS
    // No specific hardware initialization needed
    
    return 0;
}

static void desktop_cleanup(ethervox_platform_info_t* info) {
    printf("Cleaning up desktop platform\n");
    // No specific cleanup needed for desktop platforms
}

// GPIO functions not available on desktop - return error
static int desktop_gpio_configure(uint32_t pin, ethervox_gpio_mode_t mode) {
    (void)pin; (void)mode;
    return -1;  // GPIO not available on desktop
}

static int desktop_gpio_write(uint32_t pin, bool state) {
    (void)pin; (void)state;
    return -1;  // GPIO not available on desktop
}

static bool desktop_gpio_read(uint32_t pin) {
    (void)pin;
    return false;  // GPIO not available on desktop
}

// I2C functions not available on standard desktop - return error
static int desktop_i2c_write(uint8_t device_addr, uint8_t reg_addr, const uint8_t* data, size_t len) {
    (void)device_addr; (void)reg_addr; (void)data; (void)len;
    return -1;  // I2C not available on standard desktop
}

static int desktop_i2c_read(uint8_t device_addr, uint8_t reg_addr, uint8_t* data, size_t len) {
    (void)device_addr; (void)reg_addr; (void)data; (void)len;
    return -1;  // I2C not available on standard desktop
}

// SPI functions not available on standard desktop - return error
static int desktop_spi_transfer(const uint8_t* tx_data, uint8_t* rx_data, size_t len) {
    (void)tx_data; (void)rx_data; (void)len;
    return -1;  // SPI not available on standard desktop
}

// Timing functions
static void desktop_delay_ms(uint32_t ms) {
    #ifdef ETHERVOX_PLATFORM_WINDOWS
        Sleep(ms);
    #else
        usleep(ms * 1000);
    #endif
}

static void desktop_delay_us(uint32_t us) {
    #ifdef ETHERVOX_PLATFORM_WINDOWS
        // Windows doesn't have microsecond sleep, use high-resolution timer
        LARGE_INTEGER frequency, start, end;
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&start);
        
        uint64_t target_ticks = us * frequency.QuadPart / 1000000;
        
        do {
            QueryPerformanceCounter(&end);
        } while ((end.QuadPart - start.QuadPart) < target_ticks);
    #else
        usleep(us);
    #endif
}

static uint64_t desktop_get_timestamp_us(void) {
    #ifdef ETHERVOX_PLATFORM_WINDOWS
        LARGE_INTEGER frequency, counter;
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&counter);
        return (uint64_t)(counter.QuadPart * 1000000 / frequency.QuadPart);
    #else
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (uint64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
    #endif
}

// System control functions
static void desktop_reset(void) {
    #ifdef ETHERVOX_PLATFORM_WINDOWS
        // Restart Windows
        ExitWindowsEx(EWX_REBOOT | EWX_FORCE, SHTDN_REASON_MAJOR_SOFTWARE);
    #else
        // Restart Linux/macOS
        system("sudo reboot");
    #endif
}

static void desktop_enter_sleep_mode(ethervox_sleep_mode_t mode) {
    #ifdef ETHERVOX_PLATFORM_WINDOWS
        switch (mode) {
            case ETHERVOX_SLEEP_LIGHT:
                // Put computer to sleep
                SetSuspendState(FALSE, FALSE, FALSE);
                break;
            case ETHERVOX_SLEEP_DEEP:
                // Hibernate
                SetSuspendState(TRUE, FALSE, FALSE);
                break;
            default:
                break;
        }
    #else
        switch (mode) {
            case ETHERVOX_SLEEP_LIGHT:
                // Suspend to RAM
                system("systemctl suspend");
                break;
            case ETHERVOX_SLEEP_DEEP:
                // Hibernate
                system("systemctl hibernate");
                break;
            default:
                break;
        }
    #endif
}

static uint32_t desktop_get_free_heap_size(void) {
    #ifdef ETHERVOX_PLATFORM_WINDOWS
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        GlobalMemoryStatusEx(&memInfo);
        return (uint32_t)(memInfo.ullAvailPhys / 1024);  // Return KB
    #else
        struct sysinfo info;
        if (sysinfo(&info) == 0) {
            return (uint32_t)(info.freeram * info.mem_unit / 1024);  // Return KB
        }
        return 0;
    #endif
}

static float desktop_get_cpu_temperature(void) {
    // CPU temperature monitoring on desktop requires platform-specific APIs
    // This is a simplified implementation
    
    #ifdef ETHERVOX_PLATFORM_LINUX
        // Try to read from common thermal zones
        FILE* temp_file = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
        if (temp_file != NULL) {
            int temp_millicelsius;
            if (fscanf(temp_file, "%d", &temp_millicelsius) == 1) {
                fclose(temp_file);
                return temp_millicelsius / 1000.0f;
            }
            fclose(temp_file);
        }
    #endif
    
    // Default temperature if unable to read
    return 45.0f;  // Typical desktop CPU temperature
}

// Register desktop-specific HAL functions
int ethervox_platform_register_hal(ethervox_platform_t* platform) {
    if (!platform) return -1;
    
    platform->hal.init = desktop_init;
    platform->hal.cleanup = desktop_cleanup;
    
    // GPIO/I2C/SPI not available on desktop - set to error functions
    platform->hal.gpio_configure = desktop_gpio_configure;
    platform->hal.gpio_write = desktop_gpio_write;
    platform->hal.gpio_read = desktop_gpio_read;
    
    platform->hal.i2c_write = desktop_i2c_write;
    platform->hal.i2c_read = desktop_i2c_read;
    
    platform->hal.spi_transfer = desktop_spi_transfer;
    
    // Timing and system functions are available
    platform->hal.delay_ms = desktop_delay_ms;
    platform->hal.delay_us = desktop_delay_us;
    platform->hal.get_timestamp_us = desktop_get_timestamp_us;
    
    platform->hal.reset = desktop_reset;
    platform->hal.enter_sleep_mode = desktop_enter_sleep_mode;
    platform->hal.get_free_heap_size = desktop_get_free_heap_size;
    platform->hal.get_cpu_temperature = desktop_get_cpu_temperature;
    
    return 0;
}

#endif // ETHERVOX_PLATFORM_DESKTOP