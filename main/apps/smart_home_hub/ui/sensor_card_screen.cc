#include "sensor_card_screen.h"
#include "../sensor_monitor.h"
#include "application.h"
#include <esp_log.h>
#include <esp_lvgl_port.h>
#include <ctime>
#include <cstdio>

#define TAG "SH_SensorCard"

// The mini-app is built for every board, but the large Montserrat faces are only
// compiled in on boards that ask for them (see boards/m5stack/core-s3/config.json).
// Undefined LV_FONT_*_N evaluates to 0 in #if, so this stays correct either way.
// LV_FONT_DECLARE gives a variable in LVGL 9, hence the &: LV_FONT_DEFAULT is
// already a pointer, so both branches end up as const lv_font_t*.
#if LV_FONT_MONTSERRAT_28
#define kFontBig (&lv_font_montserrat_28)
#else
#define kFontBig LV_FONT_DEFAULT
#endif
#if LV_FONT_MONTSERRAT_20
#define kFontMid (&lv_font_montserrat_20)
#else
#define kFontMid LV_FONT_DEFAULT
#endif

namespace {

constexpr uint32_t kBg = 0x070C10;
constexpr uint32_t kCardBg = 0x0F1922;
constexpr uint32_t kTrackBg = 0x1E2A33;
constexpr uint32_t kLine = 0x2A3B47;
constexpr uint32_t kText = 0xFFFFFF;
constexpr uint32_t kDim = 0x7E93A3;
constexpr uint32_t kCyan = 0x29D3FF;
constexpr uint32_t kGreen = 0x22E06A;
constexpr uint32_t kBlue = 0x2E9BFF;
constexpr uint32_t kYellow = 0xFFC21A;
constexpr uint32_t kOrange = 0xFF6B35;

constexpr int kFrameX = 2;
constexpr int kFrameY = 2;
constexpr int kFrameW = 316;
constexpr int kFrameH = 236;
constexpr int kPad = 12;

const char* const kWeekday[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};

struct TypeStyle {
    uint32_t accent;
    const char* title;
    const char* unit;
    bool bar;
    bool chart;
    bool columns;
    bool wifi_bars;
    int32_t chart_min;
    int32_t chart_max;
};

constexpr int kTypeCount = static_cast<int>(SensorCardType::kCount);
const TypeStyle kStyles[kTypeCount] = {
    /* Temperature */ {kOrange, "TEMPERATURE", "C", false, true, false, false, 20, 60},
    /* Battery     */ {kGreen, "BATTERY", "%", true, false, false, false, 0, 100},
    /* Light       */ {kYellow, "LIGHT", "lux", true, true, false, false, 0, 1000},
    /* Motion      */ {kGreen, "IMU (ACC + GYR)", "", false, false, true, false, 0, 100},
    /* Network     */ {kBlue, "Wi-Fi", "", false, false, true, true, 0, 100},
    /* System      */ {kCyan, "SYSTEM", "", false, false, true, false, 0, 100},
};

lv_obj_t* MakeBox(lv_obj_t* parent, int x, int y, int w, int h, uint32_t color, int radius) {
    lv_obj_t* obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
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

void SetVisible(lv_obj_t* obj, bool visible) {
    if (obj == nullptr) return;
    if (visible) {
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}

}  // namespace

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

    if (lvgl_port_lock(200)) {
        CreateUI();
        lvgl_port_unlock();
    } else {
        ESP_LOGE(TAG, "LVGL busy: sensor card UI not created");
    }
}

void SensorCardScreen::CreateUI() {
    screen_ = lv_obj_create(NULL);
    lv_obj_remove_style_all(screen_);
    lv_obj_set_style_bg_color(screen_, lv_color_hex(kBg), 0);
    lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen_, LV_OBJ_FLAG_SCROLLABLE);

