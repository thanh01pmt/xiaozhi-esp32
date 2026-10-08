#pragma once

#include <lvgl.h>
#include <atomic>

// Shared screen lifecycle for Smart Home Hub overlays.
//
// Every overlay owns exactly one full-screen object created with
// lv_obj_create(NULL) and swaps it in with lv_screen_load(). Showing it
// remembers the previously active screen so Hide() can restore it; LVGL's
// screen list order is not display order, so that pointer must be remembered
// explicitly. Before this base class every screen managed main_screen_ itself
// and the copies drifted: only the camera preview had the
// lv_display_get_screen_prev() fallback, only the emotion eyes guarded against
// saving themselves as their own restore target, and only the camera preview
// cleared its visible flag when the LVGL lock timed out.
class ShhOverlayScreen {
public:
    // Safe to read from other tasks (the camera stream task polls it).
    bool IsVisible() const { return visible_.load(); }

protected:
    // Call once from CreateUI() after the root screen has been created.
    // display_ must already be set by Initialize().
    void SetScreen(lv_obj_t* screen) { screen_ = screen; }

    // Takes the LVGL lock, remembers the active screen as the restore target
    // (unless it is this screen already or there is none), loads this screen
    // and marks it visible. Returns false when the lock timed out; the caller
    // must treat the screen as not shown in that case.
    bool AcquireForeground();

    // Takes the LVGL lock and restores the remembered screen, falling back to
    // the display's previous screen when the restore target is missing.
    // Always clears the visible flag, even when the lock timed out: a stale
    // flag must never wedge every later screen switch.
    void ReleaseForeground();

    lv_display_t* display_ = nullptr;
    lv_obj_t* screen_ = nullptr;

private:
    lv_obj_t* main_screen_ = nullptr;
    std::atomic<bool> visible_{false};
};
