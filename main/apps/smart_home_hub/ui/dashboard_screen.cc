#include "dashboard_screen.h"
#include "application.h"
#include <esp_log.h>
#include <esp_lvgl_port.h>

#define TAG "SH_Dashboard"

DashboardScreen::DashboardScreen() = default;

DashboardScreen::~DashboardScreen() {
    if (auto_return_timer_ != nullptr) {
        esp_timer_stop(auto_return_timer_);
        esp_timer_delete(auto_return_timer_);
    }
}

void DashboardScreen::OnAutoReturnTimeout(void* arg) {
    auto self = static_cast<DashboardScreen*>(arg);
    if (self && self->IsVisible()) {
        // Hide() takes the LVGL lock; the shared esp_timer task must not block on it.
        Application::GetInstance().Schedule([self]() {
            if (self->IsVisible()) {
                ESP_LOGI(TAG, "Auto-return to XiaoZhi main screen");
                self->Hide();
            }
        });
    }
}

void DashboardScreen::Initialize(lv_display_t* display) {
    display_ = display;

    esp_timer_create_args_t timer_args = {
        .callback = &DashboardScreen::OnAutoReturnTimeout,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "sh_auto_return",
        .skip_unhandled_events = true,
    };
    esp_timer_create(&timer_args, &auto_return_timer_);

    if (lvgl_port_lock(100)) {
        CreateUI();
        lvgl_port_unlock();
    }
}

void DashboardScreen::CreateUI() {
    home_screen_ = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(home_screen_, lv_color_hex(0x121212), 0);

    // Header
    header_label_ = lv_label_create(home_screen_);
    lv_label_set_text(header_label_, "Smart Home Hub");
    lv_obj_set_style_text_color(header_label_, lv_color_hex(0x00E5FF), 0);
    lv_obj_align(header_label_, LV_ALIGN_TOP_MID, 0, 8);

    // Grid Container
    grid_container_ = lv_obj_create(home_screen_);
    lv_obj_set_size(grid_container_, 300, 180);
    lv_obj_align(grid_container_, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_set_style_bg_color(grid_container_, lv_color_hex(0x1E1E1E), 0);
    lv_obj_set_style_border_width(grid_container_, 1, 0);
    lv_obj_set_style_border_color(grid_container_, lv_color_hex(0x333333), 0);
    lv_obj_set_style_radius(grid_container_, 8, 0);
    lv_obj_set_flex_flow(grid_container_, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid_container_, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Card 1: Den Phong Khach
    lv_obj_t* btn1 = lv_button_create(grid_container_);
    lv_obj_set_size(btn1, 130, 70);
    lv_obj_set_style_bg_color(btn1, lv_color_hex(0x2A2A2A), 0);
    lv_obj_t* lbl1 = lv_label_create(btn1);
    lv_label_set_text(lbl1, "Living Light\n[OFF]");
    lv_obj_center(lbl1);

    // Card 2: Dieu hoa
    lv_obj_t* btn2 = lv_button_create(grid_container_);
    lv_obj_set_size(btn2, 130, 70);
    lv_obj_set_style_bg_color(btn2, lv_color_hex(0x2A2A2A), 0);
    lv_obj_t* lbl2 = lv_label_create(btn2);
    lv_label_set_text(lbl2, "AC Temp\n24 C");
    lv_obj_center(lbl2);

    // Back button
    lv_obj_add_event_cb(home_screen_, [](lv_event_t* e) {
        auto self = static_cast<DashboardScreen*>(lv_event_get_user_data(e));
        if (self) {
            self->ResetAutoReturnTimer();
        }
    }, LV_EVENT_CLICKED, this);
}

void DashboardScreen::Show() {
    if (home_screen_ == nullptr) return;
    if (lvgl_port_lock(200)) {
        main_screen_ = lv_screen_active();
        // Synchronous load: an animated load keeps the previous screen active for
        // the whole transition, so the next screen would remember the wrong
        // "main" screen and never come back here.
        lv_screen_load(home_screen_);
        is_visible_ = true;
        ResetAutoReturnTimer();
        lvgl_port_unlock();
    }
}

void DashboardScreen::Hide() {
    if (!is_visible_ || main_screen_ == nullptr) return;
    if (auto_return_timer_ != nullptr) {
        esp_timer_stop(auto_return_timer_);
    }
    if (lvgl_port_lock(200)) {
        lv_screen_load(main_screen_);
        lvgl_port_unlock();
    }
    // Cleared unconditionally: a timed-out lock must not leave the screen stuck
    // "visible" and block every later switch back to main.
    is_visible_ = false;
}

void DashboardScreen::ResetAutoReturnTimer() {
    if (auto_return_timer_ != nullptr) {
        esp_timer_stop(auto_return_timer_);
        esp_timer_start_once(auto_return_timer_, 30 * 1000 * 1000); // 30s
    }
}

void DashboardScreen::UpdateDeviceState(const SmartDevice& device) {
    ESP_LOGI(TAG, "Device state updated: %s -> %d", device.name.c_str(), device.state);
}
