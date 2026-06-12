/* Battery monitoring via ADC on GPIO 34 (battery voltage divider)
 * Sleep/wake handling
 */
#include "power.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_sleep.h"
#include "driver/gpio.h"

static const char *TAG = "power";
#define ADC_BAT GPIO_NUM_34
#define ADC_ATTEN ADC_ATTEN_DB_11

static adc_oneshot_unit_handle_t s_adc_h;
static int s_battery_pct = 100;
static uint64_t s_last_activity_tick = 0;

void power_init(void)
{
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &s_adc_h));
    adc_oneshot_chan_cfg_t ch_cfg = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc_h, ADC_BAT, &ch_cfg));
    ESP_LOGI(TAG, "power init");
}

int power_get_battery_pct(void)
{
    int raw = 0;
    adc_oneshot_read(s_adc_h, ADC_BAT, &raw);
    // Voltage divider: battery -> 100k+100k -> ADC
    // Full: 4.2V -> raw~4095; Empty: 3.0V -> raw~2920
    int pct = ((raw - 2920) * 100) / (4095 - 2920);
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    s_battery_pct = pct;
    return pct;
}

void power_check_sleep(void)
{
    // Auto-sleep after 5 minutes of inactivity if charging
    uint64_t now = xTaskGetTickCount();
    if (now - s_last_activity_tick > 30000) { // 5 min @ 10ms ticks
        int bat = power_get_battery_pct();
        if (bat < 5) {
            ESP_LOGI(TAG, "battery low, entering deep sleep");
            esp_deep_sleep_start();
        }
    }
}