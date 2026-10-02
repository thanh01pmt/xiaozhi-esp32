#ifndef SMART_HOME_BLE_CONTROLLER_H
#define SMART_HOME_BLE_CONTROLLER_H

#include <string>
#include <vector>
#include <functional>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "device_model.h"

class SmartHomeBleController {
public:
    SmartHomeBleController();
    ~SmartHomeBleController() = default;

    void Initialize();
    void StartScanning(uint32_t duration_sec = 10);
    void StopScanning();
    bool SendBleCommand(const std::string& address, const std::vector<uint8_t>& data);

private:
    bool initialized_ = false;
    bool is_scanning_ = false;
};

#endif // SMART_HOME_BLE_CONTROLLER_H
