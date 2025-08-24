#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "st7789.h"

#define TAG "BMP_APP"

// Your init_spiffs() function (unchanged)
void init_spiffs(void) {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage1",
        .max_files = 5,
        .format_if_mount_failed = true
    };
    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&conf));
}

// app_main is now simplified to call the new function
void app_main(void) {
    init_spiffs();
    TFT_t dev;

    ESP_LOGI(TAG, "Initializing ST7789 display...");
    spi_master_init(&dev,
                    CONFIG_MOSI_GPIO,
                    CONFIG_SCLK_GPIO,
                    CONFIG_CS_GPIO,
                    CONFIG_DC_GPIO,
                    CONFIG_RESET_GPIO,
                    CONFIG_BL_GPIO);
    lcdInit(&dev, CONFIG_WIDTH, CONFIG_HEIGHT, 0, 0);
    lcdBacklightOn(&dev); 
    lcdFillScreen(&dev, BLACK);

    // Call the new, slow drawing function
    //display_bmp_slowly(&dev, "/spiffs/mockup.bmp");

    ESP_LOGI(TAG, "Done.");
}
