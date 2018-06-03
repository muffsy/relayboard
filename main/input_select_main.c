/*
 * Muffsy Relay Input Selector
 *
 * Copyright 2018 Göran Hane
 *
 * See LICENSE for terms (BSD-3-Clause).
 *
 * This work is licensed under the terms of BSD-3-Clause
 * license which is included in the LICENSE file.
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"

/* Muffsy Relay Input Selector
   R1: IO23 index 0
   R2: IO22 index 1
   R3: IO21 index 2
   R4: IO19 index 3
   R5: IO18 index 4
 */
unsigned int GPIO_PIN_RELAY[] = {23, 22, 21, 19, 18};

#define GPIO_OUTPUT_PIN_MASK ((1ULL<<GPIO_PIN_RELAY[0]) | (1ULL<<GPIO_PIN_RELAY[1]) | (1ULL<<GPIO_PIN_RELAY[2]) | (1ULL<<GPIO_PIN_RELAY[3]) | (1ULL<<GPIO_PIN_RELAY[4]))

void relays_clear()
{
    printf("Muffsy Input Select\n");

void relay_on(unsigned int relay)
{
    gpio_set_level(GPIO_PIN_RELAY[relay], 1);
}

void gpio_init()
{
    gpio_config_t config;
    config.intr_type    = GPIO_PIN_INTR_DISABLE;
    config.mode         = GPIO_MODE_OUTPUT;
    config.pin_bit_mask = GPIO_OUTPUT_PIN_MASK;
    config.pull_down_en = 0;
    config.pull_up_en   = 0;
    gpio_config(&config);
}

void esp_info()
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("This is ESP32 chip with %d CPU cores, WiFi%s%s, silicon revision %d, %dMB %s flash\n",
            chip_info.cores,
            (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
            (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "",
            chip_info.revision,
            spi_flash_get_chip_size() / (1024 * 1024),
            (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
}

void app_main()
{
    printf("Muffsy Relay Input Selector\n");

    /* Print chip information */
    esp_info();

    /* Configure GPIO pins */
    gpio_init();

    /* Default to all off */
    relays_clear();

    unsigned int counter = 0;
    for (;;) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        printf("Relay %d\n", counter % 5);
        relays_clear();
        vTaskDelay(100 / portTICK_PERIOD_MS);
        relay_on(counter % 5);
        counter++;
    }

    fflush(stdout);
    esp_restart();
}
