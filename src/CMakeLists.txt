idf_component_register(SRCS "main.c" "wifi.c" "beacon.c" "web.c"
                    INCLUDE_DIRS "."
                    REQUIRES nvs_flash esp_wifi esp_http_server)
