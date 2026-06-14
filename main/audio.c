/* Audio: I2S MIC (INMP441) + I2S DAC (MAX98357A)
 * Classic ESP32, no PSRAM: reduced buffer sizes
 * PCM: 16kHz 16bit mono, sent as base64
 *
 * Modified 2026-06-14: IDF v5.3.2 API renames + soft-fail
 */
#include "audio.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"

static const char *TAG = "audio";
static TaskHandle_t s_cap_task_h;
static volatile int s_capturing = 0;
static i2s_chan_handle_t s_rx_ch = NULL;
static void (*s_capture_cb)(const char *, int) = NULL;
static volatile int s_audio_ready = 0;

#define FRAME_SIZE 320

static void _b64_encode(const uint8_t *src, int len, char *dst)
{
    static const char b64chars[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int i = 0;
    for (int j = 0; j < len; j += 3) {
        int a = src[j];
        int b = (j + 1 < len) ? src[j + 1] : 0;
        int c = (j + 2 < len) ? src[j + 2] : 0;
        dst[i++] = b64chars[(a >> 2) & 0x3F];
        dst[i++] = b64chars[((a << 4) | (b >> 4)) & 0x3F];
        dst[i++] = (j + 1 < len) ? b64chars[((b << 2) | (c >> 6)) & 0x3F] : '=';
        dst[i++] = (j + 2 < len) ? b64chars[c & 0x3F] : '=';
    }
    dst[i] = '\0';
}

static void _capture_task(void *arg)
{
    uint8_t b64_buf[512];
    uint8_t pcm[FRAME_SIZE];

    /* IDF v5.3.2: I2S channel must be created in caller context. We init it
     * here as a fallback; main.c audio_init() may have already done so. */
    if (s_rx_ch == NULL) {
        i2s_chan_config_t ch_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
        if (i2s_new_channel(&ch_cfg, NULL, &s_rx_ch) != ESP_OK || s_rx_ch == NULL) {
            ESP_LOGE(TAG, "i2s_new_channel failed - audio capture disabled");
            vTaskDelete(NULL);
            return;
        }
        i2s_std_config_t std_cfg = {
            .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000),
            .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
            .gpio_cfg = {
                .mclk = I2S_GPIO_UNUSED,
                .bclk = GPIO_NUM_26,
                .ws   = GPIO_NUM_25,
                .dout = I2S_GPIO_UNUSED,
                .din  = GPIO_NUM_33,
                .invert_flags = { false, false, false },
            },
        };
        if (i2s_channel_init_std_mode(s_rx_ch, &std_cfg) != ESP_OK) {
            ESP_LOGE(TAG, "i2s init std failed");
            vTaskDelete(NULL);
            return;
        }
        if (i2s_channel_enable(s_rx_ch) != ESP_OK) {
            ESP_LOGE(TAG, "i2s enable failed");
            vTaskDelete(NULL);
            return;
        }
    }
    s_audio_ready = 1;
    ESP_LOGI(TAG, "audio capture ready");

    while (1) {
        if (!s_capturing) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        size_t bytes_read = 0;
        i2s_channel_read(s_rx_ch, pcm, FRAME_SIZE, &bytes_read, pdMS_TO_TICKS(100));
        if (bytes_read >= FRAME_SIZE && s_capture_cb) {
            _b64_encode(pcm, FRAME_SIZE, (char *)b64_buf);
            s_capture_cb((char *)b64_buf, FRAME_SIZE / 2);
        }
    }
    (void)arg;
}

void audio_init(void)
{
    ESP_LOGI(TAG, "audio init (16kHz 16bit mono I2S)");
    xTaskCreate(_capture_task, "audio_cap", 4096, NULL, 5, &s_cap_task_h);
}

void audio_capture_start(void (*cb)(const char *, int))
{
    s_capture_cb = cb;
    s_capturing = 1;
}

void audio_capture_stop(void)
{
    s_capturing = 0;
}

void audio_play_b64_mp3(const char *b64_mp3)
{
    ESP_LOGW(TAG, "MP3 playback skipped (no PSRAM for decode)");
    (void)b64_mp3;
}
