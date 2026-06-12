#include "wifi.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "lwip/netif.h"

static const char *TAG = "wifi";
static EventGroupHandle_t s_wifi_evg;
static int s_connected = 0;
static int s_got_ip = 0;

#define WIFI_CONNECTED_BIT (BIT0)
#define WIFI_FAIL_BIT      (BIT1)
#define MAX_RETRY 10

static int s_retry_cnt = 0;

static void _event_handler(void *arg, esp_event_base_t evb, int32_t evid, void *data)
{
    if (evb == WIFI_EVENT && evid == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (evb == WIFI_EVENT && evid == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_cnt < MAX_RETRY) {
            esp_wifi_connect();
            s_retry_cnt++;
            ESP_LOGW(TAG, "retry %d/%d", s_retry_cnt, MAX_RETRY);
        } else {
            xEventGroupSetBits(s_wifi_evg, WIFI_FAIL_BIT);
        }
    } else if (evb == IP_EVENT && evid == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "got ip: " IPSTR, IP2STR(&event->ip_info.ip));
        s_got_ip = 1;
        s_connected = 1;
        xEventGroupSetBits(s_wifi_evg, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{
    s_wifi_evg = xEventGroupCreate();
    s_connected = 0;
    s_got_ip = 0;
    s_retry_cnt = 0;

    wifi_config_t cfg = {0};
    // Credentials loaded from nvs - placeholder so it compiles
    strncpy((char *)cfg.sta.ssid, "SSID_PLACEHOLDER", sizeof(cfg.sta.ssid) - 1);
    strncpy((char *)cfg.sta.password, "PASS_PLACEHOLDER", sizeof(cfg.sta.password) - 1);
    cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    cfg.sta.pmf_cfg.capable = 1;
    cfg.sta.pmf_cfg.required = 0;

    ESP_ERROR_CHECK(esp_netif_create_default_wifi_sta());
    wifi_init_config_t icfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&icfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, _event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, _event_handler, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "wifi sta start");
}

int wifi_is_connected(void)
{
    return s_connected && s_got_ip;
}

void wifi_init_softap(void)
{
    ESP_ERROR_CHECK(esp_netif_create_default_wifi_ap());
    wifi_config_t cfg = {
        .ap = {
            .ssid = "ESP32-TABLET",
            .ssid_len = 12,
            .password = "12345678",
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA2_PSK
        }
    };
    wifi_init_config_t icfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&icfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "wifi ap started: ESP32-TABLET / 12345678");
}