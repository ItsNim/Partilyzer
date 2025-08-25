#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "st7789.h"
#include "fontx.h"

#define TAG "APP"

#include "esp_spiffs.h"

#include "esp_log.h"
#include "i2chelper.h"

esp_err_t wifi_connect_start(const char *ssid_opt, const char *pass_opt, TickType_t wait_ticks);

// A new, slow, but reliable function to draw a BMP pixel-by-pixel
void display_bmp_slowly(TFT_t *dev, const char *filename) {
    FILE *f = fopen(filename, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file %s", filename);
        return;
    }

    uint8_t header[54];
    fread(header, sizeof(uint8_t), 54, f);

    uint32_t dataOffset = *(uint32_t*)&header[10];
    uint32_t width = *(uint32_t*)&header[18];
    uint32_t height = *(uint32_t*)&header[22];

    ESP_LOGI(TAG, "Drawing BMP %"PRIu32"x%"PRIu32" pixel by pixel...", width, height);
    
    // Move the file pointer to the start of the pixel data
    fseek(f, dataOffset, SEEK_SET);

    // Calculate padding (BMP rows are padded to a multiple of 4 bytes)
    int padding = (4 - (width * 3) % 4) % 4;
    
    uint8_t bgr_pixel[3]; // Buffer to hold one BGR pixel

    // BMPs are stored bottom-up
    for (int y = height - 1; y >= 0; y--) {
        for (int x = 0; x < width; x++) {
            // Read one pixel (3 bytes: B, G, R)
            fread(bgr_pixel, 3, 1, f);

            // Convert 24-bit BGR to 16-bit RGB565
            uint8_t b = bgr_pixel[0];
            uint8_t g = bgr_pixel[1];
            uint8_t r = bgr_pixel[2];
            uint16_t color565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);

            // Draw the single pixel
            lcdDrawPixel(dev, x, y, color565);
        }
        // Skip any padding at the end of the row
        fseek(f, padding, SEEK_CUR);
    }

    fclose(f);
    ESP_LOGI(TAG, "Finished drawing pixel by pixel.");
}

void display_bmp_fast(TFT_t *dev, const char *filename) {
    // ... (file opening and header reading code is the same) ...
    FILE *f = fopen(filename, "r");
    if (f == NULL) { /* handle error */ return; }
    uint8_t header[54];
    fread(header, sizeof(uint8_t), 54, f);
    uint32_t dataOffset = *(uint32_t*)&header[10];
    uint32_t width = *(uint32_t*)&header[18];
    uint32_t height = *(uint32_t*)&header[22];
    uint32_t rowSize = (width * 3 + 3) & ~3;
    uint8_t *bgr_buffer = malloc(rowSize);
    uint16_t *rgb565_buffer = malloc(width * sizeof(uint16_t));
    if (!bgr_buffer || !rgb565_buffer) { /* handle error */ return; }

    for (int y = height - 1; y >= 0; y--) {
        fseek(f, dataOffset + y * rowSize, SEEK_SET);
        fread(bgr_buffer, sizeof(uint8_t), rowSize, f);

        for (int x = 0; x < width; x++) {
            uint8_t b = bgr_buffer[x * 3];
            uint8_t g = bgr_buffer[x * 3 + 1];
            uint8_t r = bgr_buffer[x * 3 + 2];
            
            // --- CHANGE IS HERE ---
            // We are NOT swapping the bytes anymore
            uint16_t color565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
            rgb565_buffer[width-1-x] = color565;
        }

        lcdDrawMultiPixels(dev, 0, y, width, rgb565_buffer);
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }

    free(bgr_buffer);
    free(rgb565_buffer);
    fclose(f);
}

void init_spiffs(void) {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage1",  // matches partitions.csv
        .max_files = 5,
        .format_if_mount_failed = true
    };
    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&conf));
}

#define FONT_W 8
#define FONT_H 16

static void draw_line(TFT_t *dev, FontxFile *fx,
                      int x, int baseline_y,
                      const char *s, uint16_t fg, uint16_t bg) {
    int y_top = baseline_y - (FONT_H - 1);
    if (y_top < 0) y_top = 0;

    // clear the whole band for this line
    //lcdDrawFillRect(dev, 0, y_top, CONFIG_WIDTH - 1, y_top + FONT_H - 1, bg);

    // draw the fresh text
    lcdDrawString(dev, fx, x, baseline_y, (uint8_t*)s, fg);

}


void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(10000));
    init_spiffs();
    TFT_t dev;
    FontxFile fx[2];

    ESP_LOGI(TAG, "ST7789 Hello World with Fontx");

    // Initialize SPI and LCD (pins set in menuconfig)
    spi_master_init(&dev,
                    CONFIG_MOSI_GPIO,
                    CONFIG_SCLK_GPIO,
                    CONFIG_CS_GPIO,
                    CONFIG_DC_GPIO,
                    CONFIG_RESET_GPIO,
                    CONFIG_BL_GPIO);
    spi_clock_speed(80000000);

    lcdInit(&dev, CONFIG_WIDTH, CONFIG_HEIGHT, 0, 80);
    lcdFillScreen(&dev, BLACK);

    lcdInit(&dev, CONFIG_WIDTH, CONFIG_HEIGHT, 0, 0);
    lcdFillScreen(&dev, BLACK);

    lcdInversionOff(&dev);
    // Clear screen

    // Load the font file (from your project directory)
    InitFontx(fx, "/spiffs/ILGH16XB.FNT", "");
    lcdSetFontDirection(&dev, 0);
    lcdSetFontFill(&dev, true);           // fill behind each glyph

    vTaskDelay(pdMS_TO_TICKS(1200));  // match sensor cadence


    //lcdSetFontFillColor(&dev, BLACK);     // background color

    // Set text direction and draw string
    //lcdSetFontDirection(&dev, 0);
    //display_bmp_fast(&dev, "/spiffs/mockup.bmp");
    ESP_LOGI(TAG, "Done. Text drawn!");
    ESP_ERROR_CHECK(i2c_helper_init());
    ESP_ERROR_CHECK(ens160_init_sensor());

