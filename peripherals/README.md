# Sensor kit & peripherals

Senior-level design:

- **Common**: `common/sensor_result.h` – shared `sensor_result_t` (OK, ERR_PARAM, ERR_TIMEOUT, ERR_BUS, ERR_CHECKSUM, ERR_NOT_READY).
- **Config structs**: Each driver has an `*_config_t` passed to `*_init()`. No magic numbers in code; timeouts and addresses in config or header defines.
- **Layered**: Drivers use HAL (GPIO, I2C, SPI) from `../drivers/`; application uses only the peripheral API.

## Displays

- **lcd_16x2** – HD44780 16x2, I2C (PCF8574) or 4-bit GPIO. `lcd_16x2_config_t`, `lcd_16x2_init()`, `lcd_16x2_write_string()`, etc.
- **ssd1306** – 128x64 OLED, I2C/SPI. `ssd1306_config_t`, `ssd1306_draw_pixel()`, `ssd1306_refresh()`.

## Sensors

- **dht22** – Temp + humidity (1-wire). `dht22_config_t`, `dht22_read(dht22_data_t *)`.
- **ds18b20** – 1-Wire temperature. `ds18b20_config_t`, `ds18b20_read_temp_c_x16(int16_t *)`.
- **bme280** – Temp, humidity, pressure (I2C/SPI). `bme280_config_t`, `bme280_read(bme280_data_t *)`.
- **mpu6050** – Accel + gyro (I2C). `mpu6050_config_t`, `mpu6050_read(mpu6050_data_t *)`.

Include `peripherals/common/sensor_result.h` when using any sensor. Select the required HAL driver (I2C, GPIO) in the project tree.
