idf_component_register(
    SRCS "main.c" "stripe.c" "font.c" "wifi.c"
    INCLUDE_DIRS "."
    REQUIRES driver led_strip esp_wifi esp_netif esp_http_server nvs_flash
)