static const char *validity_strs[4] = {"normal","warm-up","initial","invalid"};
//int last_vals[ENS160_VALS_COUNT] = {-1,-1,-1,-1};
//static const char *validity_strs[4] = {"normal","warm-up","initial","invalid"};

int last_ens[ENS160_VALS_COUNT] = {-1,-1,-1,-1};
int last_scd[SCD40_VALS_COUNT]   = {-1,-1,-1};
TickType_t next_scd = 0;  // schedule next SCD40 poll

// Wait up to 20 seconds for IP
ESP_ERROR_CHECK( wifi_connect_start(NULL, NULL, pdMS_TO_TICKS(20000)) );

ESP_ERROR_CHECK(scd40_init());   // SCD40 begins periodic measurements (~5 s cadence)

while (1) {
    int vals[ENS160_VALS_COUNT] = {0};
    uint8_t buf[32];
    TickType_t now = xTaskGetTickCount(); 
    if (ens160_read_values(vals) == ESP_OK) {
        if (vals[ENS160_VAL_AQI] != last_ens[ENS160_VAL_AQI]) {
            snprintf((char*)buf, sizeof(buf), "AQI: %d     ", vals[ENS160_VAL_AQI]);
            draw_line(&dev, fx, 0, 0, (char*)buf, WHITE, BLACK);
            last_ens[ENS160_VAL_AQI] = vals[ENS160_VAL_AQI];
        }
        if (vals[ENS160_VAL_TVOC_PPB] != last_ens[ENS160_VAL_TVOC_PPB]) {
            snprintf((char*)buf, sizeof(buf), "TVOC: %d ppb", vals[ENS160_VAL_TVOC_PPB]);
            draw_line(&dev, fx, 0, 16, (char*)buf, WHITE, BLACK);
            last_ens[ENS160_VAL_TVOC_PPB] = vals[ENS160_VAL_TVOC_PPB];
        }
        if (vals[ENS160_VAL_ECO2_PPM] != last_ens[ENS160_VAL_ECO2_PPM]) {
            snprintf((char*)buf, sizeof(buf), "eCO2: %d ppm", vals[ENS160_VAL_ECO2_PPM]);
            draw_line(&dev, fx, 0, 32, (char*)buf, WHITE, BLACK);
            last_ens[ENS160_VAL_ECO2_PPM] = vals[ENS160_VAL_ECO2_PPM];
        }
        if (vals[ENS160_VAL_VALIDITY] != last_ens[ENS160_VAL_VALIDITY]) {
            snprintf((char*)buf, sizeof(buf), "Status: %s",
                     validity_strs[vals[ENS160_VAL_VALIDITY] & 3]);
            draw_line(&dev, fx, 0, 48, (char*)buf, WHITE, BLACK);
            last_ens[ENS160_VAL_VALIDITY] = vals[ENS160_VAL_VALIDITY];
        }
    } else {
        draw_line(&dev, fx, 0, 0, "ENS160 read err", WHITE, BLACK);
    }

    vTaskDelay(pdMS_TO_TICKS(1200));  // match sensor cadence
    // ----- SCD40 (~5.1s) -----
    if ((int32_t)(now - next_scd) >= 0) {
        int s[SCD40_VALS_COUNT] = {0};
        if (scd40_read_values(s) == ESP_OK) {
            char buf[40];

            if (s[SCD40_VAL_CO2_PPM] != last_scd[SCD40_VAL_CO2_PPM]) {
                snprintf(buf, sizeof(buf), "CO2: %d ppm     ", s[SCD40_VAL_CO2_PPM]);
                draw_line(&dev, fx, 0, 80, buf, WHITE, BLACK);
                last_scd[SCD40_VAL_CO2_PPM] = s[SCD40_VAL_CO2_PPM];
            }
            if (s[SCD40_VAL_TEMP_C_X100] != last_scd[SCD40_VAL_TEMP_C_X100]) {
                int t = s[SCD40_VAL_TEMP_C_X100];
                snprintf(buf, sizeof(buf), "T: %d.%02d C", t/100, abs(t)%100);
                draw_line(&dev, fx, 0, 96, buf, WHITE, BLACK);
                last_scd[SCD40_VAL_TEMP_C_X100] = s[SCD40_VAL_TEMP_C_X100];
            }
            if (s[SCD40_VAL_RH_X100] != last_scd[SCD40_VAL_RH_X100]) {
                int rh = s[SCD40_VAL_RH_X100];
                snprintf(buf, sizeof(buf), "RH: %d.%02d %%", rh/100, rh%100);
                draw_line(&dev, fx, 0, 112, buf, WHITE, BLACK);
                last_scd[SCD40_VAL_RH_X100] = s[SCD40_VAL_RH_X100];
            }
        } else {
            draw_line(&dev, fx, 0, 79, "SCD40 read err", WHITE, BLACK);
        }
        next_scd = now + pdMS_TO_TICKS(5100);
    }

}
}
