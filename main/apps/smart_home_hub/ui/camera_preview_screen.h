#ifndef CAMERA_PREVIEW_SCREEN_H
#define CAMERA_PREVIEW_SCREEN_H

#include <lvgl.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <atomic>
#include <string>

class CameraPreviewScreen {
public:
    CameraPreviewScreen();
    ~CameraPreviewScreen();

    void Initialize(lv_display_t* display);
    void Show();
    void Hide();
    bool IsVisible() const { return visible_; }

    // Freeze the live feed into a still frame and caption it with the capture
    // timestamp. Safe to call when the screen is not visible (no-op).
    void FreezeCapturedPhoto();

    // Called by Board::OnPhotoCaptured() when a still was captured elsewhere
    // (e.g. the self.camera.take_photo MCP tool) while this screen is open.
    void OnExternalPhotoCaptured();

private:
    lv_display_t* display_ = nullptr;
    lv_obj_t* screen_ = nullptr;
    lv_obj_t* img_obj_ = nullptr;
    lv_obj_t* status_label_ = nullptr;
    lv_obj_t* hint_label_ = nullptr;

    lv_img_dsc_t img_dsc_{};
    uint8_t* preview_rgb_buffer_ = nullptr; // 320x240 RGB565 (153.6 KB) in PSRAM

    std::atomic<bool> visible_{false};
    std::atomic<bool> task_running_{false};
    std::atomic<bool> capture_in_progress_{false};
    TaskHandle_t preview_task_handle_ = nullptr;
    esp_timer_handle_t auto_exit_timer_ = nullptr;
    // Screen that was active before this one; LVGL's screen list order is not
    // display order, so it must be remembered explicitly to get back to XiaoZhi.
    lv_obj_t* main_screen_ = nullptr;

    void CreateUI();
    void ResetAutoExitTimer();
    void StopLiveStream();
    void StartLiveStream();
    void TakeStillFrame(const char* caption_prefix);

    static void StreamTask(void* arg);
    static void OnAutoExitTimeout(void* arg);
};

#endif // CAMERA_PREVIEW_SCREEN_H
