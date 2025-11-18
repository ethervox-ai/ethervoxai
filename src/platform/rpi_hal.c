/**
 * @file rpi_hal.c
 * @brief Raspberry Pi Hardware Abstraction Layer implementation for EthervoxAI
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 *
 * This file is part of EthervoxAI, licensed under CC BY-NC-SA 4.0.
 * You are free to share and adapt this work under the following terms:
 * - Attribution: Credit the original authors
 * - NonCommercial: Not for commercial use
 * - ShareAlike: Distribute under same license
 *
 * For full license terms, see: https://creativecommons.org/licenses/by-nc-sa/4.0/
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#ifdef ETHERVOX_PLATFORM_RPI

#ifdef ETHERVOX_RPI_HARDWARE
// Use actual wiringPi library on real hardware
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <wiringPiSPI.h>
#else
// Stub definitions for cross-compilation without wiringPi
#define INPUT 0
#define OUTPUT 1
#define HIGH 1
#define LOW 0
#define PUD_UP 2
#define PUD_DOWN 1

// Stub functions for cross-compilation
static inline int wiringPiSetupGpio(void) {
  return 0;
}
static inline void pinMode(int pin, int mode) {
  (void)pin;
  (void)mode;
}
static inline void digitalWrite(int pin, int value) {
  (void)pin;
  (void)value;
}
static inline int digitalRead(int pin) {
  (void)pin;
  return 0;
}
static inline void pullUpDnControl(int pin, int pud) {
  (void)pin;
  (void)pud;
}
static inline int wiringPiI2CSetup(int addr) {
  (void)addr;
  return -1;
}
static inline int wiringPiI2CWrite(int fd, int data) {
  (void)fd;
  (void)data;
  return 0;
}
static inline int wiringPiI2CRead(int fd) {
  (void)fd;
  return 0;
}
static inline int wiringPiSPISetup(int channel, int speed) {
  (void)channel;
  (void)speed;
  return 0;
}
static inline int wiringPiSPIDataRW(int channel, unsigned char* data, int len) {
  (void)channel;
  (void)data;
  (void)len;
  return 0;
}
static inline void delay(unsigned int ms) {
  (void)ms;
}
static inline void delayMicroseconds(unsigned int us) {
  (void)us;
}
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <time.h>
#include <unistd.h>

#include "ethervox/platform.h"

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
  if (!wiringpi_initialized)
    return -1;

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
  if (!wiringpi_initialized)
    return -1;

  digitalWrite(pin, state ? HIGH : LOW);
  return 0;
}

static bool rpi_gpio_read(uint32_t pin) {
  if (!wiringpi_initialized)
    return false;

  return digitalRead(pin) == HIGH;
}

static int rpi_i2c_write(uint32_t bus, uint8_t device_addr, const uint8_t* data, uint32_t len) {
  (void)bus;
  // Create new I2C handle for specific device
  int handle = wiringPiI2CSetup(device_addr);
  if (handle < 0)
    return -1;

  // First byte is typically the register address in I2C protocols
  // Write all data bytes
  for (uint32_t i = 0; i < len; i++) {
    if (wiringPiI2CWrite(handle, data[i]) < 0) {
      return -1;
    }
  }

  return 0;
}

static int rpi_i2c_read(uint32_t bus, uint8_t device_addr, uint8_t* data, uint32_t len) {
  (void)bus;

  // Create new I2C handle for specific device
  int handle = wiringPiI2CSetup(device_addr);
  if (handle < 0)
    return -1;

  // Read data directly
  for (uint32_t i = 0; i < len; i++) {
    int byte = wiringPiI2CRead(handle);
    if (byte < 0)
      return -1;
    data[i] = (uint8_t)byte;
  }

  return 0;
}

static int rpi_spi_transfer(uint32_t bus, const uint8_t* tx_data, uint8_t* rx_data, uint32_t len) {
  (void)bus;
  if (!tx_data || len == 0)
    return -1;

  // Copy tx_data to rx_data buffer for in-place transfer
  if (rx_data && tx_data != rx_data) {
    memcpy(rx_data, tx_data, len);
  }

  uint8_t* buffer = rx_data ? rx_data : (uint8_t*)tx_data;

  return wiringPiSPIDataRW(0, buffer, len);
}

static uint32_t rpi_delay_ms(uint32_t ms) {
  delay(ms);  // WiringPi delay function
  return ms;
}

static uint32_t rpi_delay_us(uint32_t us) {
  delayMicroseconds(us);  // WiringPi microsecond delay
  return us;
}

static uint64_t rpi_get_timestamp_us(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

static void rpi_reset(void) {
  int result = -1;
  // Raspberry Pi reset via system call
  result = system("sudo shutdown -r now");
}

static void rpi_enter_sleep_mode(ethervox_sleep_mode_t mode) {
  // Raspberry Pi doesn't have hardware sleep modes like microcontrollers
  // Implement power management through system calls
  int result = -1;
  switch (mode) {
    case ETHERVOX_SLEEP_LIGHT:
      // Reduce CPU frequency
      result =
          system("echo powersave | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor");
      break;
    case ETHERVOX_SLEEP_DEEP:
      // Suspend to RAM
      result = system("sudo systemctl suspend");
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
int rpi_hal_register(ethervox_platform_t* platform) {
  if (!platform)
    return -1;

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

  platform->hal.system_reset = rpi_reset;
  platform->hal.system_sleep = rpi_enter_sleep_mode;
  platform->hal.get_free_heap_size = rpi_get_free_heap_size;
  platform->hal.get_cpu_temperature = rpi_get_cpu_temperature;

  return 0;
}

#endif  // ETHERVOX_PLATFORM_RPI