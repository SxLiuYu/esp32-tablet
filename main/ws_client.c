/* WebSocket client for yuanfang-brain ws://192.168.1.10:7103/ws/audio
 * Protocol: JSON {type:"audio"|"text", data:base64_pcm, sr:16000}
 * Reply:    JSON {type:"reply", text:"...", audio:base64_mp3}
 */
#include "ws_client.h"
#include "ui.h"
#include "rgb.h"
#include "audio.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_websocket_client.h"
#include "esp_event.h"
#include "cJSON.h"

static const char *TAG = "ws";
static esp_websocket_client_handle_t s_client;
static TaskHandle_t s_audio_task_h;
static QueueHandle_t s_audio_q;
static volatile int s_audio_active = 0;

/* Audio task waits for PCM data and streams it */
static void _audio_task(void *arg)
{
    char msg[8192]; // limited: classic ESP32 no PSRAM
    while (1) {
        if (xQueueReceive(s_audio_q, msg, portMAX_DELAY) == pdTRUE) {
            if (esp_websocket_client_is_connected(s_client)) {
                esp_websocket_client_send_text(s_client, msg, strlen(msg));
            }
        }
    }
}

static void _send_json(const char *type, const char *data, const char *sr)
{
    char buf[1024];
    int len = snprintf(buf, sizeof(buf),
        "{\"type\":\"%s\",\"data\":\"%s\",\"sr\":%s}", type, data, sr);
    if (esp_websocket_client_is_connected(s_client)) {
        esp_websocket_client_send_text(s_client, buf, len);
    }
}

void ws_client_send_text(const char *text)
{
    // text type - base64 encode the string as a placeholder
    char b64[512];
    // simple base64 for ASCII text only (token/sensitive data never here)
    _send_json("text", text, "16000");
}

void ws_client_send_audio_start(void)
{
    ESP_LOGI(TAG, "audio session start");
    s_audio_active = 1;
    ui_set_state(UI_STATE_LISTENING);
    rgb_set_color(0x0000FF);
}

void ws_client_send_audio(const char *base64_pcm, int samples)
{
    if (!s_audio_active || !esp_websocket_client_is_connected(s_client)) return;
    // queue for audio task to send
    char msg[8192];
    snprintf(msg, sizeof(msg), "{\"type\":\"audio\",\"data\":\"%s\",\"sr\":16000}", base64_pcm);
    if (xQueueSend(s_audio_q, msg, 0) != pdTRUE) {
        ESP_LOGW(TAG, "audio q full, dropping frame");
    }
}

void ws_client_end_session(void)
{
    s_audio_active = 0;
}

static char s_resp_buf[16384]; // holds accumulated response text
static int  s_resp_len = 0;

static void _on_websocket_event(void *arg, esp_event_base_t evb, int32_t evid, void *data)
{
    esp_websocket_event_data_t *event = (esp_websocket_event_data_t *)data;
    switch (evid) {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "WS connected");
        ui_set_state(UI_STATE_READY);
        rgb_set_color(0x00FF00);
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "WS disconnected");
        rgb_set_color(0xFF0000);
        ui_set_state(UI_STATE_ERROR);
        break;
    case WEBSOCKET_EVENT_DATA:
        if (event->data_len > 0 && event->data_len < 16384 - s_resp_len) {
            memcpy(s_resp_buf + s_resp_len, event->data, event->data_len);
            s_resp_len += event->data_len;
            s_resp_buf[s_resp_len] = '\0';
            // Try to parse complete JSON messages (newline-delimited)
            char *nl = strchr(s_resp_buf, '\n');
            if (nl) {
                *nl = '\0';
                cJSON *root = cJSON_Parse(s_resp_buf);
                if (root) {
                    cJSON *type = cJSON_GetObjectItem(root, "type");
                    if (type && strcmp(type->valuestring, "reply") == 0) {
                        cJSON *txt = cJSON_GetObjectItem(root, "text");
                        cJSON *aud = cJSON_GetObjectItem(root, "audio");
                        if (txt) {
                            ui_show_reply(txt->valuestring);
                        }
                        if (aud && aud->valuestring && aud->valuestring[0]) {
                            // decode base64 mp3 and play
                            audio_play_b64_mp3(aud->valuestring);
                        }
                        s_audio_active = 0;
                        ui_set_state(UI_STATE_IDLE);
                        rgb_set_color(0x00FF00);
                        vTaskDelay(pdMS_TO_TICKS(1500));
                        rgb_set_color(0);
                    }
                    cJSON_Delete(root);
                }
                memmove(s_resp_buf, nl + 1, s_resp_len - (nl - s_resp_buf) - 1);
                s_resp_len -= (nl - s_resp_buf) + 1;
            }
        }
        break;
    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGE(TAG, "WS error");
        break;
    default:
        break;
    }
}

void ws_client_init(void)
{
    s_audio_q = xQueueCreate(4, 8192);
    s_resp_len = 0;
    s_audio_active = 0;

    esp_websocket_client_config_t cfg = {
        .uri = "ws://192.168.1.10:7103/ws/audio",
        .ping_interval_sec = 20,
        .timeout_ms = 5000,
    };
    s_client = esp_websocket_client_init(&cfg);
    ESP_ERROR_CHECK(esp_websocket_register_events(s_client, WEBSOCKET_EVENT_ANY, _on_websocket_event, NULL));
    ESP_ERROR_CHECK(esp_websocket_client_start(s_client));

    xTaskCreate(_audio_task, "ws_audio", 3072, NULL, 4, &s_audio_task_h);
}