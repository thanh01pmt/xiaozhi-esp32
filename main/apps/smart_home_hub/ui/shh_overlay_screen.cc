#include "shh_overlay_screen.h"

#include <esp_log.h>
#include <esp_lvgl_port.h>

#define TAG "ShhOverlay"

bool ShhOverlayScreen::AcquireForeground() {
    if (screen_ == nullptr || display_ == nullptr) {
        return false;
    }
    if (lvgl_port_lock(200)) {
        lv_obj_t* cur = lv_screen_active();
        // Never remember ourselves (re-Show) or nothing (display not ready yet)
        // as the restore target; both would strand the next Hide().
        if (cur != nullptr && cur != screen_) {
            main_screen_ = cur;
        }
        lv_screen_load(screen_);
        visible_.store(true);
        lvgl_port_unlock();
        return true;
    }
    ESP_LOGW(TAG, "LVGL busy: overlay screen not shown");
    return false;
}

void ShhOverlayScreen::ReleaseForeground() {
    // Cleared unconditionally: a timed-out lock must not leave a stale visible
    // flag that wedges every later screen switch (camera preview behaviour).
    visible_.store(false);
    if (screen_ == nullptr || display_ == nullptr) {
        return;
    }
    if (lvgl_port_lock(200)) {
        lv_obj_t* target = main_screen_;
        if (target == nullptr || target == screen_) {
            target = lv_display_get_screen_prev(display_);
        }
        if (target != nullptr && target != screen_) {
            lv_screen_load(target);
        }
        lvgl_port_unlock();
    } else {
        ESP_LOGW(TAG, "LVGL busy: overlay screen left loaded");
    }
}
