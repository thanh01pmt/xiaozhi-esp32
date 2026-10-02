#include "ble_controller.h"
#include <esp_log.h>

#define TAG "SH_BleCtrl"

SmartHomeBleController::SmartHomeBleController() = default;

void SmartHomeBleController::Initialize() {
    if (initialized_) return;
    ESP_LOGI(TAG, "Initializing SmartHome BLE Controller (Stub/Ready)");
    initialized_ = true;
}

void SmartHomeBleController::StartScanning(uint32_t duration_sec) {
    if (!initialized_) return;
    is_scanning_ = true;
    ESP_LOGI(TAG, "BLE Scan started for %lu seconds", (unsigned long)duration_sec);
}

void SmartHomeBleController::StopScanning() {
    if (!initialized_ || !is_scanning_) return;
    is_scanning_ = false;
    ESP_LOGI(TAG, "BLE Scan stopped");
}

bool SmartHomeBleController::SendBleCommand(const std::string& address, const std::vector<uint8_t>& data) {
    if (!initialized_) return false;
    ESP_LOGI(TAG, "Sending BLE command to %s (len=%u)", address.c_str(), (unsigned)data.size());
    return true;
}
