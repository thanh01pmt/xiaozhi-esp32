#ifndef SENSOR_DASHBOARD_SCREEN_H
#define SENSOR_DASHBOARD_SCREEN_H

#include <lvgl.h>
#include <esp_timer.h>
#include <functional>
#include <string>
#include "shh_overlay_screen.h"

class SensorDashboardScreen : public ShhOverlayScreen {
public:
    // Opened by a tap on one of the six cards; the owner maps the id to a screen.
    using OpenCardCallback = std::function<void(const std::string& sensor_type)>;

    SensorDashboardScreen();
    ~SensorDashboardScreen();

    void Initialize(lv_display_t* display);
    void SetOpenCardCallback(OpenCardCallback callback) { open_card_ = std::move(callback); }
    void Show();
    void Hide();
    void UpdateTelemetry();

private:
    void CreateUI();
    void MakeCardTappable(lv_obj_t* card, const char* sensor_type);
    static void OnUpdateTimer(void* arg);
    static void OnAutoReturnTimeout(void* arg);

    OpenCardCallback open_card_;
    lv_obj_t* screen_ = nullptr;

    // Header
    lv_obj_t* date_label_ = nullptr;
    lv_obj_t* clock_label_ = nullptr;
    // Battery
    lv_obj_t* bat_value_ = nullptr;
    lv_obj_t* bat_state_ = nullptr;
    lv_obj_t* bat_bar_ = nullptr;
    // Temperature / light
    lv_obj_t* temp_value_ = nullptr;
    lv_obj_t* temp_bar_ = nullptr;
    lv_obj_t* light_value_ = nullptr;
    lv_obj_t* light_bar_ = nullptr;
    // IMU
    lv_obj_t* acc_value_ = nullptr;
    lv_obj_t* gyr_value_ = nullptr;
    // Posture
    lv_obj_t* posture_tilt_ = nullptr;
    lv_obj_t* posture_face_ = nullptr;
    lv_obj_t* prox_label_ = nullptr;
    // Wi-Fi / system / uptime
    lv_obj_t* wifi_ssid_ = nullptr;
    lv_obj_t* wifi_rssi_ = nullptr;
    lv_obj_t* wifi_bars_[4] = {nullptr, nullptr, nullptr, nullptr};
    lv_obj_t* sys_sram_ = nullptr;
    lv_obj_t* sys_psram_ = nullptr;
    lv_obj_t* sys_sram_bar_ = nullptr;
    lv_obj_t* sys_psram_bar_ = nullptr;
    lv_obj_t* sys_cpu_ = nullptr;
    lv_obj_t* uptime_value_ = nullptr;

    // Change detection to reduce LVGL redraws
    int last_bat_pct_{-1};
    float last_temp_c_{-1000.0f};
    uint32_t last_lux_{0xffffffff};
    uint16_t last_prox_{0xffff};
    std::string last_ssid_;
    int last_rssi_{-200};
    uint32_t last_sram_kb_{0xffffffff};
    uint32_t last_psram_mb_{0xffffffff};
    uint32_t last_cpu_mhz_{0xffffffff};
    uint32_t last_uptime_sec_{0xffffffff};
    uint32_t last_uptime_min_{0xffffffff};
    int last_sram_pct_{-1};
    int last_psram_pct_{-1};
    int last_temp_pct_{-1};
    int last_lux_pct_{-1};
    bool last_motion_ok_{false};
    bool last_light_ok_{false};
    int last_date_min_{-1};

    esp_timer_handle_t update_timer_ = nullptr;
    esp_timer_handle_t auto_return_timer_ = nullptr;
};

#endif // SENSOR_DASHBOARD_SCREEN_H
