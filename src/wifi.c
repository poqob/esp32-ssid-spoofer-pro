#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "config.h"

void wifi_init(void)
{
    printf("WiFi init...\n");
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    printf("WiFi init OK\n");

    wifi_config_t ap_config = {
        .ap = {
            .ssid = AP_SSID,
            .ssid_len = strlen(AP_SSID),
            .password = AP_PASS,
            .channel = AP_CHANNEL,
            .max_connection = 10,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    printf("Setting mode...\n");
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    printf("Setting config...\n");
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    printf("Starting WiFi...\n");
    ESP_ERROR_CHECK(esp_wifi_start());
    printf("AP: %s (pass: %s)\n", AP_SSID, AP_PASS);
}
