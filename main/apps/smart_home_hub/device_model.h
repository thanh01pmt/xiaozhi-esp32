#ifndef SMART_HOME_DEVICE_MODEL_H
#define SMART_HOME_DEVICE_MODEL_H

#include <string>
#include <vector>
#include <functional>
#include <cstdint>

enum class DeviceType {
    Switch,
    Light,
    Climate,
    Sensor
};

struct SmartDevice {
    std::string id;
    std::string name;
    std::string room;
    DeviceType type = DeviceType::Switch;
    bool state = false;
    int level = 0;          // 0-100% (Light brightness) hoặc 16-30 (Climate target temp)
    float current_temp = 0.0f; // Dành cho Climate / Sensor
    float current_humidity = 0.0f;
    std::string endpoint;   // URL HTTP hoặc BLE Address
};

using DeviceChangeCallback = std::function<void(const SmartDevice& device)>;

#endif // SMART_HOME_DEVICE_MODEL_H
