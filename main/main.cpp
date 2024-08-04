#include <stdio.h>

#include "driver/i2c_master.h"

#include "WM8960.hpp"

#define I2C_PORT_NUM 0
#define I2C_SCL_GPIO_NUM GPIO_NUM_4
#define I2C_SDA_GPIO_NUM GPIO_NUM_5

i2c_master_bus_config_t i2c_mst_config = {
    .i2c_port = I2C_PORT_NUM,
    .sda_io_num = I2C_SDA_GPIO_NUM,
    .scl_io_num = I2C_SCL_GPIO_NUM,
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .glitch_ignore_cnt = 7,
    .flags.enable_internal_pullup = true,
};

i2c_master_bus_handle_t bus_handle;

void app_main(void)
{
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &bus_handle));
}