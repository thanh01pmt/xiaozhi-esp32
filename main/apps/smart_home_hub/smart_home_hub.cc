#include "smart_home_hub.h"
#include "mcp_tools.h"
#include "application.h"
#include "settings.h"
#include "board.h"
#include "audio_codec.h"
#include <esp_log.h>
#include <esp_lvgl_port.h>
#include <cJSON.h>

#define TAG "SmartHomeHub"

SmartHomeHub::SmartHomeHub() = default;

void SmartHomeHub::Initialize(lv_display_t* lv_display) {
    if (initialized_) return;
    ESP_LOGI(TAG, "Initializing SmartHomeHub on CoreS3");

    LoadSettings();
    LoadHomeAssistantConfig();
    LoadDevices();
    ble_controller_.Initialize();

    if (lv_display != nullptr) {
        emotion_eye_screen_.Initialize(lv_display);
        dashboard_screen_.Initialize(lv_display);
        dashboard_screen_.SetToggleCallback([this](const std::string& dev_id, bool state) {
            SetDeviceState(dev_id, state);
        });
        sensor_dashboard_screen_.Initialize(lv_display);
        sensor_card_screen_.Initialize(lv_display);
        sensor_card_screen_.SetOpenWifiConfigCallback([this]() {
            ShowWifiConfig();
        });
        camera_preview_screen_.Initialize(lv_display);
        wifi_config_screen_.Initialize(lv_display);
        // A tap on a dashboard card opens that sensor's own screen. The hub
        // stays the one place that knows how to swap screens.
        sensor_dashboard_screen_.SetOpenCardCallback([this](const std::string& sensor_type) {
            ShowSensorCard(sensor_type);
        });

        // One label, re-parented to whichever screen is active when it fires.
        toast_label_ = lv_label_create(lv_screen_active());
        lv_obj_remove_style_all(toast_label_);
        lv_obj_set_width(toast_label_, 280);
        lv_obj_set_style_text_align(toast_label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(toast_label_, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(toast_label_, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_bg_color(toast_label_, lv_color_hex(0x1E2A33), 0);
        lv_obj_set_style_bg_opa(toast_label_, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(toast_label_, 1, 0);
        lv_obj_set_style_border_color(toast_label_, lv_color_hex(0x29D3FF), 0);
        lv_obj_set_style_radius(toast_label_, 6, 0);
        lv_obj_set_style_pad_all(toast_label_, 6, 0);
        lv_obj_align(toast_label_, LV_ALIGN_BOTTOM_MID, 0, -46);
        lv_obj_add_flag(toast_label_, LV_OBJ_FLAG_HIDDEN);

        esp_timer_create_args_t toast_args = {
            .callback = &SmartHomeHub::OnToastTimeout,
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "sh_toast",
            .skip_unhandled_events = true,
        };
        esp_timer_create(&toast_args, &toast_timer_);

        esp_timer_create_args_t defer_args = {
            .callback = &SmartHomeHub::OnDeferredSwitchTimeout,
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "sh_defer_switch",
            .skip_unhandled_events = true,
        };
        esp_timer_create(&defer_args, &deferred_switch_timer_);

        // Do not load eye_screen_ here: this runs from the board constructor,
        // before LcdDisplay::SetupUI() builds its UI on lv_screen_active().
        // Application::Initialize() shows the default screen after SetupUI().
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
        wifi_config_screen_.Hide();
        dashboard_screen_.Show();
    }
}

void SmartHomeHub::ShowDashboard() {
    sensor_dashboard_screen_.Hide();
    sensor_card_screen_.Hide();
    camera_preview_screen_.Hide();
    wifi_config_screen_.Hide();
    dashboard_screen_.ReloadDevices(devices_);
    dashboard_screen_.Show();
}

void SmartHomeHub::HideDashboard() {
    dashboard_screen_.Hide();
}

void SmartHomeHub::ShowSensorDashboard() {
    dashboard_screen_.Hide();
    sensor_card_screen_.Hide();
    camera_preview_screen_.Hide();
    wifi_config_screen_.Hide();
    sensor_dashboard_screen_.Show();
}

void SmartHomeHub::HideSensorDashboard() {
    sensor_dashboard_screen_.Hide();
}

void SmartHomeHub::OnToastTimeout(void* arg) {
    auto hub = static_cast<SmartHomeHub*>(arg);
    if (hub == nullptr) return;
    // Hiding touches LVGL, so it must not run on the shared esp_timer task.
    Application::GetInstance().Schedule([hub]() {
        if (lvgl_port_lock(200)) {
            if (hub->toast_label_ != nullptr) {
                lv_obj_add_flag(hub->toast_label_, LV_OBJ_FLAG_HIDDEN);
            }
            lvgl_port_unlock();
        }
    });
}

void SmartHomeHub::ShowToast(const std::string& text) {
    if (toast_label_ == nullptr) return;
    if (!lvgl_port_lock(200)) return;

    // Re-parent so the toast lands on the screen the user is actually looking
    // at, whichever of the four it happens to be.
    lv_obj_t* active = lv_screen_active();
    if (active != nullptr && lv_obj_get_parent(toast_label_) != active) {
        lv_obj_set_parent(toast_label_, active);
    }
    lv_label_set_text(toast_label_, text.c_str());
    lv_obj_clear_flag(toast_label_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(toast_label_);
    lvgl_port_unlock();

    if (toast_timer_ != nullptr) {
        esp_timer_stop(toast_timer_);
        esp_timer_start_once(toast_timer_, 2500 * 1000);
    }
}

void SmartHomeHub::ShowSensorCard(const std::string& sensor_type) {
    dashboard_screen_.Hide();
    sensor_dashboard_screen_.Hide();
    camera_preview_screen_.Hide();
    wifi_config_screen_.Hide();

    SensorCardType type = SensorCardType::Temperature;
    bool matched = sensor_type == "temperature" || sensor_type == "temp" || sensor_type == "nhiet_do";
    if (sensor_type == "battery" || sensor_type == "pin" || sensor_type == "power" || sensor_type == "sac") {
        type = SensorCardType::Battery;
        matched = true;
    } else if (sensor_type == "light" || sensor_type == "lux" || sensor_type == "anh_sang" || sensor_type == "als") {
        type = SensorCardType::Light;
        matched = true;
    } else if (sensor_type == "motion" || sensor_type == "imu" || sensor_type == "chuyen_dong" || sensor_type == "tu_the") {
        type = SensorCardType::Motion;
        matched = true;
    } else if (sensor_type == "peripherals" || sensor_type == "peripheral" || sensor_type == "ports" || sensor_type == "port" || sensor_type == "ngoai_vi") {
        type = SensorCardType::Peripherals;
        matched = true;
    } else if (sensor_type == "network" || sensor_type == "wifi" || sensor_type == "mang") {
        type = SensorCardType::Network;
        matched = true;
    } else if (sensor_type == "system" || sensor_type == "ram" || sensor_type == "memory" || sensor_type == "he_thong") {
        type = SensorCardType::System;
        matched = true;
    }
    // Falling through to temperature silently answered a question about the
    // battery with the wrong screen, which reads as the app ignoring the user.
    if (!matched) {
        ESP_LOGW(TAG, "ShowSensorCard: unknown sensor '%s', falling back to temperature",
                 sensor_type.c_str());
        ShowToast("Khong ro cam bien: " + sensor_type);
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
    emotion_eye_screen_.Hide();
    wifi_config_screen_.Hide();
    camera_preview_screen_.Show();
}

void SmartHomeHub::HideCameraPreview() {
    camera_preview_screen_.Hide();
}

void SmartHomeHub::ShowEmotionEyes() {
    dashboard_screen_.Hide();
    sensor_dashboard_screen_.Hide();
    sensor_card_screen_.Hide();
    camera_preview_screen_.Hide();
    wifi_config_screen_.Hide();
    emotion_eye_screen_.Show();
}

void SmartHomeHub::HideEmotionEyes() {
    emotion_eye_screen_.Hide();
}

void SmartHomeHub::ShowWifiConfig() {
    dashboard_screen_.Hide();
    sensor_dashboard_screen_.Hide();
    sensor_card_screen_.Hide();
    camera_preview_screen_.Hide();
    emotion_eye_screen_.Hide();
    wifi_config_screen_.Show();
}

void SmartHomeHub::HideWifiConfig() {
    wifi_config_screen_.Hide();
}

void SmartHomeHub::SetEmotionEyes(EyeEmotion emotion) {
    if (!dashboard_screen_.IsVisible() && !sensor_dashboard_screen_.IsVisible() &&
        !sensor_card_screen_.IsVisible() && !camera_preview_screen_.IsVisible() &&
        !wifi_config_screen_.IsVisible()) {
        if (!emotion_eye_screen_.IsVisible()) {
            emotion_eye_screen_.Show();
        }
    }
    emotion_eye_screen_.SetEmotion(emotion);
}

void SmartHomeHub::SetEmotionEyesByName(const std::string& name) {
    if (!dashboard_screen_.IsVisible() && !sensor_dashboard_screen_.IsVisible() &&
        !sensor_card_screen_.IsVisible() && !camera_preview_screen_.IsVisible() &&
        !wifi_config_screen_.IsVisible()) {
        if (!emotion_eye_screen_.IsVisible()) {
            emotion_eye_screen_.Show();
        }
    }
    emotion_eye_screen_.SetEmotionByName(name);
}

void SmartHomeHub::OnDeferredSwitchTimeout(void* arg) {
    auto hub = static_cast<SmartHomeHub*>(arg);
    if (hub == nullptr) return;
    Application::GetInstance().Schedule([hub]() {
        hub->ExecutePendingScreenSwitch();
    });
}

void SmartHomeHub::ScheduleScreenSwitch(const std::string& screen_name, uint32_t delay_ms) {
    pending_screen_switch_ = screen_name;
    ESP_LOGI(TAG, "ScheduleScreenSwitch: '%s' scheduled with max %lu ms delay", screen_name.c_str(), delay_ms);
    // Keep or set eye emotion to Thinking while LLM processes
    SetEmotionEyes(EyeEmotion::Thinking);

    if (deferred_switch_timer_ != nullptr) {
        esp_timer_stop(deferred_switch_timer_);
        esp_timer_start_once(deferred_switch_timer_, delay_ms * 1000);
    }
}

void SmartHomeHub::ExecutePendingScreenSwitch() {
    if (deferred_switch_timer_ != nullptr) {
        esp_timer_stop(deferred_switch_timer_);
    }
    if (!pending_screen_switch_.empty()) {
        std::string target = pending_screen_switch_;
        pending_screen_switch_.clear();
        ESP_LOGI(TAG, "Executing pending screen switch -> %s", target.c_str());
        SwitchScreen(target);
    }
}

void SmartHomeHub::LoadSettings() {
    Settings settings("smarthome", true);
    default_screen_mode_ = DefaultScreenMode::Eyes;
    settings.SetString("def_screen", "eyes");
    ESP_LOGI(TAG, "Default screen mode enforced: eyes (animation)");
}

void SmartHomeHub::SetDefaultScreenMode(DefaultScreenMode mode) {
    default_screen_mode_ = mode;
    Settings settings("smarthome", true);
    settings.SetString("def_screen", (mode == DefaultScreenMode::Eyes) ? "eyes" : "chat");
    ESP_LOGI(TAG, "Saved default screen mode: %s", (mode == DefaultScreenMode::Eyes) ? "eyes" : "chat");
}

void SmartHomeHub::ReturnToDefaultScreen() {
    ShowEmotionEyes();
    SetEmotionEyes(EyeEmotion::Idle);
}

bool SmartHomeHub::SwitchScreen(const std::string& screen_name) {
    ESP_LOGI(TAG, "SwitchScreen requested: %s", screen_name.c_str());
    if (screen_name == "eyes" || screen_name == "eye" || screen_name == "mat" || screen_name == "bieu_cam" ||
        screen_name == "emotion" || screen_name == "animation" || screen_name == "main" ||
        screen_name == "xiaozhi" || screen_name == "chinh" || screen_name == "tro_ly") {
        ShowEmotionEyes();
        return true;
    } else if (screen_name == "sensors" || screen_name == "sensor" || screen_name == "cam_bien" || screen_name == "telemetry") {
        emotion_eye_screen_.Hide();
        ShowSensorDashboard();
        return true;
    } else if (screen_name == "temperature" || screen_name == "nhiet_do") {
        emotion_eye_screen_.Hide();
        ShowSensorCard("temperature");
        return true;
    } else if (screen_name == "battery" || screen_name == "pin") {
        emotion_eye_screen_.Hide();
        ShowSensorCard("battery");
        return true;
    } else if (screen_name == "light" || screen_name == "lux" || screen_name == "anh_sang") {
        emotion_eye_screen_.Hide();
        ShowSensorCard("light");
        return true;
    } else if (screen_name == "motion" || screen_name == "imu" || screen_name == "chuyen_dong") {
        emotion_eye_screen_.Hide();
        ShowSensorCard("motion");
        return true;
    } else if (screen_name == "peripherals" || screen_name == "peripheral" || screen_name == "ports" || screen_name == "port" || screen_name == "ngoai_vi") {
        emotion_eye_screen_.Hide();
        ShowSensorCard("peripherals");
        return true;
    } else if (screen_name == "network" || screen_name == "wifi" || screen_name == "mang") {
        emotion_eye_screen_.Hide();
        ShowSensorCard("network");
        return true;
    } else if (screen_name == "system" || screen_name == "ram" || screen_name == "he_thong") {
        emotion_eye_screen_.Hide();
        ShowSensorCard("system");
        return true;
    } else if (screen_name == "wifi_config" || screen_name == "wifi_scan" || screen_name == "scan_wifi" || screen_name == "wifi_connect") {
        emotion_eye_screen_.Hide();
        ShowWifiConfig();
        return true;
    } else if (screen_name == "smarthome" || screen_name == "home" || screen_name == "dashboard" || screen_name == "nha_thong_minh") {
        emotion_eye_screen_.Hide();
        ShowDashboard();
        return true;
    } else if (screen_name == "camera" || screen_name == "may_anh" || screen_name == "chup_hinh" || screen_name == "cam" || screen_name == "live" || screen_name == "xem_truoc") {
        emotion_eye_screen_.Hide();
        ShowCameraPreview();
        return true;
    } else if (screen_name == "chat" || screen_name == "man_hinh_chat" || screen_name == "text") {
        HideDashboard();
        HideSensorDashboard();
        HideSensorCard();
        HideCameraPreview();
        HideEmotionEyes();
        HideWifiConfig();
        return true;
    } else if (screen_name == "default" || screen_name == "mac_dinh" || screen_name == "quay_lai") {
        ReturnToDefaultScreen();
        return true;
    }

    // Only an unknown name gets a toast: a successful switch is already
    // visible, so confirming it out loud would just be noise.
    ESP_LOGW(TAG, "SwitchScreen: unknown screen '%s'", screen_name.c_str());
    ShowToast("Khong co man hinh: " + screen_name);
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

    cJSON* s5 = cJSON_CreateObject();
    cJSON_AddStringToObject(s5, "id", "wifi_config");
    cJSON_AddStringToObject(s5, "name", "Màn hình Quét & Kết nối Wi-Fi");
    cJSON_AddStringToObject(s5, "description", "Màn hình quét danh sách mạng Wi-Fi xung quanh, nhập mật khẩu kết nối hoặc bật Web Portal AP mode.");
    cJSON_AddItemToArray(root, s5);

    // Single-sensor cards.
    struct CardScreen {
        const char* id;
        const char* name;
        const char* description;
    };
    static const CardScreen kCards[] = {
        {"temperature", "Màn hình Nhiệt độ", "Thẻ đơn nhiệt độ bo mạch theo thời gian thực."},
        {"battery", "Màn hình Pin & Sạc", "Thẻ đơn mức pin, trạng thái sạc và nhiệt độ."},
        {"light", "Màn hình Ánh sáng", "Thẻ đơn cường độ ánh sáng (Lux) và cảm biến tiếm cận."},
        {"motion", "Màn hình Cảm biến IMU", "Thẻ đơn gia tốc, con quay 6 trục và góc nghiêng máy."},
        {"peripherals", "Màn hình Ngoại vi & Cổng kết nối", "Thẻ đơn trạng thái chip I2C nội bộ và các cổng Port A, B, C, nguồn 5V."},
        {"network", "Màn hình Wi-Fi", "Thẻ đơn SSID, địa chỉ IP, cường độ tín hiệu."},
        {"system", "Màn hình Hệ thống", "Thẻ đơn RAM, PSRAM, tần số CPU và thời gian hoạt động."},
    };
    for (const CardScreen& card : kCards) {
        cJSON* entry = cJSON_CreateObject();
        cJSON_AddStringToObject(entry, "id", card.id);
        cJSON_AddStringToObject(entry, "name", card.name);
        cJSON_AddStringToObject(entry, "description", card.description);
        cJSON_AddItemToArray(root, entry);
    }

    char* str = cJSON_PrintUnformatted(root);
    std::string res = str ? str : "[]";
    cJSON_free(str);
    cJSON_Delete(root);
    return res;
}

bool SmartHomeHub::SetDeviceState(const std::string& device_id, bool turn_on) {
    ESP_LOGI(TAG, "SetDeviceState: %s -> %d", device_id.c_str(), turn_on);
    std::string target_id = device_id;

    // Check if device_id matches an id or friendly name in discovered devices
    for (auto& dev : devices_) {
        if (dev.id == device_id || dev.name == device_id) {
            dev.state = turn_on;
            target_id = dev.id;
            dashboard_screen_.UpdateDeviceState(dev);
            break;
        }
    }

    // If only one switch/light exists in devices_ and user calls generic name like "den" or "den_phong_khach"
    if (target_id == device_id && devices_.size() == 1) {
        target_id = devices_[0].id;
        devices_[0].state = turn_on;
        dashboard_screen_.UpdateDeviceState(devices_[0]);
    } else if (target_id == device_id) {
        // Look for partial match
        for (auto& dev : devices_) {
            if (dev.id.find(device_id) != std::string::npos ||
                dev.name.find(device_id) != std::string::npos) {
                target_id = dev.id;
                dev.state = turn_on;
                dashboard_screen_.UpdateDeviceState(dev);
                break;
            }
        }
    }

    return network_client_.SendSwitchCommand(target_id, turn_on);
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

void SmartHomeHub::LoadHomeAssistantConfig() {
    Settings settings("smarthome", false);
    std::string url = settings.GetString("ha_url", "");
    std::string token = settings.GetString("ha_token", "");
    if (!url.empty()) {
        network_client_.SetHomeAssistantConfig(url, token);
        ESP_LOGI(TAG, "Loaded Home Assistant configuration from NVS: %s", url.c_str());
    } else {
        ESP_LOGI(TAG, "No Home Assistant configuration found in NVS");
    }
}

void SmartHomeHub::SaveHomeAssistantConfig(const std::string& base_url, const std::string& access_token) {
    Settings settings("smarthome", true);
    settings.SetString("ha_url", base_url);
    settings.SetString("ha_token", access_token);
    network_client_.SetHomeAssistantConfig(base_url, access_token);
    ESP_LOGI(TAG, "Saved Home Assistant configuration to NVS: %s", base_url.c_str());
    SyncDevicesFromHomeAssistant();
}

bool SmartHomeHub::SyncDevicesFromHomeAssistant() {
    xTaskCreate([](void* arg) {
        auto self = static_cast<SmartHomeHub*>(arg);
        std::vector<SmartDevice> ha_devices;
        bool success = self->network_client_.FetchEntitiesFromHomeAssistant(ha_devices);
        if (success && !ha_devices.empty()) {
            self->devices_ = std::move(ha_devices);
            ESP_LOGI(TAG, "SyncDevicesFromHomeAssistant: successfully updated %u devices",
                     (unsigned)self->devices_.size());
            Application::GetInstance().Schedule([self]() {
                self->dashboard_screen_.ReloadDevices(self->devices_);
            });
        } else {
            ESP_LOGW(TAG, "SyncDevicesFromHomeAssistant failed or returned 0 devices");
        }
        vTaskDelete(NULL);
    }, "ha_sync_task", 8192, this, 3, NULL);
    return true;
}

bool SmartHomeHub::PlayAudioStream(const std::string& url, const std::string& title) {
    ESP_LOGI(TAG, "PlayAudioStream: %s (%s)", title.c_str(), url.c_str());
    ShowToast("Đang phát (65%): " + title);

    Application::GetInstance().Schedule([url, title]() mutable {
        auto& app = Application::GetInstance();
        // Giữ âm lượng ở mức 65% khi phát nhạc để tránh bão hòa micro (giúp wake word & tương tác nhận diện được)
        auto codec = Board::GetInstance().GetAudioCodec();
        if (codec) {
            codec->SetOutputVolume(65);
            ESP_LOGI(TAG, "Default music volume set to 65%%");
        }

        // Ngat loi noi cua AI va chuyen ve Idle de NotifyPlayer chap nhan stream
        app.AbortSpeaking(kAbortReasonNone);
        app.SetDeviceState(kDeviceStateIdle);

        std::vector<NotifySubtitle> subtitles;
        subtitles.push_back({.start_ms = 0, .text = "🎵 " + title});
        app.StartNotification(std::move(url), std::move(subtitles));
    });

    return true;
}

bool SmartHomeHub::StopAudioStream() {
    ESP_LOGI(TAG, "StopAudioStream requested");
    Application::GetInstance().StopNotification();
    ShowToast("Đã dừng phát");
    return true;
}

bool SmartHomeHub::PlayHomeAssistantMedia(const std::string& entity_id, const std::string& media_url, const std::string& media_type) {
    ESP_LOGI(TAG, "PlayHomeAssistantMedia: entity=%s url=%s type=%s",
             entity_id.c_str(), media_url.c_str(), media_type.c_str());
    ShowToast("Phát qua HA: " + entity_id);
    return network_client_.SendMediaPlayCommand(entity_id, media_url, media_type);
}

bool SmartHomeHub::StopHomeAssistantMedia(const std::string& entity_id) {
    ESP_LOGI(TAG, "StopHomeAssistantMedia: entity=%s", entity_id.c_str());
    ShowToast("Dừng phát HA");
    return network_client_.SendMediaStopCommand(entity_id);
}
