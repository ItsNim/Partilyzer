#include "i2chelper.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include "bme68x_defs.h"
#include "bme68x.h"


// Local log tag

// ------- ENS160 register map -------
#define ENS160_REG_PART_ID       0x00
#define ENS160_REG_OPMODE        0x10
#define ENS160_REG_CONFIG        0x11
#define ENS160_REG_COMMAND       0x12
#define ENS160_REG_TEMP_IN       0x13
#define ENS160_REG_RH_IN         0x15
#define ENS160_REG_DEVICE_STATUS 0x20
#define ENS160_REG_DATA_AQI      0x21
#define ENS160_REG_DATA_TVOC     0x22 // 16-bit LSB→MSB
#define ENS160_REG_DATA_ECO2     0x24 // 16-bit LSB→MSB

// OPMODE values
#define ENS160_OPMODE_DEEPSLEEP  0x00
#define ENS160_OPMODE_IDLE       0x01
#define ENS160_OPMODE_STANDARD   0x02
#define ENS160_OPMODE_RESET      0xF0

// DEVICE_STATUS bits
#define ENS160_STATUS_NEWDAT        (1 << 1)
#define ENS160_STATUS_VALIDITY_MASK (0x3 << 2)

// top of i2chelper.c, after includes
static const char *TAG_ENS160 = "ENS160";
static const char *TAG_SCD40  = "SCD40";

// BME68X device structure
static struct bme68x_dev bme68x_dev;

// BME68X configuration structure
static struct bme68x_conf conf;

// BME68X heater configuration structure
static struct bme68x_heatr_conf heatr_conf;

// BME68X data structure
static struct bme68x_data data;

esp_err_t i2c_helper_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    ESP_RETURN_ON_ERROR(i2c_param_config(I2C_MASTER_NUM, &conf), TAG_ENS160, "i2c_param_config");
    return i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

static esp_err_t ens160_write_u8(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    return i2c_master_write_to_device(I2C_MASTER_NUM, ENS160_SENSOR_ADDR,
                                      buf, sizeof(buf), pdMS_TO_TICKS(100));
}

static esp_err_t ens160_read(uint8_t reg, uint8_t *data, size_t len) {
    return i2c_master_write_read_device(I2C_MASTER_NUM, ENS160_SENSOR_ADDR,
                                        &reg, 1, data, len, pdMS_TO_TICKS(100));
}

esp_err_t ens160_init_sensor(void) {
    // PART_ID should be 0x0160 (LSB=0x60 at 0x00, MSB=0x01 at 0x01)
    uint8_t id[2] = {0};
    ESP_RETURN_ON_ERROR(ens160_read(ENS160_REG_PART_ID, id, 2), TAG_ENS160, "read PART_ID");
    if (id[0] != 0x60 || id[1] != 0x01) {
        ESP_LOGW(TAG_ENS160, "Unexpected PART_ID: 0x%02X 0x%02X", id[0], id[1]);
    }

    // Reset -> IDLE -> STANDARD
    ESP_RETURN_ON_ERROR(ens160_write_u8(ENS160_REG_OPMODE, ENS160_OPMODE_RESET), TAG_ENS160, "reset");
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_RETURN_ON_ERROR(ens160_write_u8(ENS160_REG_OPMODE, ENS160_OPMODE_IDLE), TAG_ENS160, "idle");
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_RETURN_ON_ERROR(ens160_write_u8(ENS160_REG_OPMODE, ENS160_OPMODE_STANDARD), TAG_ENS160, "standard");
    vTaskDelay(pdMS_TO_TICKS(10));

    return ESP_OK;
}

// Compensation: Temp in Kelvin*64, RH in %*512
esp_err_t ens160_set_compensation(float temp_c, float rh_percent) {
    uint16_t t  = (uint16_t)((temp_c + 273.15f) * 64.0f + 0.5f);
    uint16_t rh = (uint16_t)(rh_percent * 512.0f + 0.5f);
    uint8_t tbuf[3] = { ENS160_REG_TEMP_IN, (uint8_t)(t & 0xFF), (uint8_t)(t >> 8) };
    uint8_t rbuf[3] = { ENS160_REG_RH_IN,   (uint8_t)(rh & 0xFF), (uint8_t)(rh >> 8) };
    ESP_RETURN_ON_ERROR(i2c_master_write_to_device(I2C_MASTER_NUM, ENS160_SENSOR_ADDR, tbuf, 3, pdMS_TO_TICKS(100)), TAG_ENS160, "temp");
    ESP_RETURN_ON_ERROR(i2c_master_write_to_device(I2C_MASTER_NUM, ENS160_SENSOR_ADDR, rbuf, 3, pdMS_TO_TICKS(100)), TAG_ENS160, "rh");
    return ESP_OK;
}

