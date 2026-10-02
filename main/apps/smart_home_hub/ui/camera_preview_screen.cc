#include "camera_preview_screen.h"
#include "board.h"
#include "camera.h"
#include "application.h"
#include <esp_log.h>
#include <esp_lvgl_port.h>
#include <esp_heap_caps.h>
#include <cstring>

#define TAG "CameraPreview"
#define PREVIEW_WIDTH 320
#define PREVIEW_HEIGHT 240
#define FRAME_BUFFER_SIZE (PREVIEW_WIDTH * PREVIEW_HEIGHT * 2)

CameraPreviewScreen::CameraPreviewScreen() = default;

CameraPreviewScreen::~CameraPreviewScreen() {
    Hide();
    if (auto_exit_timer_ != nullptr) {
        esp_timer_stop(auto_exit_timer_);
        esp_timer_delete(auto_exit_timer_);
    }
    if (preview_rgb_buffer_ != nullptr) {
        heap_caps_free(preview_rgb_buffer_);
        preview_rgb_buffer_ = nullptr;
    }
}

void CameraPreviewScreen::OnAutoExitTimeout(void* arg) {
    auto self = static_cast<CameraPreviewScreen*>(arg);
    if (self && self->IsVisible()) {
        ESP_LOGI(TAG, "Camera preview timeout -> returning to main screen");
        self->Hide();
    }
}

