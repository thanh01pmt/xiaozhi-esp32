#include "camera_preview_screen.h"
#include "board.h"
#include "camera.h"
#include "application.h"
#include <esp_log.h>
#include <esp_lvgl_port.h>
#include <esp_heap_caps.h>
#include <ctime>
#include <cstdio>
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
    if (!self || !self->IsVisible()) {
        return;
    }
    // Hide() stops the stream and takes the LVGL lock, neither of which is safe
    // on the shared esp_timer task.
    Application::GetInstance().Schedule([self]() {
        if (self->IsVisible()) {
            ESP_LOGI(TAG, "Camera preview timeout -> returning to main screen");
            self->Hide();
        }
    });
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

    if (lvgl_port_lock(200)) {
        CreateUI();
        lvgl_port_unlock();
    } else {
        ESP_LOGE(TAG, "LVGL busy: camera preview UI not created");
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

    // Bottom Control Bar - also carries the capture timestamp once frozen
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

    // Click on screen triggers a still capture, shown full screen with a timestamp
    lv_obj_add_event_cb(
        screen_,
        [](lv_event_t* e) {
            auto self = static_cast<CameraPreviewScreen*>(lv_event_get_user_data(e));
            if (!self) {
                return;
            }
            bool expected = false;
            if (!self->capture_in_progress_.compare_exchange_strong(expected, true)) {
                return; // capture already running
            }
            self->ResetAutoExitTimer();
            // Capture() and the LVGL lock both block, so leave the LVGL task.
            Application::GetInstance().Schedule([self]() {
                self->FreezeCapturedPhoto();
                self->capture_in_progress_ = false;
            });
        },
        LV_EVENT_CLICKED, this);
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

    if (lvgl_port_lock(200)) {
        main_screen_ = lv_screen_active();
        if (main_screen_ == screen_) {
            main_screen_ = lv_display_get_screen_prev(display_);
        }
        lv_screen_load(screen_);
        visible_ = true;
        lvgl_port_unlock();
    } else {
        ESP_LOGW(TAG, "LVGL busy, camera preview not shown");
        return;
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

    if (lvgl_port_lock(200)) {
        lv_obj_t* target = (main_screen_ != nullptr && main_screen_ != screen_)
                               ? main_screen_
                               : lv_display_get_screen_prev(display_);
        if (target != nullptr && target != screen_) {
            lv_screen_load(target);
        }
        lvgl_port_unlock();
    } else {
        ESP_LOGW(TAG, "LVGL busy, camera screen left loaded");
    }
    // Never leave a stale visible flag behind: a timed-out lock must not wedge
    // every later screen switch.
    visible_ = false;
}

void CameraPreviewScreen::StartLiveStream() {
    if (!visible_ || task_running_ || preview_task_handle_ != nullptr) return;
    task_running_ = true;
    if (xTaskCreatePinnedToCore(&CameraPreviewScreen::StreamTask, "cam_preview_task", 4096, this, 5,
                                &preview_task_handle_, 1) != pdPASS) {
        task_running_ = false;
        preview_task_handle_ = nullptr;
        ESP_LOGE(TAG, "Failed to start camera preview task");
    }
}

void CameraPreviewScreen::StopLiveStream() {
    task_running_ = false;
    if (preview_task_handle_ == nullptr) return;

    for (int i = 0; i < 100 && preview_task_handle_ != nullptr; i++) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (preview_task_handle_ != nullptr) {
        ESP_LOGW(TAG, "Preview task stuck, deleting it");
        vTaskDelete(preview_task_handle_);
        preview_task_handle_ = nullptr;
    }
}

void CameraPreviewScreen::FreezeCapturedPhoto() {
    if (!visible_ || preview_rgb_buffer_ == nullptr) return;
    TakeStillFrame("PHOTO | GC0308");
}

void CameraPreviewScreen::OnExternalPhotoCaptured() {
    if (!visible_ || preview_rgb_buffer_ == nullptr) return;
    StopLiveStream();
    TakeStillFrame("PHOTO | GC0308");
}

void CameraPreviewScreen::TakeStillFrame(const char* caption_prefix) {
    StopLiveStream();
    // The still frame is written straight into the preview buffer, so the old
    // fingerprint no longer describes what is on screen.
    has_frame_signature_ = false;

    char stamp[64] = {0};
    time_t now = time(nullptr);
    struct tm tmv;
    localtime_r(&now, &tmv);

    uint16_t w = 0, h = 0;
    auto camera = Board::GetInstance().GetCamera();
    bool grabbed = camera != nullptr &&
                   camera->CapturePreviewFrame(preview_rgb_buffer_, FRAME_BUFFER_SIZE, w, h);

    if (lvgl_port_lock(200)) {
        if (status_label_ != nullptr) {
            if (grabbed) {
                lv_label_set_text_fmt(status_label_, "%s", caption_prefix);
                lv_obj_set_style_text_color(status_label_, lv_color_hex(0xFFB300), 0);
            } else {
                lv_label_set_text(status_label_, "CAMERA UNAVAILABLE");
                lv_obj_set_style_text_color(status_label_, lv_color_hex(0xFF5252), 0);
            }
        }
        if (hint_label_ != nullptr) {
            if (grabbed) {
                snprintf(stamp, sizeof(stamp), "Chụp lúc %02d/%02d/%04d  %02d:%02d:%02d", tmv.tm_mday,
                         tmv.tm_mon + 1, tmv.tm_year + 1900, tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
                lv_label_set_text(hint_label_, stamp);
                lv_obj_set_style_text_color(hint_label_, lv_color_hex(0xFFB300), 0);
            } else {
                lv_label_set_text(hint_label_, "Không lấy được khung hình từ camera");
                lv_obj_set_style_text_color(hint_label_, lv_color_hex(0xFF5252), 0);
            }
        }
        if (img_obj_ != nullptr) {
            lv_obj_invalidate(img_obj_);
        }
        lvgl_port_unlock();
    } else {
        ESP_LOGW(TAG, "LVGL busy, could not refresh the captured photo");
    }

    // Stay on the frozen frame; the live stream only resumes after Hide()/Show().
    ResetAutoExitTimer();
}

uint32_t CameraPreviewScreen::Fingerprint(const uint8_t* frame) {
    // Sample ~256 pixels spread across the frame instead of hashing all
    // 153.6 KB. Cheap enough to run every frame, and a real scene change
    // always moves at least a few of these.
    uint32_t hash = 2166136261u;
    constexpr size_t kStride = 600;
    for (size_t offset = 0; offset < FRAME_BUFFER_SIZE; offset += kStride) {
        hash ^= frame[offset];
        hash *= 16777619u;
    }
    return hash;
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
                const uint32_t signature = Fingerprint(self->preview_rgb_buffer_);
                const bool changed = !self->has_frame_signature_ || signature != self->last_frame_signature_;
                self->last_frame_signature_ = signature;
                self->has_frame_signature_ = true;
                if (!changed) {
                    // Same picture: skip the invalidate and keep the link free.
                    vTaskDelay(pdMS_TO_TICKS(66));
                    continue;
                }

                frame_count++;
                int64_t now = esp_timer_get_time();
                if (now - last_fps_time >= 2000000LL) { // Every 2s update FPS
                    float fps = (frame_count * 1000000.0f) / (now - last_fps_time);
                    frame_count = 0;
                    last_fps_time = now;
                    if (lvgl_port_lock(50)) {
                        if (self->status_label_ != nullptr) {
                            lv_label_set_text_fmt(self->status_label_, "LIVE PREVIEW | %.1f FPS", fps);
                            lv_obj_set_style_text_color(self->status_label_, lv_color_hex(0x00E5FF), 0);
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
