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

#include "../smart_home_hub.h"

void DashboardScreen::OnAutoReturnTimeout(void* arg) {
    auto self = static_cast<DashboardScreen*>(arg);
    if (self && self->IsVisible()) {
        if (int64_t rem = SmartHomeHub::InactivityRemainingUs(); rem > 0) {
            esp_timer_start_once(self->auto_return_timer_, rem);
            return;
        }
        Application::GetInstance().Schedule([self]() {
            if (self->IsVisible()) {
                ESP_LOGI(TAG, "Dashboard auto-return (120s) to default screen (eyes)");
                SmartHomeHub::GetInstance().ReturnToDefaultScreen();
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
    lv_obj_set_style_bg_color(home_screen_, lv_color_hex(0x070C10), 0);

    // Header container
    lv_obj_t* header_cont = lv_obj_create(home_screen_);
    lv_obj_remove_style_all(header_cont);
    lv_obj_set_size(header_cont, 320, 36);
    lv_obj_align(header_cont, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header_cont, lv_color_hex(0x111B23), 0);
    lv_obj_set_style_bg_opa(header_cont, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(header_cont, 1, 0);
    lv_obj_set_style_border_color(header_cont, lv_color_hex(0x1E2A33), 0);
    lv_obj_set_style_pad_hor(header_cont, 10, 0);
    lv_obj_set_flex_flow(header_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    header_label_ = lv_label_create(header_cont);
    lv_label_set_text(header_label_, LV_SYMBOL_HOME " Smart Home");
    lv_obj_set_style_text_color(header_label_, lv_color_hex(0x29D3FF), 0);
    lv_obj_set_style_text_font(header_label_, &lv_font_montserrat_14, 0);

    lv_obj_t* close_btn = lv_button_create(header_cont);
    lv_obj_set_size(close_btn, 28, 26);
    lv_obj_set_style_bg_color(close_btn, lv_color_hex(0x1E2A33), 0);
    lv_obj_set_style_radius(close_btn, 4, 0);
    lv_obj_t* close_lbl = lv_label_create(close_btn);
    lv_label_set_text(close_lbl, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(close_lbl, lv_color_hex(0xFF5252), 0);
    lv_obj_center(close_lbl);
    lv_obj_add_event_cb(close_btn, [](lv_event_t* e) {
        auto self = static_cast<DashboardScreen*>(lv_event_get_user_data(e));
        if (self) SmartHomeHub::GetInstance().ReturnToDefaultScreen();
    }, LV_EVENT_CLICKED, this);

    // Grid Container - scrollable vertically
    grid_container_ = lv_obj_create(home_screen_);
    lv_obj_set_size(grid_container_, 312, 196);
    lv_obj_align(grid_container_, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_obj_set_style_bg_opa(grid_container_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid_container_, 0, 0);
    lv_obj_set_style_pad_all(grid_container_, 4, 0);
    lv_obj_set_flex_flow(grid_container_, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid_container_, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_scroll_dir(grid_container_, LV_DIR_VER);
}

void DashboardScreen::ReloadDevices(const std::vector<SmartDevice>& devices) {
    if (grid_container_ == nullptr) return;
    if (!lvgl_port_lock(200)) return;

    current_devices_ = devices;
    lv_obj_clean(grid_container_);

    if (devices.empty()) {
        lv_obj_t* empty_lbl = lv_label_create(grid_container_);
        lv_label_set_text(empty_lbl, "Chưa tìm thấy thiết bị nào.\nHãy kiểm tra cấu hình Home Assistant!");
        lv_obj_set_style_text_color(empty_lbl, lv_color_hex(0x7E93A3), 0);
        lv_obj_set_style_text_align(empty_lbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(empty_lbl, &lv_font_montserrat_14, 0);
        lv_obj_center(empty_lbl);
        lvgl_port_unlock();
        return;
    }

    for (const auto& dev : devices) {
        lv_obj_t* card = lv_button_create(grid_container_);
        lv_obj_set_size(card, 146, 68);
        lv_obj_set_style_radius(card, 8, 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_pad_all(card, 6, 0);

        uint32_t bg_col = dev.state ? 0x163428 : 0x111B23;
        uint32_t border_col = dev.state ? 0x22E06A : 0x1E2A33;
        uint32_t accent_col = dev.state ? 0x22E06A : 0x7E93A3;

        lv_obj_set_style_bg_color(card, lv_color_hex(bg_col), 0);
        lv_obj_set_style_border_color(card, lv_color_hex(border_col), 0);

        // Icon symbol based on device type
        const char* symbol = LV_SYMBOL_POWER;
        if (dev.type == DeviceType::Light) symbol = LV_SYMBOL_IMAGE;
        else if (dev.type == DeviceType::Climate) symbol = LV_SYMBOL_SETTINGS;
        else if (dev.type == DeviceType::Sensor) symbol = LV_SYMBOL_EYE_OPEN;

        // Container row for icon + name
        lv_obj_t* row = lv_obj_create(card);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, LV_PCT(100), 26);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t* icon_lbl = lv_label_create(row);
        lv_label_set_text(icon_lbl, symbol);
        lv_obj_set_style_text_color(icon_lbl, lv_color_hex(accent_col), 0);
        lv_obj_set_style_text_font(icon_lbl, &lv_font_montserrat_14, 0);

        lv_obj_t* name_lbl = lv_label_create(row);
        // Truncate long name to fit compact card
        std::string disp_name = dev.name;
        if (disp_name.length() > 14) {
            disp_name = disp_name.substr(0, 12) + "..";
        }
        lv_label_set_text(name_lbl, (" " + disp_name).c_str());
        lv_obj_set_style_text_color(name_lbl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(name_lbl, &lv_font_montserrat_14, 0);

        // State indicator label
        lv_obj_t* state_lbl = lv_label_create(card);
        lv_label_set_text(state_lbl, dev.state ? "BẬT (ON)" : "TẮT (OFF)");
        lv_obj_set_style_text_color(state_lbl, lv_color_hex(accent_col), 0);
        lv_obj_set_style_text_font(state_lbl, &lv_font_montserrat_14, 0);
        lv_obj_align(state_lbl, LV_ALIGN_BOTTOM_LEFT, 2, -2);

        // Store this pointer and device ID
        struct CardContext {
            DashboardScreen* self;
            std::string id;
        };
        auto* ctx = new CardContext{this, dev.id};
        lv_obj_set_user_data(card, ctx);

        // Click handler to toggle device
        lv_obj_add_event_cb(card, [](lv_event_t* e) {
            auto code = lv_event_get_code(e);
            lv_obj_t* target = (lv_obj_t*)lv_event_get_target(e);
            auto* ctx = static_cast<CardContext*>(lv_obj_get_user_data(target));
            if (!ctx) return;

            if (code == LV_EVENT_CLICKED) {
                ESP_LOGI(TAG, "Touch card clicked for device: %s", ctx->id.c_str());
                if (ctx->self) {
                    ctx->self->ResetAutoReturnTimer();
                    if (ctx->self->toggle_cb_) {
                        bool cur_state = false;
                        for (const auto& d : ctx->self->current_devices_) {
                            if (d.id == ctx->id) {
                                cur_state = d.state;
                                break;
                            }
                        }
                        ctx->self->toggle_cb_(ctx->id, !cur_state);
                    }
                }
            } else if (code == LV_EVENT_DELETE) {
                delete ctx;
            }
        }, LV_EVENT_ALL, nullptr);
    }

    lvgl_port_unlock();
}

void DashboardScreen::Show() {
    if (home_screen_ == nullptr) return;
    if (lvgl_port_lock(200)) {
        main_screen_ = lv_screen_active();
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
    is_visible_ = false;
}

void DashboardScreen::ResetAutoReturnTimer() {
    if (auto_return_timer_ != nullptr) {
        esp_timer_stop(auto_return_timer_);
        esp_timer_start_once(auto_return_timer_, 120 * 1000 * 1000); // 120s auto return
    }
}

void DashboardScreen::UpdateDeviceState(const SmartDevice& device) {
    ESP_LOGI(TAG, "Device state updated: %s -> %d", device.name.c_str(), device.state);
    for (auto& d : current_devices_) {
        if (d.id == device.id) {
            d.state = device.state;
            break;
        }
    }
    ReloadDevices(current_devices_);
}
