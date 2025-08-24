// wifi_basic.c
// Minimal Wi-Fi STA bring-up for ESP-IDF (v4.x/v5.x)

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"

static const char *TAG = "WIFI";

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

#ifndef CONFIG_WIFI_MAX_RETRY
#define CONFIG_WIFI_MAX_RETRY 5
#endif

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;

static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < CONFIG_WIFI_MAX_RETRY) {
            s_retry_num++;
            ESP_LOGW(TAG, "WiFi disconnected, retry %d/%d", s_retry_num, CONFIG_WIFI_MAX_RETRY);
            esp_wifi_connect();
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        char ip[16];
        esp_ip4addr_ntoa(&event->ip_info.ip, ip, sizeof(ip));
        ESP_LOGI(TAG, "Got IP: %s  (gw:%s mask:%s)",
                 ip,
                 esp_ip4addr_ntoa(&event->ip_info.gw, (char[16]){0}, 16),
                 esp_ip4addr_ntoa(&event->ip_info.netmask, (char[16]){0}, 16));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// Call this once early (it will init NVS if needed), start Wi-Fi, and block until IP or fail.
// Pass ssid/pass or NULL to use CONFIG_WIFI_SSID / CONFIG_WIFI_PASSWORD.
// Returns ESP_OK on connection success.
esp_err_t wifi_connect_start(const char *ssid_opt, const char *pass_opt, TickType_t wait_ticks)
{
    // Ensure NVS is ready (required by Wi-Fi)
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_t *sta = esp_netif_create_default_wifi_sta();
    (void)sta; // not used further, but keep for clarity

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL, NULL));

    wifi_config_t wifi_config = { 0 };

    const char *ssid = (ssid_opt && ssid_opt[0]) ? ssid_opt : CONFIG_WIFI_SSID;
    const char *pass = (pass_opt && pass_opt[0]) ? pass_opt : CONFIG_WIFI_PASSWORD;

    // Copy SSID/PASS safely
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, pass, sizeof(wifi_config.sta.password) - 1);

    // Recommended auth threshold (WPA2 and up); set to WIFI_AUTH_OPEN if you need open APs
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi STA starting… SSID:\"%s\"", (const char*)wifi_config.sta.ssid);

    // Wait for connected or fail
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdTRUE, pdFALSE,
                                           wait_ticks);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Wi-Fi connected.");
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Wi-Fi failed to connect after %d retries", CONFIG_WIFI_MAX_RETRY);
        return ESP_FAIL;
    } else {
        ESP_LOGW(TAG, "Wi-Fi wait timed out.");
        return ESP_ERR_TIMEOUT;
    }
}
