/* WS2812B RGB LED strip - stub for tablet (RMT+WS2812 encoder too fragile in v5.3.2)
 * We disable the actual RMT init; rgb_set_color() just no-ops.
 * To enable later: switch to esp_driver_led_strip or pin RMT tx config correctly.
 */
#include "rgb.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"

static const char *TAG = "rgb";

void rgb_init(void)
{
    ESP_LOGW(TAG, "rgb init skipped (RMT+WS2812 encoder not stable in IDF v5.3.2; stub mode)");
}

void rgb_set_color(uint32_t color)
{
    (void)color;
}

void rgb_set_pixel(int idx, uint32_t color)
{
    (void)idx; (void)color;
}

void rgb_demo(void)
{
    /* no-op */
}
