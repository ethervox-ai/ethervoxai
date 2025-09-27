/**
 * @file esp32_hal.c
 * @brief ESP32 Hardware Abstraction Layer implementation for EthervoxAI
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

#ifdef ETHERVOX_PLATFORM_ESP32

#include "ethervox/platform.h"
#include <driver/gpio.h>
#include <driver/i2c.h>
#include <driver/spi_master.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// ESP32-specific HAL implementation

static int esp32_init(ethervox_platform_info_t* info) {
    // Initialize ESP32 subsystems
    printf("Initializing ESP32 platform\n");
    
    // Configure default GPIO pins
    gpio_install_isr_service(0);
    
    // Initialize I2C
    i2c_config_t i2c_config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = 21,
        .scl_io_num = 22,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    i2c_param_config(I2C_NUM_0, &i2c_config);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
    
    return 0;
}

static void esp32_cleanup(ethervox_platform_info_t* info) {
    printf("Cleaning up ESP32 platform\n");
    i2c_driver_delete(I2C_NUM_0);
    gpio_uninstall_isr_service();
}

static int esp32_gpio_configure(uint32_t pin, ethervox_gpio_mode_t mode) {
    gpio_config_t config = {0};
    config.pin_bit_mask = (1ULL << pin);
    
    switch (mode) {
        case ETHERVOX_GPIO_INPUT:
            config.mode = GPIO_MODE_INPUT;
            break;
        case ETHERVOX_GPIO_OUTPUT:
            config.mode = GPIO_MODE_OUTPUT;
            break;
        case ETHERVOX_GPIO_INPUT_PULLUP:
            config.mode = GPIO_MODE_INPUT;
            config.pull_up_en = GPIO_PULLUP_ENABLE;
            break;
        case ETHERVOX_GPIO_INPUT_PULLDOWN:
            config.mode = GPIO_MODE_INPUT;
            config.pull_down_en = GPIO_PULLDOWN_ENABLE;
            break;
        default:
            return -1;
    }
    
    return gpio_config(&config);
}

static int esp32_gpio_write(uint32_t pin, bool state) {
    return gpio_set_level(pin, state ? 1 : 0);
}

static bool esp32_gpio_read(uint32_t pin) {
    return gpio_get_level(pin) != 0;
}

static int esp32_i2c_write(uint8_t device_addr, uint8_t reg_addr, const uint8_t* data, size_t len) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (device_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_write(cmd, data, len, true);
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    
    return (ret == ESP_OK) ? 0 : -1;
}

static int esp32_i2c_read(uint8_t device_addr, uint8_t reg_addr, uint8_t* data, size_t len) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (device_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_start(cmd);  // Repeated start
    i2c_master_write_byte(cmd, (device_addr << 1) | I2C_MASTER_READ, true);
    
    if (len > 1) {
        i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, data + len - 1, I2C_MASTER_NACK);
    
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    
    return (ret == ESP_OK) ? 0 : -1;
}

static int esp32_spi_transfer(const uint8_t* tx_data, uint8_t* rx_data, size_t len) {
    // Basic SPI transfer - would need proper configuration in real implementation
    spi_transaction_t transaction = {
        .length = len * 8,
        .tx_buffer = tx_data,
        .rx_buffer = rx_data,
    };
    
    return spi_device_transmit(NULL, &transaction);  // NULL device handle - placeholder
}

static void esp32_delay_ms(uint32_t ms) {
    vTaskDelay(ms / portTICK_PERIOD_MS);
}

static void esp32_delay_us(uint32_t us) {
    esp_rom_delay_us(us);
}

static uint64_t esp32_get_timestamp_us(void) {
    return esp_timer_get_time();
}

static void esp32_reset(void) {
    esp_restart();
}

static void esp32_enter_sleep_mode(ethervox_sleep_mode_t mode) {
    switch (mode) {
        case ETHERVOX_SLEEP_LIGHT:
            esp_light_sleep_start();
            break;
        case ETHERVOX_SLEEP_DEEP:
            esp_deep_sleep_start();
            break;
        default:
            break;
    }
}

static uint32_t esp32_get_free_heap_size(void) {
    return esp_get_free_heap_size();
}

static float esp32_get_cpu_temperature(void) {
    // ESP32 doesn't have built-in temperature sensor in all variants
    // This would require external sensor or SoC-specific implementation
    return 25.0f;  // Placeholder
}

// Register ESP32-specific HAL functions
int ethervox_platform_register_hal(ethervox_platform_t* platform) {
    if (!platform) return -1;
    
    platform->hal.init = esp32_init;
    platform->hal.cleanup = esp32_cleanup;
    
    platform->hal.gpio_configure = esp32_gpio_configure;
    platform->hal.gpio_write = esp32_gpio_write;
    platform->hal.gpio_read = esp32_gpio_read;
    
    platform->hal.i2c_write = esp32_i2c_write;
    platform->hal.i2c_read = esp32_i2c_read;
    
    platform->hal.spi_transfer = esp32_spi_transfer;
    
    platform->hal.delay_ms = esp32_delay_ms;
    platform->hal.delay_us = esp32_delay_us;
    platform->hal.get_timestamp_us = esp32_get_timestamp_us;
    
    platform->hal.reset = esp32_reset;
    platform->hal.enter_sleep_mode = esp32_enter_sleep_mode;
    platform->hal.get_free_heap_size = esp32_get_free_heap_size;
    platform->hal.get_cpu_temperature = esp32_get_cpu_temperature;
    
    return 0;
}

#endif // ETHERVOX_PLATFORM_ESP32