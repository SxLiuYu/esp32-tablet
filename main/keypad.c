#include "keypad.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/gpio.h"

#define NUM_KEYS 5
static const gpio_num_t s_key_gpios[NUM_KEYS] = {
    GPIO_NUM_35, // VOL-
    GPIO_NUM_0,  // POW
    GPIO_NUM_39, // BOOT (EN)
    GPIO_NUM_27, // RGB
    GPIO_NUM_32, // VOL+
};
static const int s_key_ids[NUM_KEYS] = {
    KEY_VOL_DOWN, KEY_POW, KEY_BOOT, KEY_RGB, KEY_VOL_UP
};

static QueueHandle_t s_evt_q;
static void (*s_handler)(int, int) = NULL;
static volatile int s_key_state[NUM_KEYS] = {0};

static void _key_isr(void *arg)
{
    int key_idx = (int)arg;
    int lvl = gpio_get_level(s_key_gpios[key_idx]);
    int pressed = (lvl == 0); // active low
    xQueueSendFromISR(s_evt_q, (void *)&(int){key_idx * 2 + pressed}, NULL);
    (void)pressed;
}

void keypad_init(void (*handler)(int, int))
{
    s_handler = handler;
    s_evt_q = xQueueCreate(16, sizeof(int));

    for (int i = 0; i < NUM_KEYS; i++) {
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << s_key_gpios[i]),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = (s_key_gpios[i] != GPIO_NUM_35 && s_key_gpios[i] != GPIO_NUM_39)
                          ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_ANYEDGE,
        };
        gpio_config(&io_conf);
        gpio_isr_handler_add(s_key_gpios[i], _key_isr, (void *)i);
    }
    ESP_LOGI(TAG, "keypad init %d keys", NUM_KEYS);
}

void keypad_process(void)
{
    int msg;
    while (xQueueReceive(s_evt_q, &msg, 0) == pdTRUE) {
        int key_idx = msg / 2;
        int pressed = msg % 2;
        if (key_idx >= 0 && key_idx < NUM_KEYS && s_handler) {
            s_handler(s_key_ids[key_idx], pressed);
        }
    }
}