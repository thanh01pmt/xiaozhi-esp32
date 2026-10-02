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

    // Public so MCP tools can pause/resume stream around Capture() calls
    // to avoid V4L2 single-buffer deadlock on DVP cameras (GC0308).
    void StopLiveStream();
    void StartLiveStream();

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

    void CreateUI();
    void ResetAutoExitTimer();

    static void StreamTask(void* arg);
    static void OnAutoExitTimeout(void* arg);
};

#endif // CAMERA_PREVIEW_SCREEN_H
