#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "config.h"
#include "wifi.h"
#include "beacon.h"
#include "web.h"

void app_main(void)
{
    printf("Saturn SSID Spoofer starting\n");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        printf("NVS init failed: %d\n", ret);
        return;
    }

    esp_netif_init();
    esp_event_loop_create_default();

    wifi_init();
    beacon_init();
    start_webserver();

    printf("Ready! Connect to '%s' and visit http://192.168.4.1\n", AP_SSID);
}
