#ifndef SMART_HOME_HUB_H
#define SMART_HOME_HUB_H

#include <vector>
#include <string>
#include <memory>
#include <lvgl.h>
#include "device_model.h"
#include "network_client.h"
#include "ble_controller.h"
#include "ui/dashboard_screen.h"
#include "ui/sensor_dashboard_screen.h"
#include "ui/sensor_card_screen.h"
#include "ui/camera_preview_screen.h"
#include "ui/emotion_eye_screen.h"
#include "web_config_server.h"

enum class DefaultScreenMode {
    Eyes,
    Chat
};

class SmartHomeHub {
public:
    static SmartHomeHub& GetInstance() {
        static SmartHomeHub instance;
        return instance;
    }

    void Initialize(lv_display_t* lv_display = nullptr);
    void ToggleDashboard();
    void ShowDashboard();
    void HideDashboard();

    void ShowSensorDashboard();
    void HideSensorDashboard();

    void ShowSensorCard(const std::string& sensor_type);
    void HideSensorCard();

    void ShowCameraPreview();
    void HideCameraPreview();

    void ShowEmotionEyes();
    void HideEmotionEyes();
    void SetEmotionEyes(EyeEmotion emotion);
    void SetEmotionEyesByName(const std::string& name);

    // Screen navigation
    bool SwitchScreen(const std::string& screen_name);
    void ScheduleScreenSwitch(const std::string& screen_name, uint32_t delay_ms = 2000);
    void ExecutePendingScreenSwitch();
    std::string ListScreensJson();

    // Default screen preference
    void SetDefaultScreenMode(DefaultScreenMode mode);
    DefaultScreenMode GetDefaultScreenMode() const { return default_screen_mode_; }
    void ReturnToDefaultScreen();

    // Short confirmation on whatever screen is active, then fades out.
    void ShowToast(const std::string& text);

    bool SetDeviceState(const std::string& device_id, bool turn_on);
    bool SetDeviceLevel(const std::string& device_id, int level);
    bool TriggerScene(const std::string& scene_name);
    std::string GetDeviceStatusJson();

    void LoadHomeAssistantConfig();
    void SaveHomeAssistantConfig(const std::string& base_url, const std::string& access_token);
    bool SyncDevicesFromHomeAssistant();

    bool PlayAudioStream(const std::string& url, const std::string& title);
    bool StopAudioStream();
    bool PlayHomeAssistantMedia(const std::string& entity_id, const std::string& media_url, const std::string& media_type);
    bool StopHomeAssistantMedia(const std::string& entity_id);

    const std::vector<SmartDevice>& GetDevices() const { return devices_; }
    DashboardScreen& GetDashboardScreen() { return dashboard_screen_; }
    SensorDashboardScreen& GetSensorDashboard() { return sensor_dashboard_screen_; }
    SensorCardScreen& GetSensorCard() { return sensor_card_screen_; }
    CameraPreviewScreen& GetCameraPreview() { return camera_preview_screen_; }
    EmotionEyeScreen& GetEmotionEyeScreen() { return emotion_eye_screen_; }
    SmartHomeNetworkClient& GetNetworkClient() { return network_client_; }
    SmartHomeBleController& GetBleController() { return ble_controller_; }
    WebConfigServer& GetWebServer() { return web_server_; }

private:
    SmartHomeHub();
    ~SmartHomeHub() = default;

    static void OnToastTimeout(void* arg);
    static void OnDeferredSwitchTimeout(void* arg);

    bool initialized_ = false;
    SmartHomeNetworkClient network_client_;
    SmartHomeBleController ble_controller_;
    DashboardScreen dashboard_screen_;
    SensorDashboardScreen sensor_dashboard_screen_;
    SensorCardScreen sensor_card_screen_;
    CameraPreviewScreen camera_preview_screen_;
    EmotionEyeScreen emotion_eye_screen_;
    WebConfigServer web_server_;
    std::vector<SmartDevice> devices_;

    DefaultScreenMode default_screen_mode_ = DefaultScreenMode::Eyes;
    std::string pending_screen_switch_ = "";
    esp_timer_handle_t deferred_switch_timer_ = nullptr;

    lv_obj_t* toast_label_ = nullptr;
    esp_timer_handle_t toast_timer_ = nullptr;

    void LoadDevices();
    void LoadSettings();
};

#endif // SMART_HOME_HUB_H
