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
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#define LOGV(...) ESP_LOGV(__func__, __VA_ARGS__)
#define LOGD(...) ESP_LOGD(__func__, __VA_ARGS__)
#define LOGI(...) ESP_LOGI(__func__, __VA_ARGS__)
#define LOGW(...) ESP_LOGW(__func__, __VA_ARGS__)
#define LOGE(...) ESP_LOGE(__func__, __VA_ARGS__)

/* Muffsy Relay Input Selector
   R1: IO23 index 0
   R2: IO22 index 1
   R3: IO21 index 2
   R4: IO19 index 3
   R5: IO18 index 4
 */
unsigned int GPIO_PIN_RELAY[] = {23, 22, 21, 19, 18};

#define GPIO_OUTPUT_PIN_MASK ((1ULL<<GPIO_PIN_RELAY[0]) | (1ULL<<GPIO_PIN_RELAY[1]) | (1ULL<<GPIO_PIN_RELAY[2]) | (1ULL<<GPIO_PIN_RELAY[3]) | (1ULL<<GPIO_PIN_RELAY[4]))

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;

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
    LOGI("This is ESP32 chip with %d CPU cores, WiFi%s%s, silicon revision %d, %dMB %s flash",
            chip_info.cores,
            (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
            (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "",
            chip_info.revision,
            spi_flash_get_chip_size() / (1024 * 1024),
            (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
}

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    default:
        break;
    }   
    return ESP_OK;
}

static void init_wifi(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    wifi_config_t wifi_config = { 
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASS,
        },
    };
    LOGI("Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}

void app_main()
{
    LOGI("Muffsy Relay Input Selector");

    /* Print chip information */
    esp_info();

    /* Configure GPIO pins */
    gpio_init();

    /* Default to all off */
    relays_clear();

    /* Init flash */
    ESP_ERROR_CHECK(nvs_flash_init());

    /* Init WiFi and wait for network connection */
    init_wifi();
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
    LOGI("Connected!");

    unsigned int counter = 0;
    for (;;) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        LOGI("Relay %d", counter % 5);
        relays_clear();
        vTaskDelay(100 / portTICK_PERIOD_MS);
        relay_on(counter % 5);
        counter++;
    }

    fflush(stdout);
    esp_restart();
}
