/* WS2812B 2812 RGB LED strip driven by RMT (ESP32 RMT peripheral)
 * Classic ESP32: no PSRAM, use internal RMT buffer
 */
#include "rgb.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/rmt_encoder.h"
#include "driver/rmt_tx.h"
#include "driver/gpio.h"

static const char *TAG = "rgb";
#define RGB_LED_NUM   8   // adjust to actual strip length
#define RGB_GPIO      13  // data pin

static rmt_channel_handle_t s_rmt_ch;
static rmt_encoder_handle_t s_ws2812_enc;

static uint32_t _ws2812_bitmark(uint8_t r, uint8_t g, uint8_t b)
{
    // WS2812B: GRB order
    uint32_t word = 0;
    for (int i = 23; i >= 0; i--) {
        int bit = (i < 16) ? ((g >> (15 - i)) & 1)
                  : (i < 24) ? ((r >> (23 - i)) & 1)
                  : ((b >> (7 - (i - 24))) & 1);
        word |= (bit ? 0x80000000 : 0);
        if (i > 0) word >>= 1;
    }
    return word;
}

static void _rmt_tx_done_cb(rmt_channel_handle_t ch, rmt_tx_done_event_data_t *edata, void *user_ctx)
{
    (void)ch; (void)user_ctx;
}

void rgb_init(void)
{
    ESP_LOGI(TAG, "rgb init GPIO %d, %d LEDs", RGB_GPIO, RGB_LED_NUM);

    rmt_tx_channel_config_t ch_cfg = {
        .gpio_num = RGB_GPIO,
        .clk_src = RMT_CLK_SRC_APB,
        .resolution_hz = 10000000, // 10MHz = 100ns per tick
        .mem_block_symbols = 64,
        .trans_queue_depth = 1,
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&ch_cfg, &s_rmt_ch));

    ws2812_encoder_config_t enc_cfg = {
        .bit0 = {.duration0 = 1, .level0 = 0, .duration1 = 2, .level1 = 1},
        .bit1 = {.duration0 = 2, .level0 = 0, {.duration1 = 1, .level1 = 1}},
        .flags = {.inverse_out = 0, .msb_first = 1},
    };
    ESP_ERROR_CHECK(rmt_new_ws2812_encoder(&enc_cfg, &s_ws2812_enc));

    rmt_tx_event_callbacks_t cbs = {.on_trans_done = _rmt_tx_done_cb};
    ESP_ERROR_CHECK(rmt_tx_register_event_callbacks(s_rmt_ch, &cbs, NULL));
    ESP_ERROR_CHECK(rmt_enable(s_rmt_ch));
}

void rgb_set_color(uint32_t color)
{
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8)  & 0xFF;
    uint8_t b =  color        & 0xFF;
    uint32_t grb_word = _ws2812_bitmark(r, g, b);
    uint32_t grb_buf[RGB_LED_NUM];
    for (int i = 0; i < RGB_LED_NUM; i++) grb_buf[i] = grb_word;
    rmt_transmit_config_t tx_conf = {.loop_count = 0};
    ESP_ERROR_CHECK(rmt_transmit(s_rmt_ch, s_ws2812_enc, grb_buf, sizeof(grb_buf), &tx_conf));
}

void rgb_set_pixel(int idx, uint32_t color)
{
    if (idx < 0 || idx >= RGB_LED_NUM) return;
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8)  & 0xFF;
    uint8_t b =  color        & 0xFF;
    uint32_t grb_word = _ws2812_bitmark(r, g, b);
    rmt_transmit_config_t tx_conf = {.loop_count = 0};
    ESP_ERROR_CHECK(rmt_transmit(s_rmt_ch, s_ws2812_enc, &grb_word, 4, &tx_conf));
}

void rgb_demo(void)
{
    const uint32_t colors[] = {0xFF0000, 0x00FF00, 0x0000FF, 0xFFFF00, 0xFF00FF, 0x00FFFF, 0xFFFFFF};
    for (int i = 0; i < 7; i++) {
        rgb_set_color(colors[i]);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    rgb_set_color(0);
}