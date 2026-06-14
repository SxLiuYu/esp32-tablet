/* ST7789 240x240 SPI 1.54" display driver
 * Classic ESP32: framebuffer in internal RAM (no PSRAM)
 * Color depth: RGB565
 * LVGL used for rendering
 */
#include "display.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "lvgl.h"
#include "driver/gpio.h"

static const char *TAG = "display";

#define LCD_HOST HSPI_HOST
#define LCD_PIN_MOSI 23
#define LCD_PIN_CLK  18
#define LCD_PIN_CS   5
#define LCD_PIN_DC   21
#define LCD_PIN_RST  22

/* LVGL display buffer - half screen (120x240x2 = 57600 bytes) to save memory */
static lv_draw_buf_t s_disp_buf;
static uint16_t s_fb[240 * 120] __attribute__((section(".dram1.data"))); // 57.6KB

/* Minimal ST7789 send_color using spi_master */
static spi_device_handle_t s_spi;

static void _st7789_send_cmd(uint8_t cmd)
{
    spi_transaction_t t = {
        .length = 8,
        .tx_data = {cmd},
        .flags = SPI_TRANS_USE_TXDATA,
    };
    gpio_set_level(LCD_PIN_DC, 0);
    spi_device_transmit(s_spi, &t);
}

static void _st7789_send_data(const uint8_t *data, size_t len)
{
    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = data,
    };
    gpio_set_level(LCD_PIN_DC, 1);
    spi_device_transmit(s_spi, &t);
}

static void _st7789_init(void)
{
    spi_bus_config_t bus = {
        .mosi_io_num = LCD_PIN_MOSI,
        .sclk_io_num = LCD_PIN_CLK,
        .max_transfer_sz = 256,
    };
    spi_device_interface_config_t dev = {
        .clock_speed_hz = 40000000,
        .mode = 0,
        .spics_io_num = LCD_PIN_CS,
        .queue_size = 1,
    };
    spi_bus_initialize(LCD_HOST, &bus, SPI_DMA_CH_AUTO);
    spi_bus_add_device(LCD_HOST, &dev, &s_spi);

    gpio_set_direction(LCD_PIN_DC, GPIO_MODE_OUTPUT);
    gpio_set_direction(LCD_PIN_RST, GPIO_MODE_OUTPUT);

    gpio_set_level(LCD_PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(LCD_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(120));

    _st7789_send_cmd(0x01); vTaskDelay(pdMS_TO_TICKS(120));
    _st7789_send_cmd(0x11); vTaskDelay(pdMS_TO_TICKS(10));
    _st7789_send_cmd(0x21);
    _st7789_send_cmd(0x2A); _st7789_send_data((uint8_t[]){0,0,0,239}, 4);
    _st7789_send_cmd(0x2B); _st7789_send_data((uint8_t[]){0,0,0,239}, 4);
    _st7789_send_cmd(0x36); _st7789_send_data((uint8_t[]){0x00}, 1);
    _st7789_send_cmd(0x3A); _st7789_send_data((uint8_t[]){0x55}, 1); // RGB565
    _st7789_send_cmd(0xB2); _st7789_send_data((uint8_t[]){0x0C,0x0C,0x00,0x33,0x33}, 5);
    _st7789_send_cmd(0xB7); _st7789_send_data((uint8_t[]){0x35}, 1);
    _st7789_send_cmd(0xBB); _st7789_send_data((uint8_t[]){0x28}, 1);
    _st7789_send_cmd(0xC0); _st7789_send_data((uint8_t[]){0x2C}, 1);
    _st7789_send_cmd(0xC2); _st7789_send_data((uint8_t[]){0x01}, 1);
    _st7789_send_cmd(0xC3); _st7789_send_data((uint8_t[]){0x19}, 1);
    _st7789_send_cmd(0xC4); _st7789_send_data((uint8_t[]){0x20}, 1);
    _st7789_send_cmd(0xC6); _st7789_send_data((uint8_t[]){0x01}, 1);
    _st7789_send_cmd(0xD0); _st7789_send_data((uint8_t[]){0xA4,0xA1}, 2);
    _st7789_send_cmd(0x21);
    _st7789_send_cmd(0x29);
    ESP_LOGI(TAG, "ST7789 init done");
}

static void _lv_flush_cb(lv_display_t *drv, const lv_area_t *area, lv_color16_t *color_p)
{
    int x1 = area->x1, x2 = area->x2;
    int y1 = area->y1, y2 = area->y2;

    _st7789_send_cmd(0x2A);
    _st7789_send_data((uint8_t[]){x1>>8, x1&0xFF, x2>>8, x2&0xFF}, 4);
    _st7789_send_cmd(0x2B);
    _st7789_send_data((uint8_t[]){y1>>8, y1&0xFF, y2>>8, y2&0xFF}, 4);
    _st7789_send_cmd(0x2C);

    size_t len = (x2 - x1 + 1) * (y2 - y1 + 1) * 2;
    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = color_p,
    };
    gpio_set_level(LCD_PIN_DC, 1);
    spi_device_transmit(s_spi, &t);
    lv_display_flush_ready(drv);
}

void display_init(void)
{
    ESP_LOGI(TAG, "display init ST7789 240x240");
    _st7789_init();
    lv_init();
    lv_draw_buf_init(&s_disp_buf, s_fb, NULL, 240 * 120);

    static lv_display_t disp_drv;
    lv_display_init(&disp_drv);
    lv_display_set_hor_res(&disp_drv, 240); (void)0; // hor_res = 240;
    lv_display_set_ver_res(&disp_drv, 240); (void)0; // ver_res = 240;
    lv_display_set_flush_cb(&disp_drv, _lv_flush_cb);
    lv_display_set_draw_buffers(&disp_drv, &s_disp_buf, NULL);
    lv_display_register(&disp_drv);

    ESP_LOGI(TAG, "LVGL display registered (57KB fb, half-screen)");
}