#pragma once
#include "esp_err.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include <stddef.h>

// ESP-IDF 5.x provides this in esp_check.h; if it’s not available, fallback.
#if __has_include("esp_check.h")
  #include "esp_check.h"
#else
  #ifndef ESP_RETURN_ON_ERROR
  #define ESP_RETURN_ON_ERROR(expr, tag, msg) do {             \
      esp_err_t __err = (expr);                                 \
      if (__err != ESP_OK) {                                    \
        ESP_LOGE((tag), "%s: %s", (msg), esp_err_to_name(__err));\
        return __err;                                           \
      }                                                         \
    } while (0)
  #endif
#endif

// Default pins/addr unless you override these in sdkconfig or before including this header
#ifndef I2C_MASTER_NUM
#define I2C_MASTER_NUM       I2C_NUM_0
#endif
#ifndef I2C_MASTER_SCL_IO
#define I2C_MASTER_SCL_IO    16
#endif
#ifndef I2C_MASTER_SDA_IO
#define I2C_MASTER_SDA_IO    17
#endif
#ifndef I2C_MASTER_FREQ_HZ
#define I2C_MASTER_FREQ_HZ   400000
#endif
#ifndef ENS160_SENSOR_ADDR
#define ENS160_SENSOR_ADDR   0x53   // (ADDR/SMD pad decides 0x52 or 0x53)
#endif

// Order: [AQI, TVOC_ppb, eCO2_ppm, VALIDITY]
#define ENS160_VAL_AQI        0
#define ENS160_VAL_TVOC_PPB   1
#define ENS160_VAL_ECO2_PPM   2
#define ENS160_VAL_VALIDITY   3
#define ENS160_VALS_COUNT     4

// ------- SCD40 (Sensirion SCD4x family) -------
#ifndef SCD40_I2C_ADDR
#define SCD40_I2C_ADDR 0x62   // 7-bit I2C address
#endif

#define SCD40_VAL_CO2_PPM       0   // int
#define SCD40_VAL_TEMP_C_X100   1   // int: °C * 100
#define SCD40_VAL_RH_X100       2   // int: % * 100
#define SCD40_VALS_COUNT        3
#define SCD4X_CMD_SET_TEMP_OFFSET      0x241D
#define SCD4X_CMD_SET_SENSOR_ALTITUDE  0x2427

esp_err_t scd40_start(void);   // start periodic measurement (~5s updates)
esp_err_t scd40_stop(void);    // stop periodic measurement
esp_err_t scd40_reinit(void);  // soft reinit (resets state machine)
esp_err_t scd40_read_values(int out[SCD40_VALS_COUNT]);  // fills {CO2, T*100, RH*100}

// Fills out[0..3] with {AQI, TVOC_ppb, eCO2_ppm, VALIDITY}
esp_err_t ens160_read_values(int out[ENS160_VALS_COUNT]);


typedef struct {
    uint8_t  aqi;        // 1..5
    uint16_t tvoc_ppb;   // ppb
    uint16_t eco2_ppm;   // ppm
    uint8_t  validity;   // 0 normal, 1 warm-up, 2 initial, 3 invalid
} ens160_measurement_t;

esp_err_t i2c_helper_init(void);
esp_err_t ens160_init_sensor(void);
esp_err_t ens160_set_compensation(float temp_c, float rh_percent);
esp_err_t ens160_read_measurement(ens160_measurement_t *m);
esp_err_t ens160_read_string(char *out, size_t out_len);
esp_err_t ens160_log_once(char *out, size_t out_len);
// SCD40 init: wake -> stop -> reinit -> start (safe sequence)
esp_err_t scd40_init(void);