static esp_err_t ens160_wait_data_ready(uint32_t timeout_ms, uint8_t *status_out) {
    TickType_t t0 = xTaskGetTickCount();
    do {
        uint8_t st = 0;
        esp_err_t err = ens160_read(ENS160_REG_DEVICE_STATUS, &st, 1);
        if (err == ESP_OK && (st & ENS160_STATUS_NEWDAT)) {
            if (status_out) *status_out = st;
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    } while ((xTaskGetTickCount() - t0) < pdMS_TO_TICKS(timeout_ms));
    return ESP_ERR_TIMEOUT;
}

esp_err_t ens160_read_measurement(ens160_measurement_t *m) {
    if (!m) return ESP_ERR_INVALID_ARG;
    uint8_t status = 0;
    ESP_RETURN_ON_ERROR(ens160_wait_data_ready(1000, &status), TAG_ENS160, "wait NEWDAT");
    m->validity = (status & ENS160_STATUS_VALIDITY_MASK) >> 2;

    uint8_t aqi = 0;
    ESP_RETURN_ON_ERROR(ens160_read(ENS160_REG_DATA_AQI, &aqi, 1), TAG_ENS160, "read AQI");
    m->aqi = aqi & 0x07;

    uint8_t buf[2];
    ESP_RETURN_ON_ERROR(ens160_read(ENS160_REG_DATA_TVOC, buf, 2), TAG_ENS160, "read TVOC");
    m->tvoc_ppb = (uint16_t)((buf[1] << 8) | buf[0]);

    ESP_RETURN_ON_ERROR(ens160_read(ENS160_REG_DATA_ECO2, buf, 2), TAG_ENS160, "read eCO2");
    m->eco2_ppm = (uint16_t)((buf[1] << 8) | buf[0]);

    return ESP_OK;
}

static const char *validity_text(uint8_t v) {
    static const char *vtext[] = {"normal", "warm-up", "initial", "invalid"};
    return vtext[(v & 3)];
}

esp_err_t ens160_read_string(char *out, size_t out_len) {
    if (!out || out_len == 0) return ESP_ERR_INVALID_ARG;

    ens160_measurement_t m = {0};
    esp_err_t err = ens160_read_measurement(&m);
    if (err != ESP_OK) {
        // Put the error message in the buffer too
        (void)snprintf(out, out_len, "ENS160 read failed: %s", esp_err_to_name(err));
        return err;
    }

    int n = snprintf(out, out_len,
                     "AQI=%u  TVOC=%u ppb  eCO2=%u ppm  (status: %s)",
                     m.aqi, m.tvoc_ppb, m.eco2_ppm, validity_text(m.validity));
    if (n < 0 || (size_t)n >= out_len) {
        // Truncated — caller gave a small buffer
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t ens160_log_once(char *out, size_t out_len) {
    esp_err_t err = ens160_read_string(out, out_len);
    if (err == ESP_OK) {
        ESP_LOGI(TAG_ENS160, "%s", out);
    } else {
        ESP_LOGE(TAG_ENS160, "%s", out);
    }
    return err;
}

esp_err_t ens160_read_values(int out[ENS160_VALS_COUNT]) {
    if (!out) return ESP_ERR_INVALID_ARG;

    ens160_measurement_t m = {0};
    esp_err_t err = ens160_read_measurement(&m);
    if (err != ESP_OK) return err;

    out[ENS160_VAL_AQI]       = (int)m.aqi;
    out[ENS160_VAL_TVOC_PPB]  = (int)m.tvoc_ppb;
    out[ENS160_VAL_ECO2_PPM]  = (int)m.eco2_ppm;
    out[ENS160_VAL_VALIDITY]  = (int)m.validity;

    static const char *vtext[] = {"normal", "warm-up", "initial", "invalid"};
    ESP_LOGI(TAG_ENS160, "AQI=%u  TVOC=%u ppb  eCO2=%u ppm  (status: %s)",
             m.aqi, m.tvoc_ppb, m.eco2_ppm, vtext[m.validity & 3]);

    return ESP_OK;
}

// ====== SCD40 support ======
#include <math.h>  // not strictly needed (we use integer math)

// Command set (big-endian, 16-bit)
#define SCD4X_CMD_START_PERIODIC_MEAS   0x21B1
#define SCD4X_CMD_STOP_PERIODIC_MEAS    0x3F86
#define SCD4X_CMD_READ_MEASUREMENT      0xEC05
#define SCD4X_CMD_GET_DATA_READY        0xE4B8
#define SCD4X_CMD_REINIT                0x3646

// Sensirion CRC-8: poly 0x31, init 0xFF, 2 data bytes -> 1 CRC
static uint8_t scd_crc8(const uint8_t *data) {
    uint8_t crc = 0xFF;
    for (int i = 0; i < 2; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

static esp_err_t scd40_cmd(uint16_t cmd) {
    uint8_t buf[2] = { (uint8_t)(cmd >> 8), (uint8_t)(cmd & 0xFF) };
    return i2c_master_write_to_device(I2C_MASTER_NUM, SCD40_I2C_ADDR,
                                      buf, sizeof(buf), pdMS_TO_TICKS(50));
}

static esp_err_t scd40_cmd_read(uint16_t cmd, uint8_t *rx, size_t rx_len, TickType_t to_ticks) {
    uint8_t tx[2] = { (uint8_t)(cmd >> 8), (uint8_t)(cmd & 0xFF) };
    return i2c_master_write_read_device(I2C_MASTER_NUM, SCD40_I2C_ADDR,
                                        tx, sizeof(tx), rx, rx_len, to_ticks);
}

esp_err_t scd40_start(void) {
    ESP_RETURN_ON_ERROR(scd40_cmd(SCD4X_CMD_START_PERIODIC_MEAS), TAG_SCD40, "SCD40 start");
    // First valid measurement comes ~5 s later
    return ESP_OK;
}

esp_err_t scd40_stop(void) {
    return scd40_cmd(SCD4X_CMD_STOP_PERIODIC_MEAS);
}

esp_err_t scd40_reinit(void) {
    // Per datasheet, wait ~1ms after stop before reinit; we’re tolerant
    ESP_RETURN_ON_ERROR(scd40_cmd(SCD4X_CMD_REINIT), TAG_SCD40, "SCD40 reinit");
    vTaskDelay(pdMS_TO_TICKS(20));
    return ESP_OK;
}

static esp_err_t scd40_data_ready(bool *ready) {
    if (ready) *ready = false;
    uint8_t rx[3] = {0};
    ESP_RETURN_ON_ERROR(scd40_cmd_read(SCD4X_CMD_GET_DATA_READY, rx, sizeof(rx), pdMS_TO_TICKS(50)), TAG_SCD40, "SCD40 ready");
    if (scd_crc8(rx) != rx[2]) {
        ESP_LOGE(TAG_SCD40, "SCD40 ready CRC error");
        return ESP_ERR_INVALID_RESPONSE;
    }
    uint16_t flags = ((uint16_t)rx[0] << 8) | rx[1];
    if (ready) *ready = (flags != 0);
    return ESP_OK;
}

static esp_err_t scd40_wait_ready(uint32_t timeout_ms) {
    TickType_t t0 = xTaskGetTickCount();
    for (;;) {
        bool rdy = false;
        esp_err_t e = scd40_data_ready(&rdy);
        if (e == ESP_OK && rdy) return ESP_OK;
        if ((xTaskGetTickCount() - t0) > pdMS_TO_TICKS(timeout_ms)) return ESP_ERR_TIMEOUT;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Read CO2 / T / RH, verify CRCs, convert to ints
esp_err_t scd40_read_values(int out[SCD40_VALS_COUNT]) {
    if (!out) return ESP_ERR_INVALID_ARG;

    // Wait until new data available (SCD40 updates ~every 5 seconds)
    ESP_RETURN_ON_ERROR(scd40_wait_ready(6000), TAG_SCD40, "SCD40 wait ready");

    uint8_t rx[9] = {0};
    ESP_RETURN_ON_ERROR(scd40_cmd_read(SCD4X_CMD_READ_MEASUREMENT, rx, sizeof(rx), pdMS_TO_TICKS(50)), TAG_SCD40, "SCD40 read");

    // Three words: CO2 + CRC, T + CRC, RH + CRC
    if (scd_crc8(&rx[0]) != rx[2] || scd_crc8(&rx[3]) != rx[5] || scd_crc8(&rx[6]) != rx[8]) {
        ESP_LOGE(TAG_SCD40, "SCD40 CRC error");
        return ESP_ERR_INVALID_RESPONSE;
    }

    uint16_t co2_raw = ((uint16_t)rx[0] << 8) | rx[1];
    uint16_t t_raw   = ((uint16_t)rx[3] << 8) | rx[4];
    uint16_t rh_raw  = ((uint16_t)rx[6] << 8) | rx[7];

    // Convert with integer math:
    // T(°C*100) = -4500 + (17500 * raw / 65535)
    // RH(%*100) = (10000 * raw / 65535)
    int temp_c_x100 = (int)((17500LL * t_raw) / 65535) - 4500;
    int rh_x100     = (int)((10000LL * rh_raw) / 65535);

    out[SCD40_VAL_CO2_PPM]     = (int)co2_raw;
    out[SCD40_VAL_TEMP_C_X100] = temp_c_x100;
    out[SCD40_VAL_RH_X100]     = rh_x100;

    ESP_LOGI(TAG_SCD40, "SCD40 CO2=%d ppm  T=%.2f C  RH=%.2f %%", (int)co2_raw,
             temp_c_x100 / 100.0f, rh_x100 / 100.0f);

    return ESP_OK;
}
// SCD4x sleep/wake (some modules boot sleeping)
#define SCD4X_CMD_WAKE_UP               0x36F6

static esp_err_t scd40_wake(void) {
    uint8_t tx[2] = { (uint8_t)(SCD4X_CMD_WAKE_UP >> 8), (uint8_t)(SCD4X_CMD_WAKE_UP & 0xFF) };
    // Wake can be sent even if already awake; device NACKs during sleep only.
    // Use a short timeout and ignore errors (best-effort wake).
    i2c_master_write_to_device(I2C_MASTER_NUM, SCD40_I2C_ADDR, tx, sizeof(tx), pdMS_TO_TICKS(20));
    vTaskDelay(pdMS_TO_TICKS(2));
    return ESP_OK;
}

static esp_err_t scd40_set_temperature_offset(uint16_t offset_ticks) {
    // This command requires a 16-bit argument (offset_ticks) and a CRC.
    // Total payload is 5 bytes: [cmd_msb, cmd_lsb, arg_msb, arg_lsb, crc]
    uint8_t buf[5];
    uint16_t cmd = SCD4X_CMD_SET_TEMP_OFFSET;

    // Command
    buf[0] = (uint8_t)(cmd >> 8);
    buf[1] = (uint8_t)(cmd & 0xFF);

    // Argument (the offset in ticks)
    buf[2] = (uint8_t)(offset_ticks >> 8);
    buf[3] = (uint8_t)(offset_ticks & 0xFF);

    // CRC for the argument
    buf[4] = scd_crc8(&buf[2]); // CRC is calculated over the argument bytes only

    return i2c_master_write_to_device(I2C_MASTER_NUM, SCD40_I2C_ADDR,
                                      buf, sizeof(buf), pdMS_TO_TICKS(50));
}

/**
 * @brief Sets the sensor altitude compensation in meters.
 *
 * @param altitude_meters The altitude in meters above sea level.
 * @return esp_err_t ESP_OK on success.
 */
static esp_err_t scd40_set_altitude(uint16_t altitude_meters) {
    // This command requires a 16-bit argument (altitude) and a CRC.
    // Total payload is 5 bytes: [cmd_msb, cmd_lsb, arg_msb, arg_lsb, crc]
    uint8_t buf[5];
    uint16_t cmd = SCD4X_CMD_SET_SENSOR_ALTITUDE;

    // Command
    buf[0] = (uint8_t)(cmd >> 8);
    buf[1] = (uint8_t)(cmd & 0xFF);

    // Argument (the altitude in meters)
    buf[2] = (uint8_t)(altitude_meters >> 8);
    buf[3] = (uint8_t)(altitude_meters & 0xFF);

    // CRC for the argument
    buf[4] = scd_crc8(&buf[2]); // CRC is calculated over the argument bytes only

    return i2c_master_write_to_device(I2C_MASTER_NUM, SCD40_I2C_ADDR,
                                      buf, sizeof(buf), pdMS_TO_TICKS(50));
}

esp_err_t scd40_init(void) {
    // Some boards need a moment after power-up
    vTaskDelay(pdMS_TO_TICKS(50));

    scd40_wake();                                  // best-effort, ignore error
    (void) scd40_stop();                            // ignore in case not measuring
    vTaskDelay(pdMS_TO_TICKS(500));
    ESP_RETURN_ON_ERROR(scd40_reinit(), TAG_SCD40, "reinit");
    vTaskDelay(pdMS_TO_TICKS(20));

    // Calculate the offset in ticks from the Celsius value.
    // The formula is (offset_in_celsius * 65536) / 175
    uint16_t offset_ticks = (uint16_t)((1.23 * 65536.0f) / 175.0f);
    ESP_LOGI(TAG_SCD40, "Applying temperature offset of %.2f°C (%u ticks)", 1.23, offset_ticks);
    ESP_RETURN_ON_ERROR(scd40_set_temperature_offset(offset_ticks), TAG_SCD40, "set temp offset failed");
    vTaskDelay(pdMS_TO_TICKS(5));

    // --- SET ALTITUDE ---
    uint16_t altitude_m = 15; // Altitude for Flushing, NY
    ESP_LOGI(TAG_SCD40, "Applying altitude of %u meters", altitude_m);
    ESP_RETURN_ON_ERROR(scd40_set_altitude(altitude_m), TAG_SCD40, "set altitude failed");
    vTaskDelay(pdMS_TO_TICKS(5));

    ESP_RETURN_ON_ERROR(scd40_start(), TAG_SCD40, "start failed");



    return ESP_OK;
}

