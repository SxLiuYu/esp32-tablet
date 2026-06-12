/* LVGL UI for ESP32 tablet
 * Classic ESP32 no PSRAM: minimal labels, no large textures
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
static lv_obj_t *s_waveform_img;

void ui_init(void)
{
    ESP_LOGI(TAG, "ui init");

    s_screen = lv_obj_create(NULL);

    static lv_style_t style;
    lv_style_init(&style);
    lv_style_set_bg_color(&style, LV_STATE_DEFAULT, LV_COLOR_BLACK);
    lv_style_set_text_color(&style, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_add_style(s_screen, LV_STATE_DEFAULT, &style);

    s_state_label = lv_label_create(s_screen, NULL);
    lv_label_set_text(s_state_label, "yuanfang\nvoice");
    lv_obj_align(s_state_label, LV_ALIGN_CENTER, 0, -60);
    lv_obj_set_style_text_font(s_state_label, LV_STATE_DEFAULT, &lv_font_montserrat_18);

    s_bat_label = lv_label_create(s_screen, NULL);
    lv_label_set_text(s_bat_label, "BAT: --%");
    lv_obj_align(s_bat_label, LV_ALIGN_TOP_RIGHT, -5, 5);
    lv_obj_set_style_text_font(s_bat_label, LV_STATE_DEFAULT, &lv_font_montserrat_12);

    s_reply_label = lv_label_create(s_screen, NULL);
    lv_label_set_text(s_reply_label, "");
    lv_obj_align(s_reply_label, LV_ALIGN_CENTER, 0, 20);
    lv_label_set_recolor(s_reply_label, true);

    lv_scr_load(s_screen);
    ESP_LOGI(TAG, "LVGL UI ready");
}

void ui_set_state(ui_state_t state)
{
    s_state = state;
    const char *txt = "";
    lv_color_t txt_color = LV_COLOR_WHITE;
    switch (state) {
        case UI_STATE_IDLE:      txt = "yuanfang\nvoice"; break;
        case UI_STATE_LISTENING: txt = "Listening..."; txt_color = LV_COLOR_CYAN; break;
        case UI_STATE_THINKING:  txt = "Thinking..."; txt_color = LV_COLOR_YELLOW; break;
        case UI_STATE_REPLY:     txt = "Replying..."; txt_color = LV_COLOR_GREEN; break;
        case UI_STATE_READY:     txt = "Ready"; txt_color = LV_COLOR_GREEN; break;
        case UI_STATE_ERROR:     txt = "Error"; txt_color = LV_COLOR_RED; break;
        case UI_STATE_AP_MODE:   txt = "AP Mode\n192.168.4.1"; txt_color = LV_COLOR_ORANGE; break;
        default: break;
    }
    lv_label_set_text(s_state_label, txt);
    lv_obj_set_style_text_color(s_state_label, LV_STATE_DEFAULT, txt_color);
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