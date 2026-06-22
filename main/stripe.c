#include "stripe.h"

#include "esp_check.h"
#include "esp_log.h"
#include "led_strip.h"

#define LED_COUNT (FONT_HEIGHT * LED_COLS)
#define RMT_RESOLUTION_HZ (10 * 1000 * 1000)

static const char *TAG = "stripe";
static led_strip_handle_t strip;

void stripe_init(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = STRIPE_GPIO_NUM,
        .max_leds = LED_COUNT,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .led_model = LED_MODEL_WS2812,
        .flags = {
            .invert_out = false,
        },
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = RMT_RESOLUTION_HZ,
        .mem_block_symbols = 0,
        .flags = {
            .with_dma = true,
        },
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &strip));
    ESP_ERROR_CHECK(led_strip_clear(strip));

    ESP_LOGI(TAG, "WS2812B driver initialized on GPIO %d (%d LEDs)", STRIPE_GPIO_NUM, LED_COUNT);
}

void stripe_send(const uint8_t led_buf[FONT_HEIGHT][LED_COLS][LED_CHANNELS])
{
    esp_err_t err = ESP_OK;
    int failed_row = -1;
    int failed_col = -1;

    for (int row = 0; row < FONT_HEIGHT; row++) {
        for (int col = 0; col < LED_COLS; col++) {
            int index = row * LED_COLS + col;
            // If the matrix wiring is serpentine/zigzag, use per-row reversal, for example:
            // index = (row % 2 == 0) ? (row * LED_COLS + col)
            //                        : (row * LED_COLS + (LED_COLS - 1 - col));
            uint8_t g = led_buf[row][col][0];
            uint8_t r = led_buf[row][col][1];
            uint8_t b = led_buf[row][col][2];
            err = led_strip_set_pixel(strip, index, r, g, b);
            if (err != ESP_OK) {
                failed_row = row;
                failed_col = col;
                break;
            }
        }
        if (err != ESP_OK) {
            break;
        }
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "led_strip_set_pixel failed at row=%d col=%d: %s", failed_row, failed_col, esp_err_to_name(err));
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(led_strip_refresh(strip));
}
