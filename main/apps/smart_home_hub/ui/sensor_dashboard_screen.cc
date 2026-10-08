#include "sensor_dashboard_screen.h"
#include "shh_theme.h"
#include "../sensor_monitor.h"
#include "application.h"
#include <esp_log.h>
#include <esp_lvgl_port.h>
#include <ctime>
#include <cstdio>
#include <cmath>

#define TAG "SH_SensorDash"

// ---------------------------------------------------------------------------
// Palette lives in shh_theme.h so every mini-app shares one set of colours.
// ---------------------------------------------------------------------------
namespace {

using namespace shh_ui;

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
constexpr int kTallH = 236 - kRowA;  // IMU card spans both body rows

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
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
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

lv_obj_t* MakeRightLabel(lv_obj_t* parent, int y, int right, const lv_font_t* font,
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
    auto self = static_cast<SensorDashboardScreen*>(arg);        if (self && self->IsVisible()) {
            self->UpdateTelemetry();
        }
}

#include "../smart_home_hub.h"

void SensorDashboardScreen::OnAutoReturnTimeout(void* arg) {
    auto self = static_cast<SensorDashboardScreen*>(arg);
    if (self && self->IsVisible()) {
        if (int64_t rem = SmartHomeHub::InactivityRemainingUs(); rem > 0) {
            esp_timer_start_once(self->auto_return_timer_, rem);
            return;
        }
        Application::GetInstance().Schedule([self]() {
            if (self->IsVisible()) {
                ESP_LOGI(TAG, "Sensor dashboard auto-return (120s) to default screen (eyes)");
                SmartHomeHub::GetInstance().ReturnToDefaultScreen();
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

void SensorDashboardScreen::MakeCardTappable(lv_obj_t* card, const char* sensor_type) {
    // lv_obj_add_event_cb() takes no per-handler argument, so the sensor id
    // travels on the card itself while user_data carries the owner.
    lv_obj_set_user_data(card, const_cast<char*>(sensor_type));
    lv_obj_add_event_cb(
        card,
        [](lv_event_t* e) {
            auto self = static_cast<SensorDashboardScreen*>(lv_event_get_user_data(e));
            const char* sensor_type = static_cast<const char*>(
                lv_obj_get_user_data(static_cast<lv_obj_t*>(lv_event_get_target(e))));
            if (self && self->open_card_ && sensor_type != nullptr) {
                // Runs in the LVGL task, which already holds the lock;
                // lvgl_mux is a recursive mutex so the nested lock is fine.
                self->open_card_(sensor_type);
            }
        },
        LV_EVENT_CLICKED, this);

    // Press feedback. Without it a tap only does something on release, which
    // reads as lag on a screen this size. The accent colour is left alone;
    // only the fill and the border weight change.
    lv_obj_add_event_cb(
        card,
        [](lv_event_t* e) {
            lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
            lv_obj_set_style_bg_opa(target, LV_OPA_80, 0);
            lv_obj_set_style_border_width(target, 2, 0);
        },
        LV_EVENT_PRESSED, this);
    lv_obj_add_event_cb(
        card,
        [](lv_event_t* e) {
            lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
            lv_obj_set_style_bg_opa(target, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(target, 1, 0);
        },
        LV_EVENT_RELEASED, this);
}

void SensorDashboardScreen::CreateUI() {
    screen_ = lv_obj_create(NULL);
    SetScreen(screen_);
    lv_obj_remove_style_all(screen_);
    lv_obj_set_style_bg_color(screen_, lv_color_hex(kBg), 0);
    lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen_, LV_OBJ_FLAG_SCROLLABLE);

    // ---------------------------------------------------------------- header
    lv_obj_t* head = MakeCard(screen_, kCol0, kRowHead, kWideW, kHeadH, kCyan);
    // The header is the clock, not a sensor: leave it out so a tap there falls
    // through to the background handler that goes back.
    lv_obj_clear_flag(head, LV_OBJ_FLAG_CLICKABLE);
    MakeLabel(head, 10, 5, &lv_font_montserrat_14, kCyan, "M5STACK");
    MakeLabel(head, 10, 18, SHH_FONT_MID, kText, "CORE S3");
    lv_obj_t* vline = lv_obj_create(head);
    lv_obj_remove_style_all(vline);
    lv_obj_set_pos(vline, 100, 10);
    lv_obj_set_size(vline, 1, kHeadH - 20);
    lv_obj_set_style_bg_color(vline, lv_color_hex(kLine), 0);
    lv_obj_set_style_bg_opa(vline, LV_OPA_COVER, 0);
    date_label_ = MakeLabel(head, 106, 7, &lv_font_montserrat_14, kDim, "--/--/--");
    clock_label_ = MakeLabel(head, 106, 22, SHH_FONT_MID, kText, "--:--");

    // --------------------------------------------------------------- battery
    lv_obj_t* bat = MakeCard(screen_, kCol2, kRowHead, kCardW, kHeadH, kGreen);
    MakeCardTappable(bat, "battery");
    MakeLabel(bat, 8, 5, &lv_font_montserrat_14, kGreen, "BAT");
    bat_state_ = MakeRightLabel(bat, 5, kCardW - 8, &lv_font_montserrat_14, kDim, "--");
    bat_value_ = MakeLabel(bat, 8, 21, SHH_FONT_MID, kText, "--%");
    bat_bar_ = MakeBar(bat, 8, kHeadH - 10, kCardW - 16, 5, kGreen);

    // ------------------------------------------------------------ temperature
    lv_obj_t* temp = MakeCard(screen_, kCol0, kRowA, kCardW, kRowBH, kOrange);
    MakeCardTappable(temp, "temperature");
    MakeLabel(temp, 8, 5, &lv_font_montserrat_14, kOrange, "TEMP");
    temp_value_ = MakeLabel(temp, 8, 20, SHH_FONT_BIG, kText, "--.-");
    MakeRightLabel(temp, 34, kCardW - 8, &lv_font_montserrat_14, kDim, "C");
    temp_bar_ = MakeBar(temp, 8, kRowBH - 26, kCardW - 16, 6, kOrange);

    // ------------------------------------------------------------------ IMU
    lv_obj_t* imu = MakeCard(screen_, kCol1, kRowA, kCardW, kTallH, kGreen);
    MakeCardTappable(imu, "motion");
    MakeLabel(imu, 8, 5, SHH_FONT_MID, kGreen, "IMU");
    MakeLabel(imu, 46, 9, &lv_font_montserrat_14, kDim, "ACC (g)");
    acc_value_ = MakeLabel(imu, 8, 32, &lv_font_montserrat_14, kText, "X  --.--\nY  --.--\nZ  --.--");
    lv_obj_t* hline1 = lv_obj_create(imu);
    lv_obj_remove_style_all(hline1);
    lv_obj_set_pos(hline1, 8, 86);
    lv_obj_set_size(hline1, kCardW - 16, 1);
    lv_obj_set_style_bg_color(hline1, lv_color_hex(kLine), 0);
    lv_obj_set_style_bg_opa(hline1, LV_OPA_COVER, 0);
    MakeLabel(imu, 8, 90, &lv_font_montserrat_14, kDim, "GYR (d/s)");
    gyr_value_ = MakeLabel(imu, 8, 108, &lv_font_montserrat_14, kText, "X  --.--\nY  --.--\nZ  --.--");
    posture_tilt_ = MakeLabel(imu, 8, 162, &lv_font_montserrat_14, kPurple, "--");

    // ----------------------------------------------------------------- light
    lv_obj_t* light = MakeCard(screen_, kCol2, kRowA, kCardW, kRowBH, kYellow);
    MakeCardTappable(light, "light");
    MakeLabel(light, 8, 5, &lv_font_montserrat_14, kYellow, "LIGHT");
    light_value_ = MakeLabel(light, 8, 20, SHH_FONT_BIG, kText, "--");
    MakeRightLabel(light, 34, kCardW - 8, &lv_font_montserrat_14, kYellow, "lux");
    light_bar_ = MakeBar(light, 8, kRowBH - 30, kCardW - 16, 6, kYellow);
    prox_label_ = MakeLabel(light, 8, kRowBH - 20, &lv_font_montserrat_14, kDim, "PROX --");

    // ----------------------------------------------------------------- Wi-Fi
    lv_obj_t* wifi = MakeCard(screen_, kCol0, kRowB, kCardW, kRowBH, kBlue);
    MakeCardTappable(wifi, "network");
    MakeLabel(wifi, 8, 5, &lv_font_montserrat_14, kBlue, "Wi-Fi");
    wifi_ssid_ = MakeLabel(wifi, 8, 22, &lv_font_montserrat_14, kText, "--");
    lv_obj_set_width(wifi_ssid_, kCardW - 16);
    lv_obj_set_style_text_align(wifi_ssid_, LV_TEXT_ALIGN_LEFT, 0);
    lv_label_set_long_mode(wifi_ssid_, LV_LABEL_LONG_MODE_CLIP);
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
    MakeCardTappable(sys, "system");
    MakeLabel(sys, 8, 5, &lv_font_montserrat_14, kCyan, "SYSTEM");
    MakeLabel(sys, 8, 24, &lv_font_montserrat_14, kDim, "SRAM");
    sys_sram_bar_ = MakeBar(sys, 8, 40, kCardW - 16, 5, kBlue);
    sys_psram_bar_ = MakeBar(sys, 8, 62, kCardW - 16, 5, kPurple);
    MakeLabel(sys, 8, 46, &lv_font_montserrat_14, kDim, "PSRAM");
    sys_sram_ = MakeRightLabel(sys, 24, kCardW - 8, &lv_font_montserrat_14, kText, "--");
    sys_psram_ = MakeRightLabel(sys, 46, kCardW - 8, &lv_font_montserrat_14, kText, "--");
    sys_cpu_ = MakeLabel(sys, 8, kRowBH - 18, &lv_font_montserrat_14, kDim, "CPU -- MHz");
    uptime_value_ = MakeRightLabel(sys, kRowBH - 18, kCardW - 8, &lv_font_montserrat_14, kGreen, "--");

    // No background tap handler: a stray touch must never leave this screen.
    // Exit = hold the bottom-left corner (SmartHomeHub) or the 120 s timeout.
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

    if (lvgl_port_lock(200)) {
        // --- header clock ------------------------------------------------
        const time_t now = time(nullptr);
        if (now > 1700000000) {  // RTC/SNTP has actually been set
            struct tm tmv;
            localtime_r(&now, &tmv);
            int curmin = tmv.tm_hour * 60 + tmv.tm_min;
            if (curmin != last_date_min_) {
                lv_label_set_text_fmt(date_label_, "%04d-%02d-%02d", tmv.tm_year + 1900, tmv.tm_mon + 1,
                                     tmv.tm_mday);
                lv_label_set_text_fmt(clock_label_, "%02d:%02d", tmv.tm_hour, tmv.tm_min);
                last_date_min_ = curmin;
            }
        } else {
            lv_label_set_text(date_label_, "NO SYNC");
            lv_label_set_text_fmt(clock_label_, "+%02lu:%02lu", static_cast<unsigned long>(seconds / 3600),
                                 static_cast<unsigned long>((seconds / 60) % 60));
        }

        // --- battery ----------------------------------------------------
        {
            int b = s.power.battery_level;
            if (b < 0) b = 0;
            if (b > 100) b = 100;
            if (b != last_bat_pct_) {
                lv_label_set_text_fmt(bat_value_, "%d%%", s.power.battery_level);
                lv_label_set_text(bat_state_,
                                  s.power.is_charging ? "SAC" : (s.power.is_discharging ? "DIS" : "IDLE"));
                lv_bar_set_value(bat_bar_, s.power.battery_level < 0 ? 0 : s.power.battery_level, LV_ANIM_OFF);
                last_bat_pct_ = b;
            }
        }

        // --- temperature ------------------------------------------------
        if (std::abs(s.power.temperature_c - last_temp_c_) > 0.15f || temp_pct != last_temp_pct_) {
            lv_label_set_text_fmt(temp_value_, "%.1f", s.power.temperature_c);
            lv_bar_set_value(temp_bar_, temp_pct, LV_ANIM_OFF);
            last_temp_c_ = s.power.temperature_c;
            last_temp_pct_ = temp_pct;
        }

        // --- light ------------------------------------------------------
        bool light_changed = (s.light.available != last_light_ok_);
        if (s.light.available) {
            if (light_changed || s.light.lux != last_lux_ || s.light.proximity != last_prox_ || lux_pct != last_lux_pct_) {
                lv_label_set_text_fmt(light_value_, "%u", static_cast<unsigned int>(s.light.lux));
                lv_label_set_text_fmt(prox_label_, "PROX %u", static_cast<unsigned int>(s.light.proximity));
                lv_obj_set_style_text_color(light_value_, lv_color_hex(kText), 0);
                lv_obj_set_style_text_color(prox_label_, lv_color_hex(kDim), 0);
                last_lux_ = static_cast<uint32_t>(s.light.lux);
                last_prox_ = s.light.proximity;
                last_lux_pct_ = lux_pct;
                last_light_ok_ = true;
            }
        } else {
            if (light_changed) {
                lv_label_set_text(light_value_, "ERR");
                lv_label_set_text(prox_label_, "LTR-553 NOT FOUND");
                lv_obj_set_style_text_color(light_value_, lv_color_hex(kRed), 0);
                lv_obj_set_style_text_color(prox_label_, lv_color_hex(kRed), 0);
                last_light_ok_ = false;
            }
        }
        lv_bar_set_value(light_bar_, lux_pct, LV_ANIM_OFF);

        // --- IMU --------------------------------------------------------
        if (s.motion.available) {
            lv_label_set_text_fmt(acc_value_, "X %+.2f\nY %+.2f\nZ %+.2f", s.motion.accel_x,
                                 s.motion.accel_y, s.motion.accel_z);
            lv_label_set_text_fmt(gyr_value_, "X %+.2f\nY %+.2f\nZ %+.2f", s.motion.gyro_x,
                                 s.motion.gyro_y, s.motion.gyro_z);
            lv_label_set_text_fmt(posture_tilt_, "TILT %.0f DEG", s.motion.tilt_degrees);
            lv_obj_set_style_text_color(posture_tilt_, lv_color_hex(kPurple), 0);
        } else {
            lv_label_set_text(acc_value_, "X  --.--\nY  --.--\nZ  --.--");
            lv_label_set_text(gyr_value_, "X  --.--\nY  --.--\nZ  --.--");
            lv_label_set_text(posture_tilt_, "BMI270 NOT FOUND");
            lv_obj_set_style_text_color(posture_tilt_, lv_color_hex(kRed), 0);
        }

        // --- Wi-Fi ------------------------------------------------------
        int active = s.network.rssi_dbm >= -55   ? 4
                     : s.network.rssi_dbm >= -67 ? 3
                     : s.network.rssi_dbm >= -78 ? 2
                                                 : 1;
        if (s.network.ssid != last_ssid_ || abs(s.network.rssi_dbm - last_rssi_) > 1) {
            lv_label_set_text(wifi_ssid_, s.network.ssid.c_str());
            lv_label_set_text_fmt(wifi_rssi_, "%d dBm", s.network.rssi_dbm);
            for (int i = 0; i < 4; i++) {
                lv_obj_set_style_bg_color(wifi_bars_[i], lv_color_hex(i < active ? kGreen : kDim), 0);
                lv_obj_set_style_bg_opa(wifi_bars_[i], i < active ? LV_OPA_COVER : LV_OPA_40, 0);
            }
            last_ssid_ = s.network.ssid;
            last_rssi_ = s.network.rssi_dbm;
        } else {
            // bars still depend on active; if rssi changed by 1, we already updated; but just in case
            // skip heavy updates
        }

        // --- system -----------------------------------------------------
        {
            uint32_t sram_kb = static_cast<uint32_t>(s.system.free_sram_bytes / 1024);
            uint32_t psram_mb = static_cast<uint32_t>(s.system.free_psram_bytes / (1024 * 1024));
            uint32_t cpu_mhz = static_cast<uint32_t>(s.system.cpu_freq_mhz);
            if (sram_kb != last_sram_kb_ || psram_mb != last_psram_mb_ || cpu_mhz != last_cpu_mhz_ ||
                sram_pct != last_sram_pct_ || psram_pct != last_psram_pct_) {
                lv_label_set_text_fmt(sys_sram_, "%luK", static_cast<unsigned long>(sram_kb));
                lv_label_set_text_fmt(sys_psram_, "%luM", static_cast<unsigned long>(psram_mb));
                lv_label_set_text_fmt(sys_cpu_, "CPU %luMHz", static_cast<unsigned long>(cpu_mhz));
                lv_bar_set_value(sys_sram_bar_, sram_pct, LV_ANIM_OFF);
                lv_bar_set_value(sys_psram_bar_, psram_pct, LV_ANIM_OFF);
                last_sram_kb_ = sram_kb;
                last_psram_mb_ = psram_mb;
                last_cpu_mhz_ = cpu_mhz;
                last_sram_pct_ = sram_pct;
                last_psram_pct_ = psram_pct;
            }
        }
        {
            uint32_t upmin = (seconds / 60) % 1440;
            if (upmin != last_uptime_min_) {
                char text[16];
                snprintf(text, sizeof(text), "%02lu:%02lu", static_cast<unsigned long>(seconds / 3600),
                         static_cast<unsigned long>((seconds / 60) % 60));
                lv_label_set_text(uptime_value_, text);
                last_uptime_min_ = upmin;
            }
        }

        lvgl_port_unlock();
    }
}

void SensorDashboardScreen::Show() {
    if (screen_ == nullptr) return;
    ESP_LOGI(TAG, "Showing Sensor Dashboard screen");
    if (!AcquireForeground()) {
        return;
    }
    UpdateTelemetry();

    // Force initial update
    last_bat_pct_ = -1;
    last_temp_c_ = -1000.0f;
    last_lux_ = 0xffffffff;
    last_prox_ = 0xffff;
    last_ssid_.clear();
    last_rssi_ = -200;
    last_sram_kb_ = 0xffffffff;
    last_psram_mb_ = 0xffffffff;
    last_cpu_mhz_ = 0xffffffff;
    last_uptime_min_ = 0xffffffff;
    last_sram_pct_ = -1;
    last_psram_pct_ = -1;
    last_temp_pct_ = -1;
    last_lux_pct_ = -1;
    last_motion_ok_ = false;
    last_light_ok_ = false;
    last_date_min_ = -1;
    UpdateTelemetry();

    if (update_timer_ != nullptr) {
        esp_timer_start_periodic(update_timer_, 500 * 1000); // 2 Hz (reduce redraws), xem sensor_card_screen.cc
    }
    if (auto_return_timer_ != nullptr) {
        esp_timer_stop(auto_return_timer_);
        esp_timer_start_once(auto_return_timer_, 120 * 1000 * 1000); // 120s auto-return
    }
}

void SensorDashboardScreen::Hide() {
    if (!IsVisible()) return;
    ESP_LOGI(TAG, "Hiding Sensor Dashboard screen");
    if (update_timer_ != nullptr) {
        esp_timer_stop(update_timer_);
    }
    if (auto_return_timer_ != nullptr) {
        esp_timer_stop(auto_return_timer_);
    }
    ReleaseForeground();
}