void CameraPreviewScreen::Initialize(lv_display_t* display) {
    display_ = display;

    // Allocate 1 frame buffer in PSRAM (153.6 KB)
    preview_rgb_buffer_ = static_cast<uint8_t*>(
        heap_caps_malloc(FRAME_BUFFER_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (preview_rgb_buffer_ != nullptr) {
        memset(preview_rgb_buffer_, 0, FRAME_BUFFER_SIZE);
    } else {
        ESP_LOGE(TAG, "Failed to allocate preview RGB buffer in PSRAM");
    }

    // Configure image descriptor for LVGL
    bzero(&img_dsc_, sizeof(img_dsc_));
    img_dsc_.header.magic = LV_IMAGE_HEADER_MAGIC;
    img_dsc_.header.cf = LV_COLOR_FORMAT_RGB565;
    img_dsc_.header.w = PREVIEW_WIDTH;
    img_dsc_.header.h = PREVIEW_HEIGHT;
    img_dsc_.header.stride = PREVIEW_WIDTH * 2;
    img_dsc_.data_size = FRAME_BUFFER_SIZE;
    img_dsc_.data = preview_rgb_buffer_;

    esp_timer_create_args_t timer_args = {
        .callback = &CameraPreviewScreen::OnAutoExitTimeout,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "cam_auto_exit",
        .skip_unhandled_events = true,
    };
    esp_timer_create(&timer_args, &auto_exit_timer_);

    if (lvgl_port_lock(100)) {
        CreateUI();
        lvgl_port_unlock();
    }
}

void CameraPreviewScreen::CreateUI() {
    screen_ = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_, lv_color_hex(0x000000), 0);
    lv_obj_clear_flag(screen_, LV_OBJ_FLAG_SCROLLABLE);

    // Live Video Canvas / Image Widget (full screen 320x240)
    img_obj_ = lv_image_create(screen_);
    lv_obj_set_size(img_obj_, PREVIEW_WIDTH, PREVIEW_HEIGHT);
    lv_obj_center(img_obj_);
    if (preview_rgb_buffer_ != nullptr) {
        lv_image_set_src(img_obj_, &img_dsc_);
    }

    // Header Bar
    lv_obj_t* top_bar = lv_obj_create(screen_);
    lv_obj_set_size(top_bar, 320, 28);
    lv_obj_align(top_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(top_bar, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(top_bar, LV_OPA_60, 0);
    lv_obj_set_style_border_width(top_bar, 0, 0);
    lv_obj_set_style_radius(top_bar, 0, 0);
    lv_obj_set_style_pad_all(top_bar, 2, 0);

    status_label_ = lv_label_create(top_bar);
    lv_label_set_text(status_label_, "LIVE PREVIEW | GC0308");
    lv_obj_set_style_text_color(status_label_, lv_color_hex(0x00E5FF), 0);
    lv_obj_align(status_label_, LV_ALIGN_LEFT_MID, 8, 0);

    // Bottom Control Bar
    lv_obj_t* bottom_bar = lv_obj_create(screen_);
    lv_obj_set_size(bottom_bar, 320, 36);
    lv_obj_align(bottom_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(bottom_bar, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(bottom_bar, LV_OPA_70, 0);
    lv_obj_set_style_border_width(bottom_bar, 0, 0);
    lv_obj_set_style_radius(bottom_bar, 0, 0);
    lv_obj_set_style_pad_all(bottom_bar, 2, 0);

    hint_label_ = lv_label_create(bottom_bar);
    lv_label_set_text(hint_label_, "Chạm để Chụp | Nói: 'Chụp hình'");
    lv_obj_set_style_text_color(hint_label_, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(hint_label_, LV_ALIGN_CENTER, 0, 0);

    // Click on screen triggers Capture & AI explain
    lv_obj_add_event_cb(screen_, [](lv_event_t* e) {
        auto self = static_cast<CameraPreviewScreen*>(lv_event_get_user_data(e));
        if (self && !self->capture_in_progress_) {
            ESP_LOGI(TAG, "Screen clicked -> scheduling capture (stop stream first)");
            self->ResetAutoExitTimer();
            // Must schedule off the LVGL task to avoid deadlock:
            // StreamTask holds V4L2 buffer + needs lvgl_port_lock,
            // Capture() needs V4L2 buffer + we're inside LVGL lock here.
            self->capture_in_progress_ = true;
            Application::GetInstance().Schedule([self]() {
                self->StopLiveStream();
                auto camera = Board::GetInstance().GetCamera();
                if (camera != nullptr) {
                    camera->Capture();
                }
                // Restart live stream after capture completes
                if (self->visible_) {
                    self->StartLiveStream();
                }
                self->capture_in_progress_ = false;
            });
        }
    }, LV_EVENT_CLICKED, this);
}

void CameraPreviewScreen::ResetAutoExitTimer() {
    if (auto_exit_timer_ != nullptr) {
        esp_timer_stop(auto_exit_timer_);
        esp_timer_start_once(auto_exit_timer_, 30 * 1000 * 1000); // 30s auto return
    }
}

void CameraPreviewScreen::Show() {
    if (screen_ == nullptr) return;
    ESP_LOGI(TAG, "Show Camera Live Preview");

    if (lvgl_port_lock(100)) {
        lv_screen_load(screen_);
        visible_ = true;
        lvgl_port_unlock();
    }
    ResetAutoExitTimer();
    StartLiveStream();
}

void CameraPreviewScreen::Hide() {
    if (!visible_) return;
    ESP_LOGI(TAG, "Hide Camera Live Preview");

    StopLiveStream();
    if (auto_exit_timer_ != nullptr) {
        esp_timer_stop(auto_exit_timer_);
    }

    if (lvgl_port_lock(100)) {
        auto main_screen = lv_display_get_screen_active(display_);
        if (main_screen != nullptr && main_screen != screen_) {
            lv_screen_load(main_screen);
        } else {
            lv_screen_load(lv_display_get_screen_prev(display_));
        }
        visible_ = false;
        lvgl_port_unlock();
    }
}

void CameraPreviewScreen::StartLiveStream() {
    if (task_running_) return;
    task_running_ = true;

    xTaskCreatePinnedToCore(&CameraPreviewScreen::StreamTask,
                            "cam_preview_task",
                            4096,
                            this,
                            5, // Medium priority
                            &preview_task_handle_,
                            1); // Run on core 1
}

void CameraPreviewScreen::StopLiveStream() {
    if (!task_running_) return;
    task_running_ = false;

    if (preview_task_handle_ != nullptr) {
        // Wait up to 300ms for clean thread exit
        int count = 0;
        while (preview_task_handle_ != nullptr && count++ < 30) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        preview_task_handle_ = nullptr;
    }
}

void CameraPreviewScreen::StreamTask(void* arg) {
    auto self = static_cast<CameraPreviewScreen*>(arg);
    auto camera = Board::GetInstance().GetCamera();

    ESP_LOGI(TAG, "Camera live preview stream task started");

    uint32_t frame_count = 0;
    int64_t last_fps_time = esp_timer_get_time();

    while (self->task_running_ && self->visible_) {
        if (camera != nullptr && self->preview_rgb_buffer_ != nullptr) {
            uint16_t w = 0, h = 0;
            if (camera->CapturePreviewFrame(self->preview_rgb_buffer_, FRAME_BUFFER_SIZE, w, h)) {
                frame_count++;
                int64_t now = esp_timer_get_time();
                if (now - last_fps_time >= 2000000LL) { // Every 2s update FPS
                    float fps = (frame_count * 1000000.0f) / (now - last_fps_time);
                    frame_count = 0;
                    last_fps_time = now;
                    if (lvgl_port_lock(50)) {
                        if (self->status_label_ != nullptr) {
                            lv_label_set_text_fmt(self->status_label_, "LIVE PREVIEW | %.1f FPS", fps);
                        }
                        lvgl_port_unlock();
                    }
                }

                // Invalidate LVGL image object so it blits to screen smoothly
                if (lvgl_port_lock(50)) {
                    if (self->img_obj_ != nullptr) {
                        lv_obj_invalidate(self->img_obj_);
                    }
                    lvgl_port_unlock();
                }
            }
        }

        // Throttle ~15 FPS to keep PSRAM bandwidth low and audio streaming crystal clear
        vTaskDelay(pdMS_TO_TICKS(66));
    }

    ESP_LOGI(TAG, "Camera live preview stream task stopped");
    self->preview_task_handle_ = nullptr;
    vTaskDelete(NULL);
}
