#ifndef WIFI_CONFIG_SCREEN_H
#define WIFI_CONFIG_SCREEN_H

#include <lvgl.h>
#include <esp_wifi.h>
#include <esp_timer.h>
#include <vector>
#include <string>
#include "shh_overlay_screen.h"

struct ScannedApInfo {
    std::string ssid;
    int rssi = -100;
    uint8_t authmode = 0;
    bool is_saved = false;
};

class WifiConfigScreen : public ShhOverlayScreen {
public:
    WifiConfigScreen();
    ~WifiConfigScreen();

    void Initialize(lv_display_t* display);
    void Show();
    void Hide();

    void StartScan();

private:
    void CreateUI();
    void RefreshApList();
    void ShowConnectModal(const std::string& ssid);
    void HideConnectModal();
    void ConnectToAp(const std::string& ssid, const std::string& password);

    lv_display_t* display_ = nullptr;
    lv_obj_t* screen_ = nullptr;

    // Header
    lv_obj_t* title_label_ = nullptr;
    lv_obj_t* status_label_ = nullptr;
    lv_obj_t* scan_btn_ = nullptr;
    lv_obj_t* close_btn_ = nullptr;
    lv_obj_t* portal_btn_ = nullptr;

    // Body
    lv_obj_t* list_container_ = nullptr;

    // Connect modal
    lv_obj_t* modal_obj_ = nullptr;
    lv_obj_t* modal_ssid_label_ = nullptr;
    lv_obj_t* pwd_textarea_ = nullptr;
    lv_obj_t* keyboard_ = nullptr;
    std::string target_ssid_;

    void ResetAutoReturnTimer();
    static void OnAutoReturnTimeout(void* arg);

    esp_timer_handle_t auto_return_timer_ = nullptr;
    std::vector<ScannedApInfo> ap_list_;
    bool scanning_ = false;
};

#endif // WIFI_CONFIG_SCREEN_H
