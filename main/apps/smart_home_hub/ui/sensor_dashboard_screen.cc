#include "sensor_dashboard_screen.h"
#include "../sensor_monitor.h"
#include "application.h"
#include <esp_log.h>
#include <esp_lvgl_port.h>
#include <ctime>
#include <cstdio>

#define TAG "SH_SensorDash"

// ---------------------------------------------------------------------------
// Palette sampled from the reference "CoreS3 dashboard" look: near-black
// canvas, dark slate cards, one neon accent per card.
// ---------------------------------------------------------------------------
namespace {

constexpr uint32_t kBg = 0x070C10;
constexpr uint32_t kCardBg = 0x111B23;
constexpr uint32_t kTrackBg = 0x1E2A33;
constexpr uint32_t kText = 0xFFFFFF;
constexpr uint32_t kDim = 0x7E93A3;
constexpr uint32_t kCyan = 0x29D3FF;
constexpr uint32_t kGreen = 0x22E06A;
constexpr uint32_t kBlue = 0x2E9BFF;
constexpr uint32_t kPurple = 0xC24BFF;
constexpr uint32_t kYellow = 0xFFC21A;
constexpr uint32_t kOrange = 0xFF6B35;
constexpr uint32_t kRed = 0xFF4D4D;

constexpr int kMargin = 4;
constexpr int kGap = 4;
constexpr int kCol0 = 4;
constexpr int kCol1 = 108;
constexpr int kCol2 = 212;
constexpr int kCardW = 100;
constexpr int kWideW = 204;

constexpr int kRowHead = 4;
constexpr int kHeadH = 52;
constexpr int kRowA = 60;
constexpr int kRowBH = 88;
constexpr int kRowB = 152;

const char* const kWeekday[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};

lv_obj_t* MakeCard(lv_obj_t* parent, int x, int y, int w, int h, uint32_t accent) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, w, h);
    lv_obj_set_style_bg_color(card, lv_color_hex(kCardBg), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(accent), 0);
    lv_obj_set_style_radius(card, 6, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    return card;
}

lv_obj_t* MakeLabel(lv_obj_t* parent, int x, int y, const lv_font_t* font, uint32_t color,
                    const char* text) {
    lv_obj_t* label = lv_label_create(parent);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_label_set_text(label, text);
    return label;
}

lv_obj_t* MakeRightLabel(lv_obj_t* parent, int x, int y, int right, const lv_font_t* font,
                         uint32_t color, const char* text) {
    lv_obj_t* label = MakeLabel(parent, 0, y, font, color, text);
    lv_obj_set_width(label, right);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(label, LV_ALIGN_TOP_RIGHT, -8, y);
    return label;
}

lv_obj_t* MakeBar(lv_obj_t* parent, int x, int y, int w, int h, uint32_t accent) {
    lv_obj_t* bar = lv_bar_create(parent);
    lv_obj_remove_style_all(bar);
    lv_obj_set_pos(bar, x, y);
    lv_obj_set_size(bar, w, h);
    lv_obj_set_style_bg_color(bar, lv_color_hex(kTrackBg), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(bar, 3, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(accent), LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, 3, LV_PART_INDICATOR);
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);
    return bar;
}

}  // namespace

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
        // Hide() takes the LVGL lock; the shared esp_timer task must not block on it.
        Application::GetInstance().Schedule([self]() {
            if (self->IsVisible()) {
                ESP_LOGI(TAG, "Sensor dashboard auto-return to main screen");
                self->Hide();
            }
        });
    }
}

void SensorDashboardScreen::Initialize(lv_display_t* display) {
    display_ = display;

    esp_timer_create_args_t update_args = {
        .callback = &SensorDashboardScreen::OnUpdateTimer,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "sensor_update",
        .skip_unhandled_events = true,
    };
    esp_timer_create(&update_args, &update_timer_);

    esp_timer_create_args_t return_args = {
        .callback = &SensorDashboardScreen::OnAutoReturnTimeout,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "sensor_return",
        .skip_unhandled_events = true,
    };
    esp_timer_create(&return_args, &auto_return_timer_);

    if (lvgl_port_lock(200)) {
        CreateUI();
        lvgl_port_unlock();
    } else {
        ESP_LOGE(TAG, "LVGL busy: sensor dashboard UI not created");
    }
}

