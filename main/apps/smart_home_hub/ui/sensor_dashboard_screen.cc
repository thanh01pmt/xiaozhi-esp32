#include "sensor_dashboard_screen.h"
#include "../sensor_monitor.h"
#include <esp_log.h>
#include <esp_lvgl_port.h>

#define TAG "SH_SensorDash"

SensorDashboardScreen::SensorDashboardScreen() = default;

SensorDashboardScreen::~SensorDashboardScreen() {
    if (update_timer_ != nullptr) {
        esp_timer_stop(update_timer_);
        esp_timer_delete(update_timer_);
    }
    if (auto_return_timer_ != nullptr) {
        esp_timer_stop(auto_return_timer_);
        esp_timer_delete(auto_return_timer_);
    }
}

void SensorDashboardScreen::OnUpdateTimer(void* arg) {
    auto self = static_cast<SensorDashboardScreen*>(arg);
    if (self && self->IsVisible()) {
        self->UpdateTelemetry();
    }
}

void SensorDashboardScreen::OnAutoReturnTimeout(void* arg) {
    auto self = static_cast<SensorDashboardScreen*>(arg);
    if (self && self->IsVisible()) {
        ESP_LOGI(TAG, "Sensor dashboard auto-return to main screen");
        self->Hide();
    }
}

void SensorDashboardScreen::Initialize(lv_display_t* display) {
    display_ = display;

    // Timer cap nhat so lieu moi 1 giay khi dang hien thi
    esp_timer_create_args_t update_args = {
        .callback = &SensorDashboardScreen::OnUpdateTimer,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "sensor_update",
        .skip_unhandled_events = true,
    };
    esp_timer_create(&update_args, &update_timer_);

    // Timer tu dong quay ve man hinh XiaoZhi sau 20 giay khong thao tac
    esp_timer_create_args_t return_args = {
        .callback = &SensorDashboardScreen::OnAutoReturnTimeout,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "sensor_return",
        .skip_unhandled_events = true,
    };
    esp_timer_create(&return_args, &auto_return_timer_);

    if (lvgl_port_lock(100)) {
        CreateUI();
        lvgl_port_unlock();
    }
}

