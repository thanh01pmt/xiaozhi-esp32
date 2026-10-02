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
        sensor_dashboard_screen_.Initialize(lv_display);
        sensor_card_screen_.Initialize(lv_display);
        camera_preview_screen_.Initialize(lv_display);
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
        sensor_dashboard_screen_.Hide();
        sensor_card_screen_.Hide();
        camera_preview_screen_.Hide();
        dashboard_screen_.Show();
    }
}

void SmartHomeHub::ShowDashboard() {
    sensor_dashboard_screen_.Hide();
    sensor_card_screen_.Hide();
    camera_preview_screen_.Hide();
    dashboard_screen_.Show();
}

void SmartHomeHub::HideDashboard() {
    dashboard_screen_.Hide();
}

void SmartHomeHub::ShowSensorDashboard() {
    dashboard_screen_.Hide();
    sensor_card_screen_.Hide();
    camera_preview_screen_.Hide();
    sensor_dashboard_screen_.Show();
}

void SmartHomeHub::HideSensorDashboard() {
    sensor_dashboard_screen_.Hide();
}

void SmartHomeHub::ShowSensorCard(const std::string& sensor_type) {
    dashboard_screen_.Hide();
    sensor_dashboard_screen_.Hide();
    camera_preview_screen_.Hide();

    SensorCardType type = SensorCardType::Temperature;
    if (sensor_type == "battery" || sensor_type == "pin" || sensor_type == "power" || sensor_type == "sac") {
        type = SensorCardType::Battery;
    } else if (sensor_type == "light" || sensor_type == "lux" || sensor_type == "anh_sang" || sensor_type == "als") {
        type = SensorCardType::Light;
    } else if (sensor_type == "motion" || sensor_type == "imu" || sensor_type == "chuyen_dong" || sensor_type == "tu_the") {
        type = SensorCardType::Motion;
    } else if (sensor_type == "network" || sensor_type == "wifi" || sensor_type == "mang") {
        type = SensorCardType::Network;
    } else if (sensor_type == "system" || sensor_type == "ram" || sensor_type == "memory" || sensor_type == "he_thong") {
        type = SensorCardType::System;
    }

    sensor_card_screen_.Show(type);
}

void SmartHomeHub::HideSensorCard() {
    sensor_card_screen_.Hide();
}

void SmartHomeHub::ShowCameraPreview() {
    dashboard_screen_.Hide();
    sensor_dashboard_screen_.Hide();
    sensor_card_screen_.Hide();
    camera_preview_screen_.Show();
}

void SmartHomeHub::HideCameraPreview() {
    camera_preview_screen_.Hide();
}

bool SmartHomeHub::SwitchScreen(const std::string& screen_name) {
    ESP_LOGI(TAG, "SwitchScreen requested: %s", screen_name.c_str());
    if (screen_name == "sensors" || screen_name == "sensor" || screen_name == "cam_bien" || screen_name == "telemetry") {
        ShowSensorDashboard();
        return true;
    } else if (screen_name == "temperature" || screen_name == "nhiet_do") {
        ShowSensorCard("temperature");
        return true;
    } else if (screen_name == "battery" || screen_name == "pin") {
        ShowSensorCard("battery");
        return true;
    } else if (screen_name == "light" || screen_name == "lux" || screen_name == "anh_sang") {
        ShowSensorCard("light");
        return true;
    } else if (screen_name == "motion" || screen_name == "imu" || screen_name == "chuyen_dong") {
        ShowSensorCard("motion");
        return true;
    } else if (screen_name == "smarthome" || screen_name == "home" || screen_name == "dashboard" || screen_name == "nha_thong_minh") {
        ShowDashboard();
        return true;
    } else if (screen_name == "camera" || screen_name == "may_anh" || screen_name == "chup_hinh" || screen_name == "cam" || screen_name == "live" || screen_name == "xem_truoc") {
        ShowCameraPreview();
        return true;
    } else if (screen_name == "main" || screen_name == "xiaozhi" || screen_name == "chinh" || screen_name == "tro_ly") {
        HideDashboard();
        HideSensorDashboard();
        HideSensorCard();
        HideCameraPreview();
        return true;
    }
    return false;
}

std::string SmartHomeHub::ListScreensJson() {
    cJSON* root = cJSON_CreateArray();

    cJSON* s1 = cJSON_CreateObject();
    cJSON_AddStringToObject(s1, "id", "main");
    cJSON_AddStringToObject(s1, "name", "Màn hình Trợ lý chính (XiaoZhi)");
    cJSON_AddStringToObject(s1, "description", "Màn hình hội thoại chính với biểu cảm khuôn mặt AI và trạng thái hệ thống.");
    cJSON_AddItemToArray(root, s1);

    cJSON* s2 = cJSON_CreateObject();
    cJSON_AddStringToObject(s2, "id", "sensors");
    cJSON_AddStringToObject(s2, "name", "Màn hình Cảm biến & Thông số máy (Sensors)");
    cJSON_AddStringToObject(s2, "description", "Màn hình hiển thị chi tiết số liệu pin, sạc, nhiệt độ bo mạch, ánh sáng môi trường (Lux), cảm biến chuyển động IMU, Wi-Fi và bộ nhớ RAM.");
    cJSON_AddItemToArray(root, s2);

    cJSON* s3 = cJSON_CreateObject();
    cJSON_AddStringToObject(s3, "id", "smarthome");
    cJSON_AddStringToObject(s3, "name", "Màn hình Nhà thông minh (SmartHome)");
    cJSON_AddStringToObject(s3, "description", "Màn hình cảm ứng điều khiển đèn, quạt trần và máy lạnh.");
    cJSON_AddItemToArray(root, s3);

    cJSON* s4 = cJSON_CreateObject();
    cJSON_AddStringToObject(s4, "id", "camera");
    cJSON_AddStringToObject(s4, "name", "Màn hình Xem trước Camera Trực tiếp (Live Camera Preview)");
    cJSON_AddStringToObject(s4, "description", "Mở chế độ xem trước video camera trực tiếp (real-time live stream) trước khi chụp ảnh hoặc hỏi AI.");
    cJSON_AddItemToArray(root, s4);

    char* str = cJSON_PrintUnformatted(root);
    std::string res = str ? str : "[]";
    cJSON_free(str);
    cJSON_Delete(root);
    return res;
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
