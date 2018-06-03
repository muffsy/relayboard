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
#include "nvs.h"

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

/* Web server content */
static const char *http_200_ok_html = "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n";
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");
size_t  index_html_len;

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;

void relays_clear()
{
    LOGI("All relays off");
    for (unsigned int i = 0; i < 5; i++) {
        gpio_set_level(GPIO_PIN_RELAY[i], 0);
    }
}

void relay_on(unsigned int relay)
{
    LOGI("Relay %d on", relay);
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

int8_t nvs_read()
{
    uint8_t value = 0;
    nvs_handle nvs;
    if (nvs_open("storage", NVS_READWRITE, &nvs) == ESP_OK) {
        switch (nvs_get_u8(nvs, "value", &value)) {
            case ESP_OK:
            case ESP_ERR_NVS_NOT_FOUND:
                break;
            default:
                LOGE("nvs_get() failed");
                break;
        }
    } else {
        LOGE("nvs_open() failed");
    }
    nvs_close(nvs);
    return value;
}

void nvs_write(uint8_t value)
{
    nvs_handle nvs;

    if (nvs_open("storage", NVS_READWRITE, &nvs) == ESP_OK) {
        if (nvs_set_u8(nvs, "value", value) != ESP_OK) {
            LOGE("nvs_set() failed");
        }
    } else {
        LOGE("nvs_open() failed");
    }
    nvs_close(nvs);
}

void app_main()
{
    LOGI("Muffsy Relay Input Selector");

    /* Print chip information */
    esp_info();

    /* Configure GPIO pins */
    gpio_init();

    /* Init flash */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    /* Default to all off */
    relays_clear();

    /* Restore last state from flash */
    int8_t value = nvs_read();
    if (value) {
        relay_on(value - 1);
    }

    /* Init WiFi and wait for network connection */
    init_wifi();
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
    LOGI("Connected!");

    /* Setup server */
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        LOGE("socket() failed");
        esp_restart();
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(80);

    if (bind(server_socket, (struct sockaddr*)(&server_addr), sizeof(struct sockaddr)) < 0) {
        LOGE("bind() failed");
        esp_restart();
    }

    if (listen(server_socket, 8) < 0) {
        LOGE("listen() failed");
        esp_restart();
    }

    index_html_len = index_html_end - index_html_start;

    /* Run server and relay logic */
    for (;;) {
        socklen_t length = sizeof(client_addr);
        int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &length);

        static char buffer[1024] = {0};
        if (read(client_socket, buffer, 1024)) {
            write(client_socket, http_200_ok_html, strlen(http_200_ok_html));

            if (!strncmp(buffer, "GET", 3)) {
                if (buffer[5] == ' ') {
                    write(client_socket, index_html_start, index_html_len);
                } else if (buffer[5] == '?') {
                    // TODO: Return current status
                    write(client_socket, "?", 1);
                } else if (buffer[5] >= '0' && buffer[5] <= '5') {
                    relays_clear();
                    int relay = buffer[5] - '0';
                    if (relay) {
                        relay_on(relay - 1);
                    }
                    nvs_write(relay);
                }
                write(client_socket, "\0", 1);
            }
        }
        close(client_socket);
    }

    fflush(stdout);
    esp_restart();
}
