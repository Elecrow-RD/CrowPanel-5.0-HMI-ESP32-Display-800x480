#include <Arduino.h>
#include <Wire.h>

#include "dht20.h"

/* DHT20 uses address 0x38 and a short command/read conversion sequence. */
namespace {
constexpr uint8_t dht20_address = 0x38;

/** Send one DHT20 command and report the I2C acknowledgement. */
bool write_command(const uint8_t * command, size_t size)
{
    Wire.beginTransmission(dht20_address);
    Wire.write(command, size);
    return Wire.endTransmission() == 0;
}

/** Read an exact number of bytes so partial frames are rejected. */
bool read_bytes(uint8_t * data, size_t size)
{
    if (Wire.requestFrom(dht20_address, size) != size) {
        return false;
    }
    for (size_t index = 0; index < size; ++index) {
        data[index] = Wire.read();
    }
    return true;
}
}

/**
 * @brief Check the DHT20 status and issue calibration when required.
 * @return true when the sensor acknowledges initialization.
 */
bool dht20_init()
{
    delay(100);
    const uint8_t status_command = 0x71;
    uint8_t status = 0;
    if (!write_command(&status_command, 1) || !read_bytes(&status, 1)) {
        return false;
    }

    if (!(status & 0x08)) {
        const uint8_t init_command[] = {0xBE, 0x08, 0x00};
        if (!write_command(init_command, sizeof(init_command))) {
            return false;
        }
        delay(10);
    }
    return true;
}

/**
 * @brief Trigger a conversion and decode integer temperature/humidity values.
 * @param temperature Output temperature in degrees Celsius.
 * @param humidity Output relative humidity in percent.
 * @return true when a complete measurement is available.
 */
bool dht20_read(int & temperature, int & humidity)
{
    // Start one measurement; the sensor raises its busy bit during conversion.
    const uint8_t measure_command[] = {0xAC, 0x33, 0x00};
    if (!write_command(measure_command, sizeof(measure_command))) {
        return false;
    }

    delay(80);
    uint8_t data[6];
    for (uint8_t attempt = 0; attempt < 3; ++attempt) {
        if (!read_bytes(data, sizeof(data))) {
            return false;
        }
        if (!(data[0] & 0x80)) {
            // Combine the packed 20-bit fields before scaling to integer units.
            const uint32_t raw_humidity = (static_cast<uint32_t>(data[1]) << 12) |
                                          (static_cast<uint32_t>(data[2]) << 4) |
                                          (data[3] >> 4);
            const uint32_t raw_temperature = (static_cast<uint32_t>(data[3] & 0x0F) << 16) |
                                             (static_cast<uint32_t>(data[4]) << 8) | data[5];
            humidity = raw_humidity * 100 / 0x100000;
            temperature = raw_temperature * 200 / 0x100000 - 50;
            return true;
        }
        delay(10);
    }
    return false;
}
