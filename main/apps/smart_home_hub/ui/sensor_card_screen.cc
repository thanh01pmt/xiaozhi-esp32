#include "sensor_card_screen.h"
#include "../sensor_monitor.h"
#include "application.h"
#include <esp_log.h>
#include <esp_lvgl_port.h>

#define TAG "SH_SensorCard"

SensorCardScreen::SensorCardScreen() = default;

SensorCardScreen::~SensorCardScreen() {
    if (update_timer_ != nullptr) {
        esp_timer_stop(update_timer_);
        esp_timer_delete(update_timer_);
    }
    if (auto_return_timer_ != nullptr) {
        esp_timer_stop(auto_return_timer_);
        esp_timer_delete(auto_return_timer_);
    }
}

void SensorCardScreen::OnUpdateTimer(void* arg) {
    auto self = static_cast<SensorCardScreen*>(arg);
    if (self && self->IsVisible()) {
        self->UpdateData();
    }
}

void SensorCardScreen::OnAutoReturnTimeout(void* arg) {
    auto self = static_cast<SensorCardScreen*>(arg);
    if (self && self->IsVisible()) {
        // Hide() takes the LVGL lock; the shared esp_timer task must not block on it.
        Application::GetInstance().Schedule([self]() {
            if (self->IsVisible()) {
                ESP_LOGI(TAG, "Sensor card auto-return timeout");
                self->Hide();
            }
        });
    }
}

void SensorCardScreen::Initialize(lv_display_t* display) {
    display_ = display;

    esp_timer_create_args_t update_args = {
        .callback = &SensorCardScreen::OnUpdateTimer,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "scard_update",
        .skip_unhandled_events = true,
    };
    esp_timer_create(&update_args, &update_timer_);

    esp_timer_create_args_t return_args = {
        .callback = &SensorCardScreen::OnAutoReturnTimeout,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "scard_return",
        .skip_unhandled_events = true,
    };
    esp_timer_create(&return_args, &auto_return_timer_);

    if (lvgl_port_lock(100)) {
        CreateUI();
        lvgl_port_unlock();
    }
}

