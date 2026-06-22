#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "font.h"
#include "slots.h"
#include "stripe.h"
#include "webserver.h"
#include "wifi.h"

#define SCROLL_HZ 30

static const char *TAG = "ledstripe";

typedef struct {
    const char *msg_buf;
    const char *msg_pos;
    const char *color_buf;
    const char *color_pos;
    int shift;
} MSG_POS_T;

static void msg_pos_init(MSG_POS_T *pos, const char *msg_buf, const char *color_buf)
{
    pos->msg_buf = msg_buf;
    pos->msg_pos = msg_buf;
    pos->color_buf = color_buf;
    pos->color_pos = color_buf;
    pos->shift = 0;
}

typedef struct {
    char name;
    uint8_t r;
    uint8_t g;
    uint8_t b;
} NAMED_RGB_COLOR_T;

static const NAMED_RGB_COLOR_T color_map[] = {
    {'r', 0xff, 0x00, 0x00},
    {'g', 0x00, 0xff, 0x00},
    {'b', 0x00, 0x00, 0xff},
    {'c', 0x00, 0xff, 0xff},
    {'y', 0xff, 0xff, 0x00},
    {'m', 0xff, 0x00, 0xff},
    {'w', 0xff, 0xff, 0xff},
    {0},
};

static const NAMED_RGB_COLOR_T *lookup_color(char name)
{
    const NAMED_RGB_COLOR_T *p;

    for (p = color_map; p->name != 0; p++) {
        if (p->name == name) {
            return p;
        }
    }

    return NULL;
}

static void msg_pos_inc(MSG_POS_T *pos)
{
    (pos->shift)++;
    if (pos->shift < FONT_WIDTH) {
        return;
    }

    pos->shift = 0;

    (pos->msg_pos)++;
    if (*(pos->msg_pos) == 0) {
        pos->msg_pos = pos->msg_buf;
    }

    (pos->color_pos)++;
    if (*(pos->color_pos) == 0) {
        pos->color_pos = pos->color_buf;
    }
}

static MSG_POS_T msg_pos;
static uint8_t led_buf[FONT_HEIGHT][LED_COLS][LED_CHANNELS];

static void render_scroll_step(void)
{
    int x, y;
    MSG_POS_T mp;
    uint16_t m;
    const NAMED_RGB_COLOR_T *c;

    memcpy(&mp, &msg_pos, sizeof(mp));
    for (x = 0; x < LED_COLS; x++) {
        m = font[(uint8_t)*(mp.msg_pos)][mp.shift];
        c = lookup_color(*(mp.color_pos));
        msg_pos_inc(&mp);

        for (y = 0; y < FONT_HEIGHT; y++, m >>= 1) {
            if (c != NULL && (m & 1) != 0) {
                led_buf[y][x][0] = c->g;
                led_buf[y][x][1] = c->r;
                led_buf[y][x][2] = c->b;
            } else {
                led_buf[y][x][0] = 0;
                led_buf[y][x][1] = 0;
                led_buf[y][x][2] = 0;
            }
        }
    }

    stripe_send(led_buf);
    msg_pos_inc(&msg_pos);
}

static void scroll_task(void *arg)
{
    (void)arg;

    const TickType_t delay_ticks = pdMS_TO_TICKS(1000 / SCROLL_HZ);
    TickType_t last_wake = xTaskGetTickCount();

    int last_active = -1;
    bool slot_cleared = false;
    slot_t active_copy;
    memset(&active_copy, 0, sizeof(active_copy));

    while (1) {
        int current_active = slots_get_active();

        if (current_active != last_active || slots_check_changed()) {
            last_active = current_active;
            slots_copy_active(&active_copy);
            slot_cleared = false;

            if (active_copy.text[0] != '\0') {
                /* msg_pos stores pointers into active_copy.text / active_copy.colors.
                 * This is safe because active_copy lives for the lifetime of
                 * scroll_task(), which never returns. */
                msg_pos_init(&msg_pos, active_copy.text, active_copy.colors);
            }
        }

        if (active_copy.text[0] != '\0') {
            render_scroll_step();
        } else if (!slot_cleared) {
            /* Clear the display once when switching to an empty slot,
             * then stop sending until the slot changes again. */
            memset(led_buf, 0, sizeof(led_buf));
            stripe_send(led_buf);
            slot_cleared = true;
        }

        vTaskDelayUntil(&last_wake, delay_ticks);
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Initializing LED stripe");
    stripe_init();

    ESP_LOGI(TAG, "Initializing slots");
    slots_init();

    ESP_LOGI(TAG, "Starting WiFi AP");
    wifi_init();

    ESP_LOGI(TAG, "Starting web server");
    webserver_init();

    ESP_LOGI(TAG, "Starting scroll task at %d Hz", SCROLL_HZ);
    memset(led_buf, 0, sizeof(led_buf));
    /* Stack increased from 4096: slot_t local (512 B) + slots API calls. */
    xTaskCreate(scroll_task, "scroll_task", 8192, NULL, 5, NULL);
}
