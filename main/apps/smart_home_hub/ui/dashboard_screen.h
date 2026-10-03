#ifndef SMART_HOME_DASHBOARD_SCREEN_H
#define SMART_HOME_DASHBOARD_SCREEN_H

#include <vector>
#include <string>
#include <lvgl.h>
#include <esp_timer.h>
#include "device_model.h"

class DashboardScreen {
public:
    DashboardScreen();
    ~DashboardScreen();

    using DeviceToggleCallback = std::function<void(const std::string& device_id, bool new_state)>;

    void Initialize(lv_display_t* display);
    void SetToggleCallback(DeviceToggleCallback cb) { toggle_cb_ = std::move(cb); }
    void Show();
    void Hide();
    bool IsVisible() const { return is_visible_; }
    void UpdateDeviceState(const SmartDevice& device);
    void ReloadDevices(const std::vector<SmartDevice>& devices);

private:
    lv_display_t* display_ = nullptr;
    lv_obj_t* main_screen_ = nullptr;
    lv_obj_t* home_screen_ = nullptr;
    lv_obj_t* header_label_ = nullptr;
    lv_obj_t* grid_container_ = nullptr;
    bool is_visible_ = false;
    esp_timer_handle_t auto_return_timer_ = nullptr;
    DeviceToggleCallback toggle_cb_;
    std::vector<SmartDevice> current_devices_;

    void CreateUI();
    void ResetAutoReturnTimer();
    static void OnAutoReturnTimeout(void* arg);
};

#endif // SMART_HOME_DASHBOARD_SCREEN_H
