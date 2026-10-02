#include "smart_home_hub.h"
#include "mcp_tools.h"
#include "application.h"
#include <esp_log.h>
#include <cJSON.h>

#define TAG "SmartHomeHub"

SmartHomeHub::SmartHomeHub() = default;

void SmartHomeHub::Initialize(lv_display_t* lv_display) {
    if (initialized_) return;
    ESP_LOGI(TAG, "Initializing SmartHomeHub on CoreS3");

    LoadDevices();
    ble_controller_.Initialize();

    if (lv_display != nullptr) {
        dashboard_screen_.Initialize(lv_display);
    }

    SmartHomeMcpTools::RegisterTools(this);
    initialized_ = true;
    ESP_LOGI(TAG, "SmartHomeHub initialized successfully");
}

void SmartHomeHub::LoadDevices() {
    devices_.clear();
    devices_.push_back({"light.living_room", "Living Room Light", "Living Room", DeviceType::Light, false, 80, 0, 0, ""});
    devices_.push_back({"switch.fan", "Ceiling Fan", "Living Room", DeviceType::Switch, false, 0, 0, 0, ""});
    devices_.push_back({"climate.ac", "Air Conditioner", "Living Room", DeviceType::Climate, true, 24, 25.5f, 60.0f, ""});
}

void SmartHomeHub::ToggleDashboard() {
    if (dashboard_screen_.IsVisible()) {
        dashboard_screen_.Hide();
    } else {
        dashboard_screen_.Show();
    }
}

void SmartHomeHub::ShowDashboard() {
    dashboard_screen_.Show();
}

void SmartHomeHub::HideDashboard() {
    dashboard_screen_.Hide();
}

bool SmartHomeHub::SetDeviceState(const std::string& device_id, bool turn_on) {
    ESP_LOGI(TAG, "SetDeviceState: %s -> %d", device_id.c_str(), turn_on);
    for (auto& dev : devices_) {
        if (dev.id == device_id || dev.name == device_id) {
            dev.state = turn_on;
            dashboard_screen_.UpdateDeviceState(dev);
            break;
        }
    }
    return network_client_.SendSwitchCommand(device_id, turn_on);
}

bool SmartHomeHub::SetDeviceLevel(const std::string& device_id, int level) {
    ESP_LOGI(TAG, "SetDeviceLevel: %s -> %d", device_id.c_str(), level);
    for (auto& dev : devices_) {
        if (dev.id == device_id || dev.name == device_id) {
            dev.level = level;
            dashboard_screen_.UpdateDeviceState(dev);
            break;
        }
    }
    return network_client_.SendLevelCommand(device_id, level);
}

bool SmartHomeHub::TriggerScene(const std::string& scene_name) {
    ESP_LOGI(TAG, "TriggerScene: %s", scene_name.c_str());
    return network_client_.TriggerScene(scene_name);
}

std::string SmartHomeHub::GetDeviceStatusJson() {
    cJSON* root = cJSON_CreateArray();
    for (const auto& dev : devices_) {
        cJSON* obj = cJSON_CreateObject();
        cJSON_AddStringToObject(obj, "id", dev.id.c_str());
        cJSON_AddStringToObject(obj, "name", dev.name.c_str());
        cJSON_AddBoolToObject(obj, "state", dev.state);
        cJSON_AddNumberToObject(obj, "level", dev.level);
        if (dev.type == DeviceType::Climate || dev.type == DeviceType::Sensor) {
            cJSON_AddNumberToObject(obj, "temperature", dev.current_temp);
            cJSON_AddNumberToObject(obj, "humidity", dev.current_humidity);
        }
        cJSON_AddItemToArray(root, obj);
    }
    char* printed = cJSON_PrintUnformatted(root);
    std::string result = printed ? printed : "[]";
    if (printed) free(printed);
    cJSON_Delete(root);
    return result;
}
