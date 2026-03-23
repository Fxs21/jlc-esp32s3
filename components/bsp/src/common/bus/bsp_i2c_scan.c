#include "bsp_i2c.h"

#include "driver/i2c_master.h"

esp_err_t bsp_i2c_probe(uint8_t address, uint32_t timeout_ms)
{
    if (address < BSP_I2C_SCAN_FIRST_ADDR || address > BSP_I2C_SCAN_LAST_ADDR) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_master_bus_handle_t bus = NULL;
    esp_err_t ret = bsp_i2c_acquire_default(&bus);
    if (ret != ESP_OK) {
        return ret;
    }

    uint32_t probe_timeout_ms = timeout_ms == 0 ? BSP_I2C_SCAN_DEFAULT_TIMEOUT_MS : timeout_ms;
    ret = i2c_master_probe(bus, address, probe_timeout_ms);
    esp_err_t release_ret = bsp_i2c_release_default();
    return ret != ESP_OK ? ret : release_ret;
}

esp_err_t bsp_i2c_scan(uint8_t *out_addresses, size_t address_capacity, size_t *out_count, uint32_t timeout_ms)
{
    if (address_capacity > 0 && out_addresses == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (out_count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *out_count = 0;
    i2c_master_bus_handle_t bus = NULL;
    esp_err_t ret = bsp_i2c_acquire_default(&bus);
    if (ret != ESP_OK) {
        return ret;
    }

    uint32_t probe_timeout_ms = timeout_ms == 0 ? BSP_I2C_SCAN_DEFAULT_TIMEOUT_MS : timeout_ms;
    size_t found = 0;
    for (uint8_t addr = BSP_I2C_SCAN_FIRST_ADDR; addr <= BSP_I2C_SCAN_LAST_ADDR; addr++) {
        if (i2c_master_probe(bus, addr, probe_timeout_ms) == ESP_OK) {
            if (found < address_capacity) {
                out_addresses[found] = addr;
            }
            found++;
        }
    }

    *out_count = found;
    esp_err_t release_ret = bsp_i2c_release_default();
    if (release_ret != ESP_OK) {
        return release_ret;
    }
    return found > address_capacity ? ESP_ERR_INVALID_SIZE : ESP_OK;
}
