/* LVGL UI for ESP32 tablet
 * Classic ESP32 no PSRAM: minimal labels, no large textures
 * LVGL v9 (IDF v5.3.2)
 *
 * Modified 2026-06-14: port to LVGL v9 API (no LV_STATE_DEFAULT, lv_color_black() fn, etc.)
 */
#include "ui.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "lvgl.h"

static const char *TAG = "ui";
static ui_state_t s_state = UI_STATE_IDLE;

static lv_obj_t *s_screen;
static lv_obj_t *s_state_label;
static lv_obj_t *s_bat_label;
static lv_obj_t *s_reply_label;

void ui_init(void)
{
    ESP_LOGI(TAG, "ui init");

    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_black(), 0);

    s_state_label = lv_label_create(s_screen);
    lv_label_set_text(s_state_label, "yuanfang\nvoice");
    lv_obj_align(s_state_label, LV_ALIGN_CENTER, 0, -60);
    lv_obj_set_style_text_font(s_state_label, &lv_font_montserrat_14, 0);

    s_bat_label = lv_label_create(s_screen);
    lv_label_set_text(s_bat_label, "BAT: --%");
    lv_obj_align(s_bat_label, LV_ALIGN_TOP_RIGHT, -5, 5);
    lv_obj_set_style_text_font(s_bat_label, &lv_font_montserrat_14, 0);

    s_reply_label = lv_label_create(s_screen);
    lv_label_set_text(s_reply_label, "");
    lv_obj_align(s_reply_label, LV_ALIGN_CENTER, 0, 20);
    lv_label_set_recolor(s_reply_label, true);

    lv_screen_load(s_screen);
    ESP_LOGI(TAG, "LVGL UI ready");
}

void ui_set_state(ui_state_t state)
{
    s_state = state;
    const char *txt = "";
    lv_color_t txt_color = lv_color_white();
    switch (state) {
        case UI_STATE_IDLE:      txt = "yuanfang\nvoice"; break;
        case UI_STATE_LISTENING: txt = "Listening..."; txt_color = lv_color_make(0x00, 0xCC, 0xCC); break;
        case UI_STATE_THINKING:  txt = "Thinking...";  txt_color = lv_color_make(0xFF, 0xCC, 0x00); break;
        case UI_STATE_REPLY:     txt = "Replying...";   txt_color = lv_color_make(0x00, 0xFF, 0x00); break;
        case UI_STATE_READY:     txt = "Ready";         txt_color = lv_color_make(0x00, 0xFF, 0x00); break;
        case UI_STATE_ERROR:     txt = "Error";         txt_color = lv_color_make(0xFF, 0x00, 0x00); break;
        case UI_STATE_AP_MODE:   txt = "AP Mode\n192.168.4.1"; txt_color = lv_color_make(0xFF, 0x80, 0x00); break;
        default: break;
    }
    lv_label_set_text(s_state_label, txt);
    lv_obj_set_style_text_color(s_state_label, txt_color, 0);
}

void ui_show_reply(const char *text)
{
    if (!text) return;
    static char disp[256];
    snprintf(disp, sizeof(disp), "#00FF00 %s", text);
    lv_label_set_text(s_reply_label, disp);
    vTaskDelay(pdMS_TO_TICKS(3000));
    lv_label_set_text(s_reply_label, "");
}