void SensorCardScreen::CreateUI() {
    screen_ = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_, lv_color_hex(0x0A0A0E), 0);
    lv_obj_clear_flag(screen_, LV_OBJ_FLAG_SCROLLABLE);

    // Dedicated Center Spotlight Card
    card_box_ = lv_obj_create(screen_);
    lv_obj_set_size(card_box_, 290, 180);
    lv_obj_align(card_box_, LV_ALIGN_CENTER, 0, -8);
    lv_obj_set_style_bg_color(card_box_, lv_color_hex(0x161622), 0);
    lv_obj_set_style_border_width(card_box_, 2, 0);
    lv_obj_set_style_border_color(card_box_, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_radius(card_box_, 14, 0);
    lv_obj_set_style_pad_all(card_box_, 12, 0);

    // Title
    title_label_ = lv_label_create(card_box_);
    lv_label_set_text(title_label_, "CAM BIEN");
    lv_obj_set_style_text_color(title_label_, lv_color_hex(0x00E5FF), 0);
    lv_obj_align(title_label_, LV_ALIGN_TOP_LEFT, 0, 0);

    // Big Value Display
    value_label_ = lv_label_create(card_box_);
    lv_label_set_text(value_label_, "--");
    lv_obj_set_style_text_color(value_label_, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(value_label_, LV_ALIGN_CENTER, 0, -10);

    // Secondary Detail Line
    detail_label_ = lv_label_create(card_box_);
    lv_label_set_text(detail_label_, "Dang doc du lieu...");
    lv_obj_set_style_text_color(detail_label_, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(detail_label_, LV_ALIGN_BOTTOM_MID, 0, 0);

    // Footer Hint
    hint_label_ = lv_label_create(screen_);
    lv_label_set_text(hint_label_, "Cham de dong | Tu dong quay ve sau 15s");
    lv_obj_set_style_text_color(hint_label_, lv_color_hex(0x666688), 0);
    lv_obj_align(hint_label_, LV_ALIGN_BOTTOM_MID, 0, -6);

    // Touch on screen closes card
    lv_obj_add_event_cb(screen_, [](lv_event_t* e) {
        auto self = static_cast<SensorCardScreen*>(lv_event_get_user_data(e));
        if (self) {
            self->Hide();
        }
    }, LV_EVENT_CLICKED, this);
}

void SensorCardScreen::ResetAutoReturnTimer() {
    if (auto_return_timer_ != nullptr) {
        esp_timer_stop(auto_return_timer_);
        esp_timer_start_once(auto_return_timer_, 15 * 1000 * 1000); // 15 giay auto return
    }
}

void SensorCardScreen::UpdateData() {
    CoreS3SensorSnapshot s = SensorMonitor::GetInstance().GetSnapshot();

    if (lvgl_port_lock(100)) {
        switch (current_type_) {
            case SensorCardType::Temperature:
                lv_label_set_text(title_label_, "NHIET DO BO MACH");
                lv_obj_set_style_border_color(card_box_, lv_color_hex(0xFF5252), 0);
                lv_obj_set_style_text_color(title_label_, lv_color_hex(0xFF5252), 0);
                lv_label_set_text_fmt(value_label_, "%.1f C", s.power.temperature_c);
                lv_label_set_text_fmt(detail_label_, "Trang thai: %s",
                                      s.power.temperature_c < 45.0f ? "Mat / Binh thuong" : "Hoi am");
                break;

            case SensorCardType::Battery:
                lv_label_set_text(title_label_, "PIN & NGUON DIEN");
                lv_obj_set_style_border_color(card_box_, lv_color_hex(0xFFB300), 0);
                lv_obj_set_style_text_color(title_label_, lv_color_hex(0xFFB300), 0);
                lv_label_set_text_fmt(value_label_, "%d %%", s.power.battery_level);
                lv_label_set_text_fmt(detail_label_, "Trang thai: %s",
                                      s.power.is_charging ? "Dang cam sac USB" : "Dang dung pin");
                break;

            case SensorCardType::Light:
                lv_label_set_text(title_label_, "ANH SANG MOI TRUONG (LTR-553)");
                lv_obj_set_style_border_color(card_box_, lv_color_hex(0xFFEB3B), 0);
                lv_obj_set_style_text_color(title_label_, lv_color_hex(0xFFEB3B), 0);
                if (s.light.available) {
                    lv_label_set_text_fmt(value_label_, "%.0f Lux", s.light.lux);
                    lv_label_set_text_fmt(detail_label_, "Moi truong: %s | Proximity: %u",
                                          s.light.lux < 50.0f ? "Toi / Thieu sang" : (s.light.lux < 500.0f ? "Trong phong vua" : "Rat sang"),
                                          s.light.proximity);
                } else {
                    lv_label_set_text(value_label_, "-- Lux");
                    lv_label_set_text(detail_label_, "Dang khoi tao cam bien...");
                }
                break;

            case SensorCardType::Motion:
                lv_label_set_text(title_label_, "CON QUAY & GIA TOC (BMI270)");
                lv_obj_set_style_border_color(card_box_, lv_color_hex(0x00E5FF), 0);
                lv_obj_set_style_text_color(title_label_, lv_color_hex(0x00E5FF), 0);
                if (s.motion.available) {
                    lv_label_set_text(value_label_, s.motion.posture.c_str());
                    lv_label_set_text_fmt(detail_label_, "Gyro: %.0f, %.0f, %.0f dps\nAcc: %.2f, %.2f, %.2f g",
                                          s.motion.gyro_x, s.motion.gyro_y, s.motion.gyro_z,
                                          s.motion.accel_x, s.motion.accel_y, s.motion.accel_z);
                } else {
                    lv_label_set_text(value_label_, "Dang doc...");
                    lv_label_set_text(detail_label_, "Dang lay mau du lieu...");
                }
                break;

            case SensorCardType::Network:
                lv_label_set_text(title_label_, "TIN HIEU WI-FI & MANG");
                lv_obj_set_style_border_color(card_box_, lv_color_hex(0x00E676), 0);
                lv_obj_set_style_text_color(title_label_, lv_color_hex(0x00E676), 0);
                lv_label_set_text_fmt(value_label_, "%d dBm", s.network.rssi_dbm);
                lv_label_set_text_fmt(detail_label_, "SSID: %s\nIP: %s",
                                      s.network.ssid.c_str(), s.network.ip_address.c_str());
                break;

            case SensorCardType::System:
                lv_label_set_text(title_label_, "TAI NGUYEN HE THONG");
                lv_obj_set_style_border_color(card_box_, lv_color_hex(0x7C4DFF), 0);
                lv_obj_set_style_text_color(title_label_, lv_color_hex(0x7C4DFF), 0);
                lv_label_set_text_fmt(value_label_, "%lu KB RAM",
                                      static_cast<unsigned long>(s.system.free_sram_bytes / 1024));
                lv_label_set_text_fmt(detail_label_, "PSRAM: %lu MB | Uptime: %lus",
                                      static_cast<unsigned long>(s.system.free_psram_bytes / (1024 * 1024)),
                                      static_cast<unsigned long>(s.system.uptime_seconds));
                break;
        }
        lvgl_port_unlock();
    }
}

void SensorCardScreen::Show(SensorCardType type) {
    if (screen_ == nullptr) return;
    current_type_ = type;

    if (lvgl_port_lock(200)) {
        main_screen_ = lv_screen_active();
        // Synchronous load, see DashboardScreen::Show().
        lv_screen_load(screen_);
        visible_ = true;
        lvgl_port_unlock();
    }

    UpdateData();
    ResetAutoReturnTimer();

    if (update_timer_ != nullptr) {
        esp_timer_start_periodic(update_timer_, 1000 * 1000); // 1 giay / lan
    }
}

void SensorCardScreen::Hide() {
    if (!visible_ || main_screen_ == nullptr) return;

    if (update_timer_ != nullptr) {
        esp_timer_stop(update_timer_);
    }
    if (auto_return_timer_ != nullptr) {
        esp_timer_stop(auto_return_timer_);
    }

    if (lvgl_port_lock(200)) {
        lv_screen_load(main_screen_);
        lvgl_port_unlock();
    }
    // Cleared unconditionally, see DashboardScreen::Hide().
    visible_ = false;
}