void SensorDashboardScreen::CreateUI() {
    screen_ = lv_obj_create(NULL);
    lv_obj_remove_style_all(screen_);
    lv_obj_set_style_bg_color(screen_, lv_color_hex(kBg), 0);
    lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen_, LV_OBJ_FLAG_SCROLLABLE);

    // ---------------------------------------------------------------- header
    lv_obj_t* head = MakeCard(screen_, kCol0, kRowHead, kWideW, kHeadH, kCyan);
    MakeLabel(head, 10, 5, &lv_font_montserrat_14, kCyan, "M5STACK");
    MakeLabel(head, 10, 18, &lv_font_montserrat_20, kText, "CORE S3");
    lv_obj_t* vline = lv_obj_create(head);
    lv_obj_remove_style_all(vline);
    lv_obj_set_pos(vline, 100, 10);
    lv_obj_set_size(vline, 1, kHeadH - 20);
    lv_obj_set_style_bg_color(vline, lv_color_hex(0x2A3B47), 0);
    lv_obj_set_style_bg_opa(vline, LV_OPA_COVER, 0);
    date_label_ = MakeLabel(head, 110, 7, &lv_font_montserrat_14, kDim, "--/--/--");
    clock_label_ = MakeLabel(head, 110, 22, &lv_font_montserrat_20, kText, "--:--");

    // --------------------------------------------------------------- battery
    lv_obj_t* bat = MakeCard(screen_, kCol2, kRowHead, kCardW, kHeadH, kGreen);
    MakeLabel(bat, 8, 5, &lv_font_montserrat_14, kGreen, "BAT");
    bat_state_ = MakeRightLabel(bat, 0, 5, kCardW - 8, &lv_font_montserrat_14, kDim, "--");
    bat_value_ = MakeLabel(bat, 8, 21, &lv_font_montserrat_20, kText, "--%");
    bat_bar_ = MakeBar(bat, 8, kHeadH - 10, kCardW - 16, 5, kGreen);

    // ------------------------------------------------------------ temperature
    lv_obj_t* temp = MakeCard(screen_, kCol0, kRowA, kCardW, kRowBH, kOrange);
    MakeLabel(temp, 8, 5, &lv_font_montserrat_14, kOrange, "TEMP");
    temp_value_ = MakeLabel(temp, 8, 20, &lv_font_montserrat_28, kText, "--.-");
    MakeRightLabel(temp, 0, 34, kCardW - 8, &lv_font_montserrat_14, kDim, "C");
    temp_bar_ = MakeBar(temp, 8, kRowBH - 14, kCardW - 16, 6, kOrange);

    // ------------------------------------------------------------------ IMU
    lv_obj_t* imu = MakeCard(screen_, kCol1, kRowA, kCardW, kRowB - kRowA, kGreen);
    MakeLabel(imu, 8, 5, &lv_font_montserrat_20, kGreen, "IMU");
    MakeLabel(imu, 46, 9, &lv_font_montserrat_14, kDim, "ACC (g)");
    acc_value_ = MakeLabel(imu, 8, 33, &lv_font_montserrat_14, kText, "X  --.--\nY  --.--\nZ  --.--");
    lv_obj_t* hline1 = lv_obj_create(imu);
    lv_obj_remove_style_all(hline1);
    lv_obj_set_pos(hline1, 8, 88);
    lv_obj_set_size(hline1, kCardW - 16, 1);
    lv_obj_set_style_bg_color(hline1, lv_color_hex(0x2A3B47), 0);
    lv_obj_set_style_bg_opa(hline1, LV_OPA_COVER, 0);
    MakeLabel(imu, 8, 94, &lv_font_montserrat_14, kDim, "GYR (d/s)");
    gyr_value_ = MakeLabel(imu, 8, 112, &lv_font_montserrat_14, kText, "X  --.--\nY  --.--\nZ  --.--");
    lv_obj_t* hline2 = lv_obj_create(imu);
    lv_obj_remove_style_all(hline2);
    lv_obj_set_pos(hline2, 8, 166);
    lv_obj_set_size(hline2, kCardW - 16, 1);
    lv_obj_set_style_bg_color(hline2, lv_color_hex(0x2A3B47), 0);
    lv_obj_set_style_bg_opa(hline2, LV_OPA_COVER, 0);
    posture_tilt_ = MakeLabel(imu, 8, 170, &lv_font_montserrat_14, kPurple, "--");

    // ----------------------------------------------------------------- light
    lv_obj_t* light = MakeCard(screen_, kCol2, kRowA, kCardW, kRowBH, kYellow);
    MakeLabel(light, 8, 5, &lv_font_montserrat_14, kYellow, "LIGHT");
    light_value_ = MakeLabel(light, 8, 20, &lv_font_montserrat_28, kText, "--");
    MakeRightLabel(light, 0, 34, kCardW - 8, &lv_font_montserrat_14, kYellow, "lux");
    light_bar_ = MakeBar(light, 8, kRowBH - 26, kCardW - 16, 6, kYellow);
    prox_label_ = MakeLabel(light, 8, kRowBH - 16, &lv_font_montserrat_14, kDim, "PROX --");

    // ----------------------------------------------------------------- Wi-Fi
    lv_obj_t* wifi = MakeCard(screen_, kCol0, kRowB, kCardW, kRowBH, kBlue);
    MakeLabel(wifi, 8, 5, &lv_font_montserrat_14, kBlue, "Wi-Fi");
    wifi_ssid_ = MakeLabel(wifi, 8, 22, &lv_font_montserrat_14, kText, "--");
    lv_obj_set_width(wifi_ssid_, kCardW - 16);
    lv_obj_set_style_text_align(wifi_ssid_, LV_TEXT_ALIGN_LEFT, 0);
    lv_label_set_long_mode(wifi_ssid_, LV_LABEL_LONG_MODE_DOT);
    wifi_rssi_ = MakeLabel(wifi, 8, 42, &lv_font_montserrat_14, kDim, "-- dBm");
    for (int i = 0; i < 4; i++) {
        lv_obj_t* bar = lv_obj_create(wifi);
        lv_obj_remove_style_all(bar);
        lv_obj_set_size(bar, 5, 8 + i * 5);
        lv_obj_set_pos(bar, 52 + i * 8, kRowBH - 10 - (8 + i * 5));
        lv_obj_set_style_bg_color(bar, lv_color_hex(kDim), 0);
        lv_obj_set_style_bg_opa(bar, LV_OPA_40, 0);
        lv_obj_set_style_radius(bar, 1, 0);
        wifi_bars_[i] = bar;
    }

    // ---------------------------------------------------------------- system
    lv_obj_t* sys = MakeCard(screen_, kCol2, kRowB, kCardW, kRowBH, kCyan);
    MakeLabel(sys, 8, 5, &lv_font_montserrat_14, kCyan, "SYSTEM");
    MakeLabel(sys, 8, 24, &lv_font_montserrat_14, kDim, "SRAM");
    lv_obj_t* sram_bar = MakeBar(sys, 8, 40, kCardW - 16, 5, kBlue);
    lv_obj_t* psram_bar = MakeBar(sys, 8, 62, kCardW - 16, 5, kPurple);
    MakeLabel(sys, 8, 46, &lv_font_montserrat_14, kDim, "PSRAM");
    sys_sram_ = MakeRightLabel(sys, 0, 24, kCardW - 8, &lv_font_montserrat_14, kText, "--");
    sys_psram_ = MakeRightLabel(sys, 0, 46, kCardW - 8, &lv_font_montserrat_14, kText, "--");
    sys_cpu_ = MakeLabel(sys, 8, kRowBH - 16, &lv_font_montserrat_14, kDim, "CPU -- MHz");
    uptime_value_ = MakeRightLabel(sys, 0, kRowBH - 16, kCardW - 8, &lv_font_montserrat_14, kGreen, "--");

    // Touch anywhere to go back to the XiaoZhi main screen.
    lv_obj_add_flag(screen_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(
        screen_,
        [](lv_event_t* e) {
            auto self = static_cast<SensorDashboardScreen*>(lv_event_get_user_data(e));
            if (self) {
                self->Hide();
            }
        },
        LV_EVENT_CLICKED, this);
}

