#include "slots.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include "nvs_flash.h"

#define NVS_NAMESPACE "slots"
#define DEFAULT_TEXT "Hello LED World!"
#define DEFAULT_COLORS "wwwwwwwwwwwwwwww"

static const char *TAG = "slots";

static slot_t slots[SLOTS_COUNT];
static int active_slot = 0;
static SemaphoreHandle_t slots_mutex;
static volatile bool slots_changed_flag = false;

void slots_init(void)
{
    slots_mutex = xSemaphoreCreateMutex();

    /* Initialize all slots to empty, then set slot 0 demo defaults. */
    memset(slots, 0, sizeof(slots));
    strncpy(slots[0].text, DEFAULT_TEXT, SLOT_MAX_LEN - 1);
    strncpy(slots[0].colors, DEFAULT_COLORS, SLOT_MAX_LEN - 1);

    /* NVS init — required before any NVS read/write. */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Try to load saved slots from NVS. */
    nvs_handle_t nvs;
    ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs);
    if (ret != ESP_OK) {
        ESP_LOGI(TAG, "No saved slots found, using defaults");
        return;
    }

    /* Load active slot index. */
    int32_t active = 0;
    ret = nvs_get_i32(nvs, "active", &active);
    if (ret == ESP_OK && active >= 0 && active < SLOTS_COUNT) {
        active_slot = (int)active;
    }

    /* Load text and colors for each slot. */
    for (int i = 0; i < SLOTS_COUNT; i++) {
        char key_text[16];
        char key_color[16];
        snprintf(key_text, sizeof(key_text), "slot%d_text", i);
        snprintf(key_color, sizeof(key_color), "slot%d_color", i);

        size_t len = SLOT_MAX_LEN;
        ret = nvs_get_str(nvs, key_text, slots[i].text, &len);
        if (ret != ESP_OK) {
            slots[i].text[0] = '\0';
        }

        len = SLOT_MAX_LEN;
        ret = nvs_get_str(nvs, key_color, slots[i].colors, &len);
        if (ret != ESP_OK) {
            slots[i].colors[0] = '\0';
        }
    }

    nvs_close(nvs);
    ESP_LOGI(TAG, "Slots loaded from NVS, active=%d", active_slot);
}

void slots_save(int index, const char *text, const char *colors)
{
    if (index < 0 || index >= SLOTS_COUNT) {
        ESP_LOGE(TAG, "slots_save: invalid index %d", index);
        return;
    }

    xSemaphoreTake(slots_mutex, portMAX_DELAY);
    strncpy(slots[index].text, text, SLOT_MAX_LEN - 1);
    slots[index].text[SLOT_MAX_LEN - 1] = '\0';
    strncpy(slots[index].colors, colors, SLOT_MAX_LEN - 1);
    slots[index].colors[SLOT_MAX_LEN - 1] = '\0';
    slots_changed_flag = true;
    xSemaphoreGive(slots_mutex);

    /* Persist to NVS. */
    nvs_handle_t nvs;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (ret == ESP_OK) {
        char key_text[16];
        char key_color[16];
        snprintf(key_text, sizeof(key_text), "slot%d_text", index);
        snprintf(key_color, sizeof(key_color), "slot%d_color", index);
        nvs_set_str(nvs, key_text, slots[index].text);
        nvs_set_str(nvs, key_color, slots[index].colors);
        nvs_commit(nvs);
        nvs_close(nvs);
    } else {
        ESP_LOGE(TAG, "Failed to open NVS for write: %s", esp_err_to_name(ret));
    }

    ESP_LOGI(TAG, "Slot %d saved", index);
}

void slots_set_active(int index)
{
    if (index < 0 || index >= SLOTS_COUNT) {
        ESP_LOGE(TAG, "slots_set_active: invalid index %d", index);
        return;
    }

    xSemaphoreTake(slots_mutex, portMAX_DELAY);
    active_slot = index;
    slots_changed_flag = true;
    xSemaphoreGive(slots_mutex);

    /* Persist to NVS. */
    nvs_handle_t nvs;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (ret == ESP_OK) {
        nvs_set_i32(nvs, "active", (int32_t)index);
        nvs_commit(nvs);
        nvs_close(nvs);
    } else {
        ESP_LOGE(TAG, "Failed to open NVS for write: %s", esp_err_to_name(ret));
    }

    ESP_LOGI(TAG, "Active slot set to %d", index);
}

int slots_get_active(void)
{
    xSemaphoreTake(slots_mutex, portMAX_DELAY);
    int active = active_slot;
    xSemaphoreGive(slots_mutex);
    return active;
}

const slot_t *slots_get(int index)
{
    if (index < 0 || index >= SLOTS_COUNT) {
        return NULL;
    }
    return &slots[index];
}

const slot_t *slots_get_all(void)
{
    return slots;
}

void slots_copy_active(slot_t *dst)
{
    xSemaphoreTake(slots_mutex, portMAX_DELAY);
    memcpy(dst, &slots[active_slot], sizeof(slot_t));
    xSemaphoreGive(slots_mutex);
}

bool slots_check_changed(void)
{
    if (!slots_changed_flag) {
        return false;
    }
    slots_changed_flag = false;
    return true;
}
