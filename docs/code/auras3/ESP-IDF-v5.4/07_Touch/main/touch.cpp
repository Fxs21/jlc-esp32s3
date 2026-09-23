#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "freertos/queue.h"
#include "driver/i2c.h"
#include "SensorLib.h"
#include "TouchDrvCST92xx.h"

// I2C configuration
#define I2C_MASTER_NUM (i2c_port_t)1
#define I2C_MASTER_FREQ_HZ 100000 /*!< I2C master clock frequency */
#define I2C_MASTER_SDA_IO (gpio_num_t)15
#define I2C_MASTER_SCL_IO (gpio_num_t)14
#define Touch_INT (gpio_num_t)11
#define Touch_RST (gpio_num_t)40

#define I2C_MASTER_TX_BUF_DISABLE 0 /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE 0 /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS 1000
uint8_t touchAddress = 0x5A;

TouchDrvCST92xx touch;
int16_t x[5], y[5];
bool isPressed = false;

static const char *TAG = "CST9217"; // Define a tag for logging

esp_err_t i2c_init(void)
{
    i2c_config_t i2c_conf;
    memset(&i2c_conf, 0, sizeof(i2c_conf));
    i2c_conf.mode = I2C_MODE_MASTER;
    i2c_conf.sda_io_num = I2C_MASTER_SDA_IO;
    i2c_conf.scl_io_num = I2C_MASTER_SCL_IO;
    i2c_conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    i2c_conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    i2c_conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    i2c_param_config(I2C_MASTER_NUM, &i2c_conf);
    return i2c_driver_install(I2C_MASTER_NUM, i2c_conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}

void read_sensor_data(void *arg); // Function declaration

void setup_sensor()
{
    uint8_t touchAddress = 0x5A;

    touch.setPins(Touch_RST, Touch_INT);
    touch.begin(I2C_MASTER_NUM, touchAddress, I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);
    touch.reset();
    touch.setMaxCoordinates(466, 466);
    touch.setMirrorXY(true, true);
}

extern "C" void app_main()
{
    ESP_ERROR_CHECK(i2c_init());

    setup_sensor();
    xTaskCreate(read_sensor_data, "sensor_read_task", 4096, NULL, 10, NULL);
}

void read_sensor_data(void *arg)
{
    while (1)
    {   
        uint8_t touched = touch.getPoint(x, y, 2);
        if (touched)
        {
            for (int i = 0; i < 1; ++i)
            {
                ESP_LOGI(TAG, "Touch[%d]: X=%d Y=%d", i, x[i], y[i]);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}
