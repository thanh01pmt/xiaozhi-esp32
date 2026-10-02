#ifndef SENSOR_DASHBOARD_SCREEN_H
#define SENSOR_DASHBOARD_SCREEN_H

#include <lvgl.h>
#include <esp_timer.h>

class SensorDashboardScreen {
public:
    SensorDashboardScreen();
    ~SensorDashboardScreen();

    void Initialize(lv_display_t* display);
    void Show();
    void Hide();
    bool IsVisible() const { return visible_; }
    void UpdateTelemetry();

private:
    void CreateUI();
    static void OnUpdateTimer(void* arg);
    static void OnAutoReturnTimeout(void* arg);

    lv_display_t* display_ = nullptr;
    lv_obj_t* main_screen_ = nullptr;
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

    esp_timer_handle_t update_timer_ = nullptr;
    esp_timer_handle_t auto_return_timer_ = nullptr;
    bool visible_ = false;
};

#endif // SENSOR_DASHBOARD_SCREEN_H
