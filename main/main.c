/* yuanfang-brain ESP32 Voice Frontend
 * Hardware: WROOM-32 (520KB SRAM, no PSRAM) + 1.54" ST7789 240x240 SPI
 * Protocol: ws://192.168.1.10:7103/ws/audio
 *
 * Modified 2026-06-14: include esp_chip_info.h, drop .revision (removed in v5.3.2)
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_chip_info.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"

#include "wifi.h"
#include "ws_client.h"
#include "audio.h"
#include "display.h"
#include "keypad.h"
#include "rgb.h"
#include "power.h"
#include "ui.h"

static const char *TAG = "main";

static void led_effect(uint32_t color, uint32_t ms)
{
    rgb_set_color(color);
    vTaskDelay(pdMS_TO_TICKS(ms));
    rgb_set_color(0);
}

static void button_handler(int key, int pressed)
{
    if (!pressed) return;
    ESP_LOGI(TAG, "key %d pressed", key);
    switch (key) {
        case KEY_BOOT:
            rgb_set_color(0x0000FF);
            ui_set_state(UI_STATE_LISTENING);
            ws_client_send_audio_start();
            break;
        case KEY_VOL_UP:
            rgb_set_color(0x00FF00);
            break;
        case KEY_VOL_DOWN:
            rgb_set_color(0xFF0000);
            break;
        case KEY_POW:
            rgb_set_color(0xFFFF00);
            break;
        case KEY_RGB:
            rgb_demo();
            break;
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== yuanfang-brain ESP32 Firmware ===");
    esp_chip_info_t chip;
    esp_chip_info(&chip);
    /* v5.3.2: chip.revision field removed - use esp_chip_info() to print via feature flags */
    ESP_LOGI(TAG, "Chip: %s, cores=%d, features=0x%08lx",
             CONFIG_IDF_TARGET, chip.cores, (unsigned long)chip.features);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    power_init();
    rgb_init();
    display_init();
    keypad_init(button_handler);
    audio_init();

    ui_init();
    ui_set_state(UI_STATE_IDLE);

    led_effect(0x0000FF, 200);

    wifi_init_sta();

    uint8_t retries = 0;
    while (!wifi_is_connected() && retries < 30) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        retries++;
    }

    if (wifi_is_connected()) {
        led_effect(0x00FF00, 300);
        ws_client_init();
        ui_set_state(UI_STATE_READY);
    } else {
        led_effect(0xFF0000, 500);
        ESP_LOGW(TAG, "WiFi not connected, starting AP mode...");
        wifi_init_softap();
        ui_set_state(UI_STATE_AP_MODE);
    }

    ESP_LOGI(TAG, "Main loop running");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100));
        keypad_process();
        power_check_sleep();
    }
}
