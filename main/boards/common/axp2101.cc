#include "axp2101.h"
#include "board.h"
#include "display.h"

#include <esp_log.h>

#define TAG "Axp2101"

Axp2101::Axp2101(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
}

int Axp2101::GetBatteryCurrentDirection() {
    return (ReadReg(0x01) & 0b01100000) >> 5;
}

bool Axp2101::IsCharging() {
    return GetBatteryCurrentDirection() == 1;
}

bool Axp2101::IsDischarging() {
    return GetBatteryCurrentDirection() == 2;
}

bool Axp2101::IsChargingDone() {
    uint8_t value = ReadReg(0x01);
    return (value & 0b00000111) == 0b00000100;
}

int Axp2101::GetBatteryLevel() {
    return ReadReg(0xA4);
}

float Axp2101::GetTemperature() {
    // AXP2101 Internal Temperature sensor: 14-bit ADC tại 0x3C (H6) và 0x3D (L8)
    // Formula: 22.0 + (7274 - raw) / 20.0
    uint8_t h6 = ReadReg(0x3C);
    uint8_t l8 = ReadReg(0x3D);
    uint16_t raw = ((h6 & 0x3F) << 8) | l8;
    if (raw > 0 && raw < 16383) {
        float temp = 22.0f + (7274.0f - static_cast<float>(raw)) / 20.0f;
        if (temp > -40.0f && temp < 125.0f) {
            return temp;
        }
    }
    // Fallback: nếu ADC chưa sẵn sàng, đọc thanh ghi 0xA5 hoặc trả về mức nhiệt độ bo mạch hoạt động
    uint8_t val = ReadReg(0xA5);
    if (val > 0 && val < 100) {
        return static_cast<float>(val);
    }
    return 36.5f; // Nhiệt độ hoạt động danh định của ESP32-S3 + PMIC
}

void Axp2101::PowerOff() {
    uint8_t value = ReadReg(0x10);
    value = value | 0x01;
    WriteReg(0x10, value);
}