void SensorDashboardScreen::CreateUI() {
    screen_ = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_, lv_color_hex(0x0F0F14), 0);
    lv_obj_clear_flag(screen_, LV_OBJ_FLAG_SCROLLABLE);

    // Header Title
    lv_obj_t* header = lv_label_create(screen_);
    lv_label_set_text(header, "M5Stack CoreS3 Telemetry");
    lv_obj_set_style_text_color(header, lv_color_hex(0x00E5FF), 0);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 8);

    // 4 Sensor Cards Container (2x2 Grid)
    lv_obj_t* grid = lv_obj_create(screen_);
    lv_obj_set_size(grid, 304, 185);
    lv_obj_align(grid, LV_ALIGN_BOTTOM_MID, 0, -6);
    lv_obj_set_style_bg_color(grid, lv_color_hex(0x181820), 0);
    lv_obj_set_style_border_width(grid, 1, 0);
    lv_obj_set_style_border_color(grid, lv_color_hex(0x282836), 0);
    lv_obj_set_style_radius(grid, 8, 0);
    lv_obj_set_style_pad_all(grid, 6, 0);
    lv_obj_set_style_pad_row(grid, 4, 0);
    lv_obj_set_style_pad_column(grid, 4, 0);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);

    // Card 1: Power & Battery
    lv_obj_t* card1 = lv_obj_create(grid);
    lv_obj_set_size(card1, 140, 80);
    lv_obj_set_style_bg_color(card1, lv_color_hex(0x22222E), 0);
    lv_obj_set_style_border_width(card1, 0, 0);
    lv_obj_set_style_radius(card1, 6, 0);
    lv_obj_set_style_pad_all(card1, 6, 0);
    lv_obj_clear_flag(card1, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* t1 = lv_label_create(card1);
    lv_label_set_text(t1, "PIN & NGUON");
    lv_obj_set_style_text_color(t1, lv_color_hex(0xFFB300), 0);
    lv_obj_align(t1, LV_ALIGN_TOP_LEFT, 0, 0);
    pwr_label_ = lv_label_create(card1);
    lv_label_set_text(pwr_label_, "Pin: --%\nSac: --");
    lv_obj_set_style_text_color(pwr_label_, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(pwr_label_, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    // Card 2: Temperature & Thermal
    lv_obj_t* card2 = lv_obj_create(grid);
    lv_obj_set_size(card2, 140, 80);
    lv_obj_set_style_bg_color(card2, lv_color_hex(0x22222E), 0);
    lv_obj_set_style_border_width(card2, 0, 0);
    lv_obj_set_style_radius(card2, 6, 0);
    lv_obj_set_style_pad_all(card2, 6, 0);
    lv_obj_clear_flag(card2, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* t2 = lv_label_create(card2);
    lv_label_set_text(t2, "NHIET DO & AS");
    lv_obj_set_style_text_color(t2, lv_color_hex(0xFF5252), 0);
    lv_obj_align(t2, LV_ALIGN_TOP_LEFT, 0, 0);
    temp_label_ = lv_label_create(card2);
    lv_label_set_text(temp_label_, "Nhiet: -- C\nAS: -- Lux");
    lv_obj_set_style_text_color(temp_label_, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(temp_label_, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    // Card 3: Network Wi-Fi
    lv_obj_t* card3 = lv_obj_create(grid);
    lv_obj_set_size(card3, 140, 80);
    lv_obj_set_style_bg_color(card3, lv_color_hex(0x22222E), 0);
    lv_obj_set_style_border_width(card3, 0, 0);
    lv_obj_set_style_radius(card3, 6, 0);
    lv_obj_set_style_pad_all(card3, 6, 0);
    lv_obj_clear_flag(card3, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* t3 = lv_label_create(card3);
    lv_label_set_text(t3, "WI-FI & MANG");
    lv_obj_set_style_text_color(t3, lv_color_hex(0x00E676), 0);
    lv_obj_align(t3, LV_ALIGN_TOP_LEFT, 0, 0);
    net_label_ = lv_label_create(card3);
    lv_label_set_text(net_label_, "SSID: --\nRSSI: -- dBm");
    lv_obj_set_style_text_color(net_label_, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(net_label_, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    // Card 4: System & Memory
    lv_obj_t* card4 = lv_obj_create(grid);
    lv_obj_set_size(card4, 140, 80);
    lv_obj_set_style_bg_color(card4, lv_color_hex(0x22222E), 0);
    lv_obj_set_style_border_width(card4, 0, 0);
    lv_obj_set_style_radius(card4, 6, 0);
    lv_obj_set_style_pad_all(card4, 6, 0);
    lv_obj_clear_flag(card4, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* t4 = lv_label_create(card4);
    lv_label_set_text(t4, "HE THONG");
    lv_obj_set_style_text_color(t4, lv_color_hex(0x7C4DFF), 0);
    lv_obj_align(t4, LV_ALIGN_TOP_LEFT, 0, 0);
    sys_label_ = lv_label_create(card4);
    lv_label_set_text(sys_label_, "RAM: -- KB\nUptime: --s");
    lv_obj_set_style_text_color(sys_label_, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(sys_label_, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    // Touch tren man hinh de dong va quay ve man hinh chinh
    lv_obj_add_event_cb(screen_, [](lv_event_t* e) {
        auto self = static_cast<SensorDashboardScreen*>(lv_event_get_user_data(e));
        if (self) {
            self->Hide();
        }
    }, LV_EVENT_CLICKED, this);
}

void SensorDashboardScreen::UpdateTelemetry() {
    CoreS3SensorSnapshot s = SensorMonitor::GetInstance().GetSnapshot();

    if (lvgl_port_lock(100)) {
        if (pwr_label_ != nullptr) {
            lv_label_set_text_fmt(pwr_label_, "Pin: %d%%\nSac: %s",
                                 s.power.battery_level,
                                 s.power.is_charging ? "Dang sac" : "Pin");
        }
        if (temp_label_ != nullptr) {
            if (s.light.available) {
                lv_label_set_text_fmt(temp_label_, "%.1f C | %u Lux\nAS: %s",
                                     s.power.temperature_c,
                                     static_cast<unsigned int>(s.light.lux),
                                     s.light.lux < 50.0f ? "Toi" : (s.light.lux < 500.0f ? "Vua" : "Sang"));
            } else {
                lv_label_set_text_fmt(temp_label_, "Nhiet: %.1f C\nTrang thai: %s",
                                     s.power.temperature_c,
                                     s.power.temperature_c < 45.0f ? "Binh thuong" : "Nong");
            }
        }
        if (net_label_ != nullptr) {
            lv_label_set_text_fmt(net_label_, "%s\n%d dBm (Ch%d)",
                                 s.network.ssid.c_str(),
                                 s.network.rssi_dbm,
                                 s.network.channel);
        }
        if (sys_label_ != nullptr) {
            if (s.motion.available) {
                lv_label_set_text_fmt(sys_label_, "RAM: %luKB\nIMU: %s",
                                     static_cast<unsigned long>(s.system.free_sram_bytes / 1024),
                                     s.motion.posture.c_str());
            } else {
                lv_label_set_text_fmt(sys_label_, "RAM: %luKB\nUptime: %lus",
                                     static_cast<unsigned long>(s.system.free_sram_bytes / 1024),
                                     static_cast<unsigned long>(s.system.uptime_seconds));
            }
        }
        lvgl_port_unlock();
    }
}

void SensorDashboardScreen::Show() {
    if (screen_ == nullptr) return;
    ESP_LOGI(TAG, "Showing Sensor Dashboard screen");
    if (lvgl_port_lock(100)) {
        main_screen_ = lv_screen_active();
        lv_screen_load_anim(screen_, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
        visible_ = true;
        lvgl_port_unlock();
    }
    UpdateTelemetry();

    if (update_timer_ != nullptr) {
        esp_timer_start_periodic(update_timer_, 1000 * 1000); // 1 giay / lan
    }
    if (auto_return_timer_ != nullptr) {
        esp_timer_stop(auto_return_timer_);
        esp_timer_start_once(auto_return_timer_, 20 * 1000 * 1000); // 20 giay auto-return
    }
}

void SensorDashboardScreen::Hide() {
    if (!visible_ || main_screen_ == nullptr) return;
    ESP_LOGI(TAG, "Hiding Sensor Dashboard screen");
    if (update_timer_ != nullptr) {
        esp_timer_stop(update_timer_);
    }
    if (auto_return_timer_ != nullptr) {
        esp_timer_stop(auto_return_timer_);
    }

    if (lvgl_port_lock(100)) {
        lv_screen_load_anim(main_screen_, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0, false);
        visible_ = false;
        lvgl_port_unlock();
    }
}
