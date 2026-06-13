/* Audio: I2S MIC (INMP441) + I2S DAC (MAX98357A)
 * Classic ESP32, no PSRAM: reduced buffer sizes
 * PCM: 16kHz 16bit mono, sent as base64
 */
#include "audio.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2s_std.h"

static const char *TAG = "audio";
static TaskHandle_t s_cap_task_h;
static volatile int s_capturing = 0;
static void (*s_capture_cb)(const char *, int) = NULL;

/* 16kHz 16bit mono: 320 bytes per 10ms frame */
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
    uint8_t b64_buf[512]; // base64 of 320 bytes = ~430 chars
    uint8_t pcm[FRAME_SIZE];

    i2s_std_slot_config_t slot = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
        I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);

    i2s_std_clk_config_t clk = I2S_STD_CLK_DEFAULT_CONFIG(16000);

    i2s_std_gpio_config_t gpio = {
        .bck_io_num = 26,
        .ws_io_num  = 25,
        .data_out_num = -1,
        .data_in_num  = 33,
        .mck_io_num = 0,
    };

    i2s_std_config_t cfg = {
        .clk_cfg = clk,
        .slot_cfg = slot,
        .gpio_cfg = gpio,
    };

    i2s_channel_handle_t rx_ch;
    ESP_ERROR_CHECK(i2s_new_channel(&cfg, NULL, &rx_ch));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_ch, &cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_ch));

    while (1) {
        if (!s_capturing) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        size_t bytes_read = 0;
        i2s_channel_read(rx_ch, pcm, FRAME_SIZE, &bytes_read, pdMS_TO_TICKS(100));
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
    // Classic ESP32: no PSRAM, MP3 decode not feasible
    ESP_LOGW(TAG, "MP3 playback skipped (no PSRAM for decode)");
    (void)b64_mp3;
}