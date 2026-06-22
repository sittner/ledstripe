#include "webserver.h"
#include "slots.h"

#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "esp_http_server.h"
#include "esp_log.h"

#define WS_MAX_CLIENTS 4

static const char *TAG = "webserver";

static httpd_handle_t server = NULL;

static const char index_html[] =
    "<!DOCTYPE html>\n"
    "<html>\n"
    "<head><meta charset=\"UTF-8\"><title>LED Stripe Editor</title></head>\n"
    "<body><h1>Editor coming soon</h1></body>\n"
    "</html>\n";

static esp_err_t root_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, index_html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static char *build_state_json(void)
{
    slot_t all_slots[SLOTS_COUNT];
    int active;
    slots_copy_all(all_slots, &active);

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return NULL;
    }

    cJSON_AddStringToObject(root, "cmd", "state");
    cJSON_AddNumberToObject(root, "active", active);

    cJSON *slots_arr = cJSON_AddArrayToObject(root, "slots");
    for (int i = 0; i < SLOTS_COUNT; i++) {
        cJSON *slot_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(slot_obj, "text", all_slots[i].text);
        cJSON_AddStringToObject(slot_obj, "colors", all_slots[i].colors);
        cJSON_AddItemToArray(slots_arr, slot_obj);
    }

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

/* Broadcast args passed to an httpd work item so sends happen from the
 * httpd task context and the payload buffer is owned until after the send. */
typedef struct {
    httpd_handle_t hd;
    int fd;
    char *json;
} ws_bcast_arg_t;

static void ws_bcast_work(void *arg)
{
    ws_bcast_arg_t *bcast = (ws_bcast_arg_t *)arg;

    httpd_ws_frame_t frame = {
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t *)bcast->json,
        .len = strlen(bcast->json),
    };

    esp_err_t ret = httpd_ws_send_frame_async(bcast->hd, bcast->fd, &frame);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send state to fd=%d: %s",
                 bcast->fd, esp_err_to_name(ret));
    }

    free(bcast->json);
    free(bcast);
}

static void ws_broadcast_state(void)
{
    if (server == NULL) {
        return;
    }

    size_t fds = WS_MAX_CLIENTS;
    int client_fds[WS_MAX_CLIENTS];
    esp_err_t ret = httpd_get_client_list(server, &fds, client_fds);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to get client list: %s", esp_err_to_name(ret));
        return;
    }

    for (size_t i = 0; i < fds; i++) {
        if (httpd_ws_get_fd_info(server, client_fds[i]) != HTTPD_WS_CLIENT_WEBSOCKET) {
            continue;
        }

        char *json = build_state_json();
        if (json == NULL) {
            ESP_LOGE(TAG, "Failed to build state JSON");
            continue;
        }

        ws_bcast_arg_t *bcast = malloc(sizeof(ws_bcast_arg_t));
        if (bcast == NULL) {
            ESP_LOGE(TAG, "Failed to allocate broadcast arg");
            free(json);
            continue;
        }

        bcast->hd = server;
        bcast->fd = client_fds[i];
        bcast->json = json;

        ret = httpd_queue_work(server, ws_bcast_work, bcast);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to queue broadcast work: %s", esp_err_to_name(ret));
            free(json);
            free(bcast);
        }
    }
}

static void handle_ws_message(const char *msg)
{
    cJSON *root = cJSON_Parse(msg);
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON: %s", msg);
        return;
    }

    cJSON *cmd = cJSON_GetObjectItem(root, "cmd");
    if (cmd == NULL || !cJSON_IsString(cmd)) {
        ESP_LOGE(TAG, "Missing or invalid 'cmd' field");
        cJSON_Delete(root);
        return;
    }

    if (strcmp(cmd->valuestring, "save") == 0) {
        cJSON *slot = cJSON_GetObjectItem(root, "slot");
        cJSON *text = cJSON_GetObjectItem(root, "text");
        cJSON *colors = cJSON_GetObjectItem(root, "colors");

        if (cJSON_IsNumber(slot) && cJSON_IsString(text) && cJSON_IsString(colors)) {
            double dval = slot->valuedouble;
            int idx = (int)dval;
            if ((double)idx != dval || idx < 0 || idx >= SLOTS_COUNT) {
                ESP_LOGE(TAG, "Invalid slot index in 'save' command");
            } else {
                slots_save(idx, text->valuestring, colors->valuestring);
                ws_broadcast_state();
            }
        } else {
            ESP_LOGE(TAG, "Invalid 'save' command parameters");
        }
    } else if (strcmp(cmd->valuestring, "activate") == 0) {
        cJSON *slot = cJSON_GetObjectItem(root, "slot");
        if (cJSON_IsNumber(slot)) {
            double dval = slot->valuedouble;
            int idx = (int)dval;
            if ((double)idx != dval || idx < 0 || idx >= SLOTS_COUNT) {
                ESP_LOGE(TAG, "Invalid slot index in 'activate' command");
            } else {
                slots_set_active(idx);
                ws_broadcast_state();
            }
        } else {
            ESP_LOGE(TAG, "Invalid 'activate' command parameters");
        }
    } else {
        ESP_LOGW(TAG, "Unknown command: %s", cmd->valuestring);
    }

    cJSON_Delete(root);
}

static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        /* New WebSocket connection — send full current state. */
        ESP_LOGI(TAG, "New WebSocket connection, fd=%d", httpd_req_to_sockfd(req));

        char *json = build_state_json();
        if (json != NULL) {
            httpd_ws_frame_t frame = {
                .type = HTTPD_WS_TYPE_TEXT,
                .payload = (uint8_t *)json,
                .len = strlen(json),
            };
            httpd_ws_send_frame(req, &frame);
            free(json);
        }
        return ESP_OK;
    }

    /* Receive incoming frame (first call with len=0 to get frame length). */
    httpd_ws_frame_t frame = {0};
    frame.type = HTTPD_WS_TYPE_TEXT;

    esp_err_t ret = httpd_ws_recv_frame(req, &frame, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get frame size: %s", esp_err_to_name(ret));
        return ret;
    }

    if (frame.len == 0) {
        return ESP_OK;
    }

    frame.payload = malloc(frame.len + 1);
    if (frame.payload == NULL) {
        ESP_LOGE(TAG, "Failed to allocate frame buffer (%zu bytes)", frame.len + 1);
        return ESP_ERR_NO_MEM;
    }

    ret = httpd_ws_recv_frame(req, &frame, frame.len);
    if (ret == ESP_OK) {
        frame.payload[frame.len] = '\0';
        ESP_LOGI(TAG, "Received WS: %s", (char *)frame.payload);
        handle_ws_message((char *)frame.payload);
    } else {
        ESP_LOGE(TAG, "Failed to receive frame: %s", esp_err_to_name(ret));
    }

    free(frame.payload);
    return ret;
}

static const httpd_uri_t root_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = root_handler,
    .user_ctx = NULL,
};

static const httpd_uri_t ws_uri = {
    .uri = "/ws",
    .method = HTTP_GET,
    .handler = ws_handler,
    .user_ctx = NULL,
    .is_websocket = true,
};

void webserver_init(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = WS_MAX_CLIENTS + 2;

    ESP_LOGI(TAG, "Starting web server on port %d", config.server_port);
    esp_err_t ret = httpd_start(&server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start web server: %s", esp_err_to_name(ret));
        return;
    }

    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &root_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &ws_uri));
    ESP_LOGI(TAG, "Web server started");
}
