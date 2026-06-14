/* Battery monitoring via ADC on GPIO 34 (battery voltage divider)
 * Sleep/wake handling
 *
 * Modified 2026-06-14: soft-fail on ADC init (board may not have ADC wired)
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

static adc_oneshot_unit_handle_t s_adc_h = NULL;
static int s_battery_pct = 100;
static uint64_t s_last_activity_tick = 0;
static bool s_power_ready = false;

void power_init(void)
{
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    esp_err_t err = adc_oneshot_new_unit(&init_cfg, &s_adc_h);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "adc_oneshot_new_unit failed: %s - power monitoring disabled", esp_err_to_name(err));
        return;
    }
    adc_oneshot_chan_cfg_t ch_cfg = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN,
    };
    err = adc_oneshot_config_channel(s_adc_h, ADC_BAT, &ch_cfg);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "adc_oneshot_config_channel failed: %s", esp_err_to_name(err));
        return;
    }
    s_power_ready = true;
    ESP_LOGI(TAG, "power init OK");
}

int power_get_battery_pct(void)
{
    if (!s_power_ready) return 100;
    int raw = 0;
    if (adc_oneshot_read(s_adc_h, ADC_BAT, &raw) != ESP_OK) return 100;
    int pct = ((raw - 2920) * 100) / (4095 - 2920);
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    s_battery_pct = pct;
    return pct;
}

void power_check_sleep(void)
{
    uint64_t now = xTaskGetTickCount();
    if (now - s_last_activity_tick > 30000) {
        int bat = power_get_battery_pct();
        if (bat < 5) {
            ESP_LOGI(TAG, "battery low, entering deep sleep");
            esp_deep_sleep_start();
        }
    }
}
