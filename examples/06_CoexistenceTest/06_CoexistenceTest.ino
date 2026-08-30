/**
 * Pyintel Lux — Example 06: Standard Library Coexistence Test
 * 
 * Validates concurrent operation of Lux telemetry with Wire (I2C), SPI,
 * and standard peripherals without bus interference or memory exhaustion.
 */

#include <Lux.h>
#include <Wire.h>
#include <SPI.h>

#define SYM_I2C_STATUS    0x0601
#define SYM_SPI_STATUS    0x0602
#define SYM_FREE_RAM      0x0603

void setup() {
    Serial.begin(115200);
    Lux.begin(Serial);

    // Initialize I2C and SPI buses concurrently
    Wire.begin();
    SPI.begin();

    Lux.deviceInfo("Coexistence-Test-Node");
}

void loop() {
    Lux.tick();

    // 1. I2C bus scan ping (address 0x68 e.g. RTC / IMU)
    Wire.beginTransmission(0x68);
    uint8_t i2c_err = Wire.endTransmission();
    Lux.trace(SYM_I2C_STATUS, i2c_err);

    // 2. SPI transaction
    SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
    uint8_t spi_ret = SPI.transfer(0x00);
    SPI.endTransaction();
    Lux.trace(SYM_SPI_STATUS, spi_ret);

    // 3. Free RAM check
    Lux.trace(SYM_FREE_RAM, Lux.getFreeRam());

    Lux.flush();
    delay(100);
}