void SensorDashboardScreen::UpdateTelemetry() {
    CoreS3SensorSnapshot s = SensorMonitor::GetInstance().GetSnapshot();

    // Clamp first: the bar widgets never see out-of-range values.
    int temp_pct = static_cast<int>((s.power.temperature_c - 20.0f) * 5.0f);
    temp_pct = temp_pct < 0 ? 0 : (temp_pct > 100 ? 100 : temp_pct);
    int lux_pct = static_cast<int>(s.light.lux / 5.0f);
    lux_pct = lux_pct < 0 ? 0 : (lux_pct > 100 ? 100 : lux_pct);
    int sram_pct = static_cast<int>(s.system.free_sram_bytes * 100 / 320000);
    sram_pct = sram_pct < 0 ? 0 : (sram_pct > 100 ? 100 : sram_pct);
    int psram_pct = static_cast<int>(s.system.free_psram_bytes * 100 / (8 * 1024 * 1024));
    psram_pct = psram_pct < 0 ? 0 : (psram_pct > 100 ? 100 : psram_pct);

    const uint32_t seconds = s.system.uptime_seconds;
    char text[64];

    if (lvgl_port_lock(200)) {
        // --- header clock ------------------------------------------------
        const time_t now = time(nullptr);
        if (now > 1700000000) {  // RTC/SNTP has actually been set
            struct tm tmv;
            localtime_r(&now, &tmv);
            lv_label_set_text_fmt(date_label_, "%04d-%02d-%02d", tmv.tm_year + 1900, tmv.tm_mon + 1,
                                 tmv.tm_mday);
            lv_label_set_text(clock_label_, kWeekday[tmv.tm_wday % 7]);
            lv_label_set_text_fmt(clock_label_, "%02d:%02d", tmv.tm_hour, tmv.tm_min);
        } else {
            lv_label_set_text(date_label_, "NO SYNC");
            lv_label_set_text_fmt(clock_label_, "+%02lu:%02lu", seconds / 3600, (seconds / 60) % 60);
        }

        // --- battery ----------------------------------------------------
        lv_label_set_text_fmt(bat_value_, "%d%%", s.power.battery_level);
        lv_label_set_text(bat_state_,
                          s.power.is_charging ? "SAC" : (s.power.is_discharging ? "DIS" : "IDLE"));
        lv_bar_set_value(bat_bar_, s.power.battery_level < 0 ? 0 : s.power.battery_level, LV_ANIM_OFF);

        // --- temperature ------------------------------------------------
        lv_label_set_text_fmt(temp_value_, "%.1f", s.power.temperature_c);
        lv_bar_set_value(temp_bar_, temp_pct, LV_ANIM_OFF);

        // --- light ------------------------------------------------------
        if (s.light.available) {
            lv_label_set_text_fmt(light_value_, "%u", static_cast<unsigned int>(s.light.lux));
            lv_label_set_text_fmt(prox_label_, "PROX %u", static_cast<unsigned int>(s.light.proximity));
        } else {
            lv_label_set_text(light_value_, "--");
            lv_label_set_text(prox_label_, "PROX --");
        }
        lv_bar_set_value(light_bar_, lux_pct, LV_ANIM_OFF);

        // --- IMU --------------------------------------------------------
        if (s.motion.available) {
            lv_label_set_text_fmt(acc_value_, "X %+.2f\nY %+.2f\nZ %+.2f", s.motion.accel_x,
                                 s.motion.accel_y, s.motion.accel_z);
            lv_label_set_text_fmt(gyr_value_, "X %+.2f\nY %+.2f\nZ %+.2f", s.motion.gyro_x,
                                 s.motion.gyro_y, s.motion.gyro_z);
            lv_label_set_text_fmt(posture_tilt_, "TILT %.0f DEG", s.motion.tilt_degrees);
        } else {
            lv_label_set_text(acc_value_, "X  --.--\nY  --.--\nZ  --.--");
            lv_label_set_text(gyr_value_, "X  --.--\nY  --.--\nZ  --.--");
            lv_label_set_text(posture_tilt_, "NO IMU");
        }

        // --- Wi-Fi ------------------------------------------------------
        lv_label_set_text(wifi_ssid_, s.network.ssid.c_str());
        lv_label_set_text_fmt(wifi_rssi_, "%d dBm", s.network.rssi_dbm);
        int active = s.network.rssi_dbm >= -55   ? 4
                     : s.network.rssi_dbm >= -67 ? 3
                     : s.network.rssi_dbm >= -78 ? 2
                                                 : 1;
        for (int i = 0; i < 4; i++) {
            lv_obj_set_style_bg_color(wifi_bars_[i], lv_color_hex(i < active ? kGreen : kDim), 0);
            lv_obj_set_style_bg_opa(wifi_bars_[i], i < active ? LV_OPA_COVER : LV_OPA_40, 0);
        }

        // --- system -----------------------------------------------------
        lv_label_set_text_fmt(sys_sram_, "%luK", static_cast<unsigned long>(s.system.free_sram_bytes / 1024));
        lv_label_set_text_fmt(sys_psram_, "%luM",
                              static_cast<unsigned long>(s.system.free_psram_bytes / (1024 * 1024)));
        lv_label_set_text_fmt(sys_cpu_, "CPU %luMHz", static_cast<unsigned long>(s.system.cpu_freq_mhz));
        snprintf(text, sizeof(text), "%02lu:%02lu", seconds / 3600, (seconds / 60) % 60);
        lv_label_set_text(uptime_value_, text);
        lv_bar_set_value(static_cast<lv_obj_t*>(lv_obj_get_parent(sys_sram_)), sram_pct, LV_ANIM_OFF);
        lv_bar_set_value(static_cast<lv_obj_t*>(lv_obj_get_parent(sys_psram_)), psram_pct, LV_ANIM_OFF);

        lvgl_port_unlock();
    }
}

void SensorDashboardScreen::Show() {
    if (screen_ == nullptr) return;
    ESP_LOGI(TAG, "Showing Sensor Dashboard screen");
    if (lvgl_port_lock(200)) {
        main_screen_ = lv_screen_active();
        // Synchronous load, see DashboardScreen::Show().
        lv_screen_load(screen_);
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

    if (lvgl_port_lock(200)) {
        lv_screen_load(main_screen_);
        lvgl_port_unlock();
    }
    // Cleared unconditionally, see DashboardScreen::Hide().
    visible_ = false;
}
