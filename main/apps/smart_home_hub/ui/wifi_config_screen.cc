#include "wifi_config_screen.h"
#include <esp_lvgl_port.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <wifi_manager.h>
#include <ssid_manager.h>
#include "application.h"
#include <algorithm>

#define TAG "WifiConfigScreen"

namespace {

constexpr uint32_t kBg = 0x0F1115;
constexpr uint32_t kCardBg = 0x161A20;
constexpr uint32_t kModalBg = 0x1E242E;
constexpr uint32_t kLine = 0x242A34;
constexpr uint32_t kCyan = 0x00E5FF;
constexpr uint32_t kGreen = 0x00E676;
constexpr uint32_t kOrange = 0xFF9100;
constexpr uint32_t kText = 0xE6EDF3;
constexpr uint32_t kDim = 0x8B949E;

}  // namespace

WifiConfigScreen::WifiConfigScreen() = default;

WifiConfigScreen::~WifiConfigScreen() = default;

void WifiConfigScreen::Initialize(lv_display_t* display) {
    display_ = display;
    if (lvgl_port_lock(200)) {
        CreateUI();
        lvgl_port_unlock();
    } else {
        ESP_LOGE(TAG, "LVGL busy: WiFi config UI not created");
    }
}

void WifiConfigScreen::CreateUI() {
    screen_ = lv_obj_create(NULL);
    lv_obj_remove_style_all(screen_);
    lv_obj_set_style_bg_color(screen_, lv_color_hex(kBg), 0);
    lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen_, LV_OBJ_FLAG_SCROLLABLE);

    // Frame
    lv_obj_t* frame = lv_obj_create(screen_);
    lv_obj_remove_style_all(frame);
    lv_obj_set_pos(frame, 5, 4);
    lv_obj_set_size(frame, 310, 232);
    lv_obj_set_style_bg_color(frame, lv_color_hex(kCardBg), 0);
    lv_obj_set_style_bg_opa(frame, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(frame, 1, 0);
    lv_obj_set_style_border_color(frame, lv_color_hex(kCyan), 0);
    lv_obj_set_style_radius(frame, 8, 0);
    lv_obj_clear_flag(frame, LV_OBJ_FLAG_SCROLLABLE);

    // Header Back button
    close_btn_ = lv_btn_create(frame);
    lv_obj_set_pos(close_btn_, 8, 8);
    lv_obj_set_size(close_btn_, 32, 26);
    lv_obj_set_style_bg_color(close_btn_, lv_color_hex(0x28303C), 0);
    lv_obj_set_style_radius(close_btn_, 4, 0);
    lv_obj_t* back_lbl = lv_label_create(close_btn_);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(back_lbl, lv_color_hex(kCyan), 0);
    lv_obj_center(back_lbl);
    lv_obj_add_event_cb(close_btn_, [](lv_event_t* e) {
        auto self = static_cast<WifiConfigScreen*>(lv_event_get_user_data(e));
        if (self) self->Hide();
    }, LV_EVENT_CLICKED, this);

    // Title
    title_label_ = lv_label_create(frame);
    lv_obj_set_pos(title_label_, 46, 12);
    lv_obj_set_style_text_font(title_label_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title_label_, lv_color_hex(kCyan), 0);
    lv_label_set_text(title_label_, "WI-FI NETWORKS");

    // Scan button
    scan_btn_ = lv_btn_create(frame);
    lv_obj_set_pos(scan_btn_, 238, 8);
    lv_obj_set_size(scan_btn_, 64, 26);
    lv_obj_set_style_bg_color(scan_btn_, lv_color_hex(kCyan), 0);
    lv_obj_set_style_radius(scan_btn_, 4, 0);
    lv_obj_t* scan_lbl = lv_label_create(scan_btn_);
    lv_label_set_text(scan_lbl, "SCAN");
    lv_obj_set_style_text_font(scan_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(scan_lbl, lv_color_hex(0x000000), 0);
    lv_obj_center(scan_lbl);
    lv_obj_add_event_cb(scan_btn_, [](lv_event_t* e) {
        auto self = static_cast<WifiConfigScreen*>(lv_event_get_user_data(e));
        if (self) self->StartScan();
    }, LV_EVENT_CLICKED, this);

    // Sub-header status & Portal AP button
    status_label_ = lv_label_create(frame);
    lv_obj_set_pos(status_label_, 10, 38);
    lv_obj_set_style_text_font(status_label_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(status_label_, lv_color_hex(kDim), 0);
    lv_label_set_text(status_label_, "Tap network to connect");

    portal_btn_ = lv_btn_create(frame);
    lv_obj_set_pos(portal_btn_, 214, 36);
    lv_obj_set_size(portal_btn_, 88, 22);
    lv_obj_set_style_bg_color(portal_btn_, lv_color_hex(0x28303C), 0);
    lv_obj_set_style_radius(portal_btn_, 3, 0);
    lv_obj_t* portal_lbl = lv_label_create(portal_btn_);
    lv_label_set_text(portal_lbl, "Web Portal");
    lv_obj_set_style_text_font(portal_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(portal_lbl, lv_color_hex(kOrange), 0);
    lv_obj_center(portal_lbl);
    lv_obj_add_event_cb(portal_btn_, [](lv_event_t* e) {
        auto self = static_cast<WifiConfigScreen*>(lv_event_get_user_data(e));
        if (self) {
            WifiManager::GetInstance().StartConfigAp();
            lv_label_set_text(self->status_label_, "AP Mode: 192.168.4.1");
        }
    }, LV_EVENT_CLICKED, this);

    // List container
    list_container_ = lv_obj_create(frame);
    lv_obj_remove_style_all(list_container_);
    lv_obj_set_pos(list_container_, 6, 62);
    lv_obj_set_size(list_container_, 298, 162);
    lv_obj_set_flex_flow(list_container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(list_container_, 4, 0);
    lv_obj_set_style_pad_row(list_container_, 4, 0);
    lv_obj_set_style_bg_color(list_container_, lv_color_hex(0x10141A), 0);
    lv_obj_set_style_bg_opa(list_container_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(list_container_, 1, 0);
    lv_obj_set_style_border_color(list_container_, lv_color_hex(kLine), 0);
    lv_obj_set_style_radius(list_container_, 4, 0);
    lv_obj_add_flag(list_container_, LV_OBJ_FLAG_SCROLLABLE);

    // Modal background overlay (hidden initially)
    modal_obj_ = lv_obj_create(screen_);
    lv_obj_remove_style_all(modal_obj_);
    lv_obj_set_pos(modal_obj_, 12, 10);
    lv_obj_set_size(modal_obj_, 296, 220);
    lv_obj_set_style_bg_color(modal_obj_, lv_color_hex(kModalBg), 0);
    lv_obj_set_style_bg_opa(modal_obj_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(modal_obj_, 2, 0);
    lv_obj_set_style_border_color(modal_obj_, lv_color_hex(kCyan), 0);
    lv_obj_set_style_radius(modal_obj_, 8, 0);
    lv_obj_clear_flag(modal_obj_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(modal_obj_, LV_OBJ_FLAG_HIDDEN);

    modal_ssid_label_ = lv_label_create(modal_obj_);
    lv_obj_set_pos(modal_ssid_label_, 12, 8);
    lv_obj_set_style_text_font(modal_ssid_label_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(modal_ssid_label_, lv_color_hex(kCyan), 0);
    lv_label_set_text(modal_ssid_label_, "SSID: --");

    pwd_textarea_ = lv_textarea_create(modal_obj_);
    lv_obj_set_pos(pwd_textarea_, 12, 30);
    lv_obj_set_size(pwd_textarea_, 204, 30);
    lv_textarea_set_password_mode(pwd_textarea_, true);
    lv_textarea_set_one_line(pwd_textarea_, true);
    lv_textarea_set_placeholder_text(pwd_textarea_, "Password");
    lv_obj_set_style_bg_color(pwd_textarea_, lv_color_hex(0x10141A), 0);
    lv_obj_set_style_text_color(pwd_textarea_, lv_color_hex(kText), 0);
    lv_obj_set_style_border_color(pwd_textarea_, lv_color_hex(kCyan), 0);

    // Connect button
    lv_obj_t* conn_btn = lv_btn_create(modal_obj_);
    lv_obj_set_pos(conn_btn, 222, 30);
    lv_obj_set_size(conn_btn, 62, 30);
    lv_obj_set_style_bg_color(conn_btn, lv_color_hex(kGreen), 0);
    lv_obj_set_style_radius(conn_btn, 4, 0);
    lv_obj_t* conn_lbl = lv_label_create(conn_btn);
    lv_label_set_text(conn_lbl, "JOIN");
    lv_obj_set_style_text_font(conn_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(conn_lbl, lv_color_hex(0x000000), 0);
    lv_obj_center(conn_lbl);
    lv_obj_add_event_cb(conn_btn, [](lv_event_t* e) {
        auto self = static_cast<WifiConfigScreen*>(lv_event_get_user_data(e));
        if (self) {
            const char* pwd = lv_textarea_get_text(self->pwd_textarea_);
            self->ConnectToAp(self->target_ssid_, pwd ? pwd : "");
        }
    }, LV_EVENT_CLICKED, this);

    // Cancel modal button
    lv_obj_t* cancel_btn = lv_btn_create(modal_obj_);
    lv_obj_set_pos(cancel_btn, 256, 4);
    lv_obj_set_size(cancel_btn, 28, 22);
    lv_obj_set_style_bg_color(cancel_btn, lv_color_hex(0x323B47), 0);
    lv_obj_set_style_radius(cancel_btn, 3, 0);
    lv_obj_t* cancel_lbl = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_lbl, "X");
    lv_obj_set_style_text_color(cancel_lbl, lv_color_hex(kOrange), 0);
    lv_obj_center(cancel_lbl);
    lv_obj_add_event_cb(cancel_btn, [](lv_event_t* e) {
        auto self = static_cast<WifiConfigScreen*>(lv_event_get_user_data(e));
        if (self) self->HideConnectModal();
    }, LV_EVENT_CLICKED, this);

    // On-screen Keypad via standard button matrix
    static const char* const kKeyMap[] = {
        "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "\n",
        "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "\n",
        "a", "s", "d", "f", "g", "h", "j", "k", "l", "\n",
        "z", "x", "c", "v", "b", "n", "m", "_", "-", "@", "\n",
        LV_SYMBOL_BACKSPACE, "Clr", "Space", "OK", ""
    };
    keyboard_ = lv_btnmatrix_create(modal_obj_);
    lv_btnmatrix_set_map(keyboard_, kKeyMap);
    lv_obj_set_pos(keyboard_, 6, 68);
    lv_obj_set_size(keyboard_, 284, 144);
    lv_obj_set_style_bg_color(keyboard_, lv_color_hex(0x161C24), 0);
    lv_obj_set_style_border_width(keyboard_, 0, 0);
    lv_obj_add_event_cb(keyboard_, [](lv_event_t* e) {
        lv_obj_t* obj = static_cast<lv_obj_t*>(lv_event_get_target(e));
        auto self = static_cast<WifiConfigScreen*>(lv_event_get_user_data(e));
        if (!obj || !self) return;
        uint32_t btn_id = lv_btnmatrix_get_selected_btn(obj);
        const char* txt = lv_btnmatrix_get_btn_text(obj, btn_id);
        if (!txt) return;
        if (strcmp(txt, LV_SYMBOL_BACKSPACE) == 0) {
            lv_textarea_delete_char(self->pwd_textarea_);
        } else if (strcmp(txt, "Clr") == 0) {
            lv_textarea_set_text(self->pwd_textarea_, "");
        } else if (strcmp(txt, "Space") == 0) {
            lv_textarea_add_char(self->pwd_textarea_, ' ');
        } else if (strcmp(txt, "OK") == 0) {
            const char* pwd = lv_textarea_get_text(self->pwd_textarea_);
            self->ConnectToAp(self->target_ssid_, pwd ? pwd : "");
        } else {
            lv_textarea_add_text(self->pwd_textarea_, txt);
        }
    }, LV_EVENT_VALUE_CHANGED, this);
}

void WifiConfigScreen::ShowConnectModal(const std::string& ssid) {
    target_ssid_ = ssid;
    lv_label_set_text_fmt(modal_ssid_label_, "SSID: %s", ssid.c_str());
    lv_textarea_set_text(pwd_textarea_, "");
    lv_obj_clear_flag(modal_obj_, LV_OBJ_FLAG_HIDDEN);
}

void WifiConfigScreen::HideConnectModal() {
    lv_obj_add_flag(modal_obj_, LV_OBJ_FLAG_HIDDEN);
}

void WifiConfigScreen::ConnectToAp(const std::string& ssid, const std::string& password) {
    ESP_LOGI(TAG, "Connecting to SSID: %s", ssid.c_str());
    HideConnectModal();
    lv_label_set_text_fmt(status_label_, "Connecting: %s...", ssid.c_str());

    SsidManager::GetInstance().AddSsid(ssid, password);
    WifiManager::GetInstance().StartStation();
}

void WifiConfigScreen::RefreshApList() {
    lv_obj_clean(list_container_);

    if (ap_list_.empty()) {
        lv_obj_t* empty_lbl = lv_label_create(list_container_);
        lv_label_set_text(empty_lbl, LV_SYMBOL_WARNING "  No networks found");
        lv_obj_set_style_text_font(empty_lbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(empty_lbl, lv_color_hex(kOrange), 0);
        lv_obj_set_style_pad_all(empty_lbl, 12, 0);
        return;
    }

    for (const auto& ap : ap_list_) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%-18.18s [%ddBm]", ap.ssid.c_str(), ap.rssi);
        const char* icon = (ap.rssi >= -65) ? LV_SYMBOL_WIFI : (ap.rssi >= -80 ? LV_SYMBOL_WIFI : LV_SYMBOL_WARNING);

        lv_obj_t* btn = lv_btn_create(list_container_);
        lv_obj_remove_style_all(btn);
        lv_obj_set_width(btn, 286);
        lv_obj_set_height(btn, 32);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x161C24), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(btn, 4, 0);
        lv_obj_set_style_pad_hor(btn, 8, 0);
        lv_obj_set_style_pad_ver(btn, 4, 0);

        lv_obj_t* icon_lbl = lv_label_create(btn);
        lv_label_set_text(icon_lbl, icon);
        lv_obj_align(icon_lbl, LV_ALIGN_LEFT_MID, 4, 0);
        lv_obj_set_style_text_color(icon_lbl, lv_color_hex(ap.is_saved ? kGreen : kCyan), 0);

        lv_obj_t* text_lbl = lv_label_create(btn);
        lv_label_set_text(text_lbl, buf);
        lv_obj_set_style_text_font(text_lbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(text_lbl, lv_color_hex(ap.is_saved ? kGreen : kText), 0);
        lv_obj_align(text_lbl, LV_ALIGN_LEFT_MID, 26, 0);

        // Store SSID in user data via a heap string attached to button
        auto ssid_copy = new std::string(ap.ssid);
        lv_obj_set_user_data(btn, ssid_copy);
        lv_obj_add_event_cb(btn, [](lv_event_t* e) {
            auto target_obj = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
            auto ptr = static_cast<std::string*>(lv_obj_get_user_data(target_obj));
            delete ptr;
        }, LV_EVENT_DELETE, nullptr);
        lv_obj_add_event_cb(btn, [](lv_event_t* e) {
            auto self = static_cast<WifiConfigScreen*>(lv_event_get_user_data(e));
            auto target_obj = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
            auto ssid_ptr = static_cast<std::string*>(lv_obj_get_user_data(target_obj));
            if (self && ssid_ptr) {
                self->ShowConnectModal(*ssid_ptr);
            }
        }, LV_EVENT_CLICKED, this);
    }
}

void WifiConfigScreen::StartScan() {
    if (scanning_) return;
    scanning_ = true;
    lv_label_set_text(status_label_, "Scanning Wi-Fi...");

    Application::GetInstance().Schedule([this]() {
        wifi_scan_config_t scan_cfg = {};
        scan_cfg.show_hidden = false;
        scan_cfg.scan_type = WIFI_SCAN_TYPE_ACTIVE;
        scan_cfg.scan_time.active.min = 100;
        scan_cfg.scan_time.active.max = 250;

        esp_wifi_scan_start(&scan_cfg, true);

        uint16_t ap_count = 0;
        esp_wifi_scan_get_ap_num(&ap_count);
        if (ap_count > 20) ap_count = 20;

        std::vector<wifi_ap_record_t> records(ap_count);
        if (ap_count > 0) {
            esp_wifi_scan_get_ap_records(&ap_count, records.data());
        }

        const auto& saved_list = SsidManager::GetInstance().GetSsidList();

        std::vector<ScannedApInfo> results;
        for (uint16_t i = 0; i < ap_count; i++) {
            std::string ssid = reinterpret_cast<char*>(records[i].ssid);
            if (ssid.empty()) continue;

            bool saved = false;
            for (const auto& s : saved_list) {
                if (s.ssid == ssid) {
                    saved = true;
                    break;
                }
            }
            results.push_back({ssid, records[i].rssi, records[i].authmode, saved});
        }

        // Sort strongest signal first
        std::sort(results.begin(), results.end(), [](const ScannedApInfo& a, const ScannedApInfo& b) {
            return a.rssi > b.rssi;
        });

        if (lvgl_port_lock(200)) {
            ap_list_ = std::move(results);
            scanning_ = false;
            RefreshApList();
            lv_label_set_text_fmt(status_label_, "Found %d networks", (int)ap_list_.size());
            lvgl_port_unlock();
        } else {
            scanning_ = false;
        }
    });
}

void WifiConfigScreen::Show() {
    if (screen_ == nullptr) return;
    if (lvgl_port_lock(200)) {
        main_screen_ = lv_screen_active();
        lv_screen_load(screen_);
        visible_ = true;
        HideConnectModal();
        lvgl_port_unlock();
    }
    StartScan();
}

void WifiConfigScreen::Hide() {
    if (!visible_ || screen_ == nullptr) return;
    if (lvgl_port_lock(200)) {
        visible_ = false;
        HideConnectModal();
        if (main_screen_ != nullptr && main_screen_ != screen_) {
            lv_screen_load(main_screen_);
        }
        lvgl_port_unlock();
    }
}