    frame_ = lv_obj_create(screen_);
    lv_obj_remove_style_all(frame_);
    lv_obj_set_pos(frame_, kFrameX, kFrameY);
    lv_obj_set_size(frame_, kFrameW, kFrameH);
    lv_obj_set_style_bg_color(frame_, lv_color_hex(kCardBg), 0);
    lv_obj_set_style_bg_opa(frame_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(frame_, 1, 0);
    lv_obj_set_style_border_color(frame_, lv_color_hex(kCyan), 0);
    lv_obj_set_style_radius(frame_, 8, 0);
    lv_obj_clear_flag(frame_, LV_OBJ_FLAG_SCROLLABLE);

    // ------------------------------------------------------------ status bar
    clock_label_ = MakeLabel(frame_, kPad, 6, &lv_font_montserrat_14, kText, "--:--");
    date_label_ = MakeLabel(frame_, 54, 6, &lv_font_montserrat_14, kDim, "--/--/--");
    wifi_label_ = MakeLabel(frame_, 244, 5, &lv_font_montserrat_14, kDim, LV_SYMBOL_WIFI);
    bat_label_ = MakeLabel(frame_, 0, 6, &lv_font_montserrat_14, kGreen, "--");
    lv_obj_set_width(bat_label_, kFrameW - 2 * kPad);
    lv_obj_set_style_text_align(bat_label_, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(bat_label_, LV_ALIGN_TOP_RIGHT, -kPad, 6);

    MakeBox(frame_, kPad, 26, kFrameW - 2 * kPad, 1, kLine, 0);

    // ------------------------------------------------------------------ body
    icon_box_ = lv_obj_create(frame_);
    lv_obj_remove_style_all(icon_box_);
    lv_obj_set_pos(icon_box_, kPad, 32);
    lv_obj_set_size(icon_box_, 34, 34);
    lv_obj_clear_flag(icon_box_, LV_OBJ_FLAG_SCROLLABLE);

    title_label_ = MakeLabel(frame_, 56, 32, kFontMid, kCyan, "--");
    value_label_ = MakeLabel(frame_, 56, 58, kFontBig, kText, "--");
    unit_label_ = MakeLabel(frame_, 0, 72, &lv_font_montserrat_14, kDim, "");
    sub_label_ = MakeLabel(frame_, 56, 92, &lv_font_montserrat_14, kDim, "");

    bar_ = lv_bar_create(frame_);
    lv_obj_remove_style_all(bar_);
    lv_obj_set_pos(bar_, kPad, 112);
    lv_obj_set_size(bar_, kFrameW - 2 * kPad, 8);
    lv_obj_set_style_bg_color(bar_, lv_color_hex(kTrackBg), 0);
    lv_obj_set_style_bg_opa(bar_, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(bar_, 4, 0);
    lv_bar_set_value(bar_, 0, LV_ANIM_OFF);

    // ----------------------------------------------------------------- chart
    chart_max_ = MakeLabel(frame_, 6, 130, &lv_font_montserrat_14, kDim, "--");
    chart_mid_ = MakeLabel(frame_, 6, 152, &lv_font_montserrat_14, kDim, "--");
    chart_min_ = MakeLabel(frame_, 6, 176, &lv_font_montserrat_14, kDim, "--");

    // Sparkline: a bare line chart, no grid, no point bullets.
    chart_ = lv_chart_create(frame_);
    lv_obj_remove_style_all(chart_);
    lv_obj_set_pos(chart_, 46, 128);
    lv_obj_set_size(chart_, kFrameW - kPad - 46 - 6, 68);
    lv_obj_clear_flag(chart_, LV_OBJ_FLAG_SCROLLABLE);
    lv_chart_set_type(chart_, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart_, kChartPoints);
    lv_chart_set_update_mode(chart_, LV_CHART_UPDATE_MODE_CIRCULAR);
    lv_chart_set_div_line_count(chart_, 0, 0);
    lv_obj_set_style_line_width(chart_, 2, LV_PART_ITEMS);
    lv_obj_set_style_line_rounded(chart_, true, LV_PART_ITEMS);
    lv_obj_set_style_size(chart_, 0, 0, LV_PART_INDICATOR);
    chart_series_ = lv_chart_add_series(chart_, lv_color_hex(kCyan), LV_CHART_AXIS_PRIMARY_Y);

    // ------------------------------------------------------------ axis / rows
    left_column_ = MakeLabel(frame_, 56, 128, &lv_font_montserrat_14, kText, "");
    right_column_ = MakeLabel(frame_, 186, 128, &lv_font_montserrat_14, kText, "");
    for (int i = 0; i < 4; i++) {
        lv_obj_t* b = MakeBox(frame_, 252 + i * 13, 84 - (10 + i * 6), 8, 10 + i * 6, kDim, 2);
        lv_obj_set_style_bg_opa(b, LV_OPA_40, 0);
        wifi_bars_[i] = b;
    }

    // ----------------------------------------------------------------- footer
    for (int i = 0; i < kTypeCount; i++) {
        dots_[i] = MakeBox(frame_, kPad + i * 16, 220, 8, 8, kDim, 4);
    }
    lv_obj_t* brand = MakeLabel(frame_, 0, 220, &lv_font_montserrat_14, kDim, "M5STACK CORE S3");
    lv_obj_set_width(brand, kFrameW - 2 * kPad);
    lv_obj_set_style_text_align(brand, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(brand, LV_ALIGN_BOTTOM_RIGHT, -kPad, 0);

    lv_obj_add_flag(screen_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(
        screen_,
        [](lv_event_t* e) {
            auto self = static_cast<SensorCardScreen*>(lv_event_get_user_data(e));
            if (self) {
                self->Hide();
            }
        },
        LV_EVENT_CLICKED, this);
}

void SensorCardScreen::BuildIcon(SensorCardType type) {
    const uint32_t accent = kStyles[static_cast<int>(type)].accent;
    lv_obj_clean(icon_box_);

    switch (type) {
        case SensorCardType::Temperature:  // thermometer
            MakeBox(icon_box_, 14, 3, 6, 20, accent, 3);
            MakeBox(icon_box_, 10, 19, 14, 14, accent, 7);
            break;
        case SensorCardType::Light:  // sun
            MakeBox(icon_box_, 10, 10, 14, 14, accent, 7);
            MakeBox(icon_box_, 16, 1, 2, 7, accent, 1);
            MakeBox(icon_box_, 16, 26, 2, 7, accent, 1);
            MakeBox(icon_box_, 1, 16, 7, 2, accent, 1);
            MakeBox(icon_box_, 26, 16, 7, 2, accent, 1);
            break;
        case SensorCardType::Motion:  // 6-axis crosshair
            MakeBox(icon_box_, 2, 16, 30, 3, accent, 1);
            MakeBox(icon_box_, 16, 2, 3, 30, accent, 1);
            break;
        case SensorCardType::Battery: {  // battery outline + cells
            lv_obj_t* shell = MakeBox(icon_box_, 4, 10, 26, 14, kTrackBg, 3);
            lv_obj_set_style_border_width(shell, 2, 0);
            lv_obj_set_style_border_color(shell, lv_color_hex(accent), 0);
            MakeBox(icon_box_, 30, 14, 3, 6, accent, 1);
            for (int i = 0; i < 3; i++) {
                MakeBox(icon_box_, 8 + i * 7, 13, 5, 8, accent, 1);
            }
            break;
        }
        case SensorCardType::Network: {
            lv_obj_t* sym = lv_label_create(icon_box_);
            lv_label_set_text(sym, LV_SYMBOL_WIFI);
            lv_obj_set_style_text_font(sym, kFontBig, 0);
            lv_obj_set_style_text_color(sym, lv_color_hex(accent), 0);
            lv_obj_align(sym, LV_ALIGN_LEFT_MID, 0, 0);
            break;
        }
        case SensorCardType::System: {
            lv_obj_t* sym = lv_label_create(icon_box_);
            lv_label_set_text(sym, LV_SYMBOL_SETTINGS);
            lv_obj_set_style_text_font(sym, kFontBig, 0);
            lv_obj_set_style_text_color(sym, lv_color_hex(accent), 0);
            lv_obj_align(sym, LV_ALIGN_LEFT_MID, 0, 0);
            break;
        }
        default:
            break;
    }
}

void SensorCardScreen::ApplyType(SensorCardType type) {
    const TypeStyle& style = kStyles[static_cast<int>(type)];
    lv_obj_set_style_border_color(frame_, lv_color_hex(style.accent), 0);
    lv_obj_set_style_text_color(title_label_, lv_color_hex(style.accent), 0);
    lv_label_set_text(title_label_, style.title);
    lv_label_set_text(unit_label_, style.unit);
    lv_obj_set_style_text_color(unit_label_, lv_color_hex(style.accent), 0);
    // Network recolours the value per state; reset it for every other page.
    lv_obj_set_style_text_color(value_label_, lv_color_hex(kText), 0);
    lv_obj_set_style_bg_color(bar_, lv_color_hex(style.accent), LV_PART_INDICATOR);
    lv_chart_set_series_color(chart_, chart_series_, lv_color_hex(style.accent));

    SetVisible(bar_, style.bar);
    SetVisible(chart_, style.chart);
    SetVisible(chart_max_, style.chart);
    SetVisible(chart_mid_, style.chart);
    SetVisible(chart_min_, style.chart);
    SetVisible(left_column_, style.columns);
    SetVisible(right_column_, style.columns);
    for (int i = 0; i < 4; i++) {
        SetVisible(wifi_bars_[i], style.wifi_bars);
    }

    // Restart the sparkline window whenever the page changes.
    chart_min_v_ = style.chart_min;
    chart_max_v_ = style.chart_max;
    if (style.chart) {
        const int32_t mid = (style.chart_min + style.chart_max) / 2;
        lv_chart_set_axis_range(chart_, LV_CHART_AXIS_PRIMARY_Y, style.chart_min, style.chart_max);
        lv_chart_set_all_values(chart_, chart_series_, mid);
        lv_label_set_text_fmt(chart_max_, "%ld", static_cast<long>(style.chart_max));
        lv_label_set_text_fmt(chart_mid_, "%ld", static_cast<long>(mid));
        lv_label_set_text_fmt(chart_min_, "%ld", static_cast<long>(style.chart_min));
    }

    for (int i = 0; i < kTypeCount; i++) {
        lv_obj_set_style_bg_color(dots_[i], lv_color_hex(i == static_cast<int>(type) ? style.accent : kDim), 0);
        lv_obj_set_style_bg_opa(dots_[i], i == static_cast<int>(type) ? LV_OPA_COVER : LV_OPA_40, 0);
    }

    BuildIcon(type);
}

void SensorCardScreen::PushChartSample(int value) {
    if (value < chart_min_v_) value = chart_min_v_;
    if (value > chart_max_v_) value = chart_max_v_;
    lv_chart_set_next_value(chart_, chart_series_, value);
}

void SensorCardScreen::UpdateData() {
    CoreS3SensorSnapshot s = SensorMonitor::GetInstance().GetSnapshot();
    const TypeStyle& style = kStyles[static_cast<int>(current_type_)];
    char buf[48];

    if (lvgl_port_lock(200)) {
        // Status bar is shared by every page.
        const time_t now = time(nullptr);
        if (now > 1700000000) {
            struct tm tmv;
            localtime_r(&now, &tmv);
            lv_label_set_text_fmt(clock_label_, "%02d:%02d", tmv.tm_hour, tmv.tm_min);
            lv_label_set_text_fmt(date_label_, "%04d-%02d-%02d %s", tmv.tm_year + 1900,
                                 tmv.tm_mon + 1, tmv.tm_mday, kWeekday[tmv.tm_wday % 7]);
        } else {
            lv_label_set_text(clock_label_, "--:--");
            lv_label_set_text(date_label_, "NO SYNC");
        }
        lv_label_set_text_fmt(bat_label_, "%d%%", s.power.battery_level);
        lv_obj_set_style_text_color(wifi_label_,
                                    lv_color_hex(s.network.rssi_dbm > -90 ? kBlue : kDim), 0);

        switch (current_type_) {
            case SensorCardType::Temperature: {
                const int temp = static_cast<int>(s.power.temperature_c);
                lv_label_set_text_fmt(value_label_, "%.1f", s.power.temperature_c);
                lv_label_set_text(sub_label_, temp < 45 ? "Status  Normal" : "Status  Warm");
                PushChartSample(temp);
                break;
            }
            case SensorCardType::Battery: {
                int pct = s.power.battery_level;
                if (pct < 0) pct = 0;
                if (pct > 100) pct = 100;
                lv_label_set_text_fmt(value_label_, "%d", pct);
                lv_label_set_text(sub_label_, s.power.is_charging     ? "Status  Charging"
                                      : s.power.is_discharging   ? "Status  On battery"
                                                                  : "Status  Idle");
                lv_bar_set_value(bar_, pct, LV_ANIM_OFF);
                break;
            }
            case SensorCardType::Light: {
                if (s.light.available) {
                    lv_label_set_text_fmt(value_label_, "%u", static_cast<unsigned int>(s.light.lux));
                    lv_label_set_text_fmt(sub_label_, "Proximity  %u",
                                          static_cast<unsigned int>(s.light.proximity));
                    PushChartSample(static_cast<int>(s.light.lux));
                    int lux_pct = static_cast<int>(s.light.lux / 10.0f);
                    if (lux_pct > 100) lux_pct = 100;
                    lv_bar_set_value(bar_, lux_pct, LV_ANIM_OFF);
                } else {
                    lv_label_set_text(value_label_, "--");
                    lv_label_set_text(sub_label_, "Sensor not responding");
                    lv_bar_set_value(bar_, 0, LV_ANIM_OFF);
                }
                break;
            }
            case SensorCardType::Motion: {
                if (s.motion.available) {
                    lv_label_set_text_fmt(left_column_,
                                         "ACC (g)\n"
                                         "X  %+.2f\n"
                                         "Y  %+.2f\n"
                                         "Z  %+.2f",
                                         s.motion.accel_x, s.motion.accel_y, s.motion.accel_z);
                    lv_label_set_text_fmt(right_column_,
                                         "GYR (d/s)\n"
                                         "X  %+.2f\n"
                                         "Y  %+.2f\n"
                                         "Z  %+.2f",
                                         s.motion.gyro_x, s.motion.gyro_y, s.motion.gyro_z);
                    lv_label_set_text_fmt(sub_label_, "Tilt  %.0f deg", s.motion.tilt_degrees);
                } else {
                    lv_label_set_text(left_column_, "ACC (g)\nX  --.--\nY  --.--\nZ  --.--");
                    lv_label_set_text(right_column_, "GYR (d/s)\nX  --.--\nY  --.--\nZ  --.--");
                    lv_label_set_text(sub_label_, "IMU not responding");
                }
                break;
            }
            case SensorCardType::Network: {
                const bool up = s.network.rssi_dbm > -90;
                lv_label_set_text(value_label_, up ? "Connected" : "Offline");
                lv_obj_set_style_text_color(value_label_, lv_color_hex(up ? kGreen : kOrange), 0);
                lv_label_set_text(left_column_, "SSID\n\nIP");
                lv_label_set_text_fmt(right_column_, "%s\n\n%s", s.network.ssid.c_str(),
                                      s.network.ip_address.c_str());
                lv_label_set_text_fmt(sub_label_, "%d dBm   ch %d", s.network.rssi_dbm,
                                      s.network.channel);
                int active = s.network.rssi_dbm >= -55   ? 4
                             : s.network.rssi_dbm >= -67 ? 3
                             : s.network.rssi_dbm >= -78 ? 2
                                                         : (up ? 1 : 0);
                for (int i = 0; i < 4; i++) {
                    lv_obj_set_style_bg_color(wifi_bars_[i],
                                              lv_color_hex(i < active ? kGreen : kDim), 0);
                    lv_obj_set_style_bg_opa(wifi_bars_[i], i < active ? LV_OPA_COVER : LV_OPA_40, 0);
                }
                break;
            }
            case SensorCardType::System: {
                lv_label_set_text_fmt(value_label_, "%luK",
                                      static_cast<unsigned long>(s.system.free_sram_bytes / 1024));
                lv_label_set_text(left_column_, "PSRAM\n\nCPU");
                lv_label_set_text_fmt(right_column_, "%lu MB\n\n%lu MHz",
                                      static_cast<unsigned long>(s.system.free_psram_bytes / (1024 * 1024)),
                                      static_cast<unsigned long>(s.system.cpu_freq_mhz));
                const uint32_t seconds = s.system.uptime_seconds;
                snprintf(buf, sizeof(buf), "Up  %02lu:%02lu:%02lu", seconds / 3600, (seconds / 60) % 60,
                         seconds % 60);
                lv_label_set_text(sub_label_, buf);
                break;
            }
            default:
                break;
        }

        // Keep the unit glued to the end of the (variable width) big value.
        if (style.unit[0] != '\0') {
            lv_obj_update_layout(value_label_);
            lv_obj_align(unit_label_, LV_ALIGN_TOP_LEFT,
                         lv_obj_get_x(value_label_) + lv_obj_get_width(value_label_) + 5, 72);
        }

        lvgl_port_unlock();
    }
}

void SensorCardScreen::ResetAutoReturnTimer() {
    if (auto_return_timer_ != nullptr) {
        esp_timer_stop(auto_return_timer_);
        esp_timer_start_once(auto_return_timer_, 15 * 1000 * 1000); // 15 giay auto return
    }
}

void SensorCardScreen::Show(SensorCardType type) {
    if (screen_ == nullptr) return;
    current_type_ = type;

    if (lvgl_port_lock(200)) {
        main_screen_ = lv_screen_active();
        // Synchronous load, see DashboardScreen::Show().
        lv_screen_load(screen_);
        ApplyType(type);
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
