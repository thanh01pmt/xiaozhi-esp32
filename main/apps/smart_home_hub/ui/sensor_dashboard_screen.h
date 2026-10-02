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
    lv_obj_t* pwr_label_ = nullptr;
    lv_obj_t* temp_label_ = nullptr;
    lv_obj_t* net_label_ = nullptr;
    lv_obj_t* sys_label_ = nullptr;

    esp_timer_handle_t update_timer_ = nullptr;
    esp_timer_handle_t auto_return_timer_ = nullptr;
    bool visible_ = false;
};

#endif // SENSOR_DASHBOARD_SCREEN_H
