#include "emotion_eye_screen.h"
#include "application.h"
#include <esp_log.h>
#include <esp_lvgl_port.h>
#include <esp_random.h>

#define TAG "SH_EmotionEyes"

EmotionEyeScreen::EmotionEyeScreen() = default;

EmotionEyeScreen::~EmotionEyeScreen() {
    StopIdleAnimationTimer();
}

void EmotionEyeScreen::OnBlinkTimer(void* arg) {
    auto self = static_cast<EmotionEyeScreen*>(arg);
    if (!self || !self->is_visible_) return;

    Application::GetInstance().Schedule([self]() {
        if (!self->is_visible_) return;
        if (self->current_emotion_ == EyeEmotion::Idle) {
            uint32_t r = esp_random() % 100;
            if (r < 65) {
                self->TriggerBlink();
            } else {
                self->TriggerLookAround();
            }
        }
    });
}

void EmotionEyeScreen::StartIdleAnimationTimer() {
    if (blink_timer_ == nullptr) {
        esp_timer_create_args_t timer_args = {
            .callback = &EmotionEyeScreen::OnBlinkTimer,
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "eye_blink_timer",
            .skip_unhandled_events = true,
        };
        esp_timer_create(&timer_args, &blink_timer_);
    }
    // Random blink every 3 to 5 seconds
    uint32_t delay_ms = 3000 + (esp_random() % 2500);
    esp_timer_stop(blink_timer_);
    esp_timer_start_periodic(blink_timer_, delay_ms * 1000);
}

void EmotionEyeScreen::StopIdleAnimationTimer() {
    if (blink_timer_ != nullptr) {
        esp_timer_stop(blink_timer_);
        esp_timer_delete(blink_timer_);
        blink_timer_ = nullptr;
    }
}

void EmotionEyeScreen::Initialize(lv_display_t* display) {
    display_ = display;
    if (lvgl_port_lock(100)) {
        CreateUI();
        lvgl_port_unlock();
    }
}

void EmotionEyeScreen::CreateUI() {
    eye_screen_ = lv_obj_create(NULL);
    // Deep OLED/IPS Black Background
    lv_obj_set_style_bg_color(eye_screen_, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(eye_screen_, LV_OPA_COVER, 0);
    lv_obj_remove_flag(eye_screen_, LV_OBJ_FLAG_SCROLLABLE);

    // Left Eye
    left_eye_ = lv_obj_create(eye_screen_);
    lv_obj_remove_style_all(left_eye_);
    lv_obj_set_size(left_eye_, eye_width_, eye_height_);
    lv_obj_align(left_eye_, LV_ALIGN_CENTER, -eye_spacing_ - (eye_width_ / 2), -15);
    lv_obj_set_style_bg_color(left_eye_, lv_color_hex(0x00E5FF), 0); // Vibrant Cyan/Neon
    lv_obj_set_style_bg_opa(left_eye_, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(left_eye_, eye_radius_, 0);

    // Right Eye
    right_eye_ = lv_obj_create(eye_screen_);
    lv_obj_remove_style_all(right_eye_);
    lv_obj_set_size(right_eye_, eye_width_, eye_height_);
    lv_obj_align(right_eye_, LV_ALIGN_CENTER, eye_spacing_ + (eye_width_ / 2), -15);
    lv_obj_set_style_bg_color(right_eye_, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_bg_opa(right_eye_, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(right_eye_, eye_radius_, 0);

    // Left Blush (Kawaii pink cheek)
    left_blush_ = lv_obj_create(eye_screen_);
    lv_obj_remove_style_all(left_blush_);
    lv_obj_set_size(left_blush_, 26, 12);
    lv_obj_align(left_blush_, LV_ALIGN_CENTER, -eye_spacing_ - (eye_width_ / 2) - 18, 45);
    lv_obj_set_style_bg_color(left_blush_, lv_color_hex(0xFF4081), 0);
    lv_obj_set_style_bg_opa(left_blush_, LV_OPA_60, 0);
    lv_obj_set_style_radius(left_blush_, 6, 0);

    // Right Blush (Kawaii pink cheek)
    right_blush_ = lv_obj_create(eye_screen_);
    lv_obj_remove_style_all(right_blush_);
    lv_obj_set_size(right_blush_, 26, 12);
    lv_obj_align(right_blush_, LV_ALIGN_CENTER, eye_spacing_ + (eye_width_ / 2) + 18, 45);
    lv_obj_set_style_bg_color(right_blush_, lv_color_hex(0xFF4081), 0);
    lv_obj_set_style_bg_opa(right_blush_, LV_OPA_60, 0);
    lv_obj_set_style_radius(right_blush_, 6, 0);

    // Cute animated mouth
    mouth_ = lv_obj_create(eye_screen_);
    lv_obj_remove_style_all(mouth_);
    lv_obj_set_size(mouth_, 20, 10);
    lv_obj_align(mouth_, LV_ALIGN_CENTER, 0, 48);
    lv_obj_set_style_bg_color(mouth_, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_bg_opa(mouth_, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(mouth_, 5, 0);

    // Touch on eyes toggles back to chat or wakes device
    lv_obj_add_event_cb(eye_screen_, [](lv_event_t* e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
            ESP_LOGI(TAG, "Touch on eye screen -> toggle chat state");
            Application::GetInstance().Schedule([]() {
                Application::GetInstance().ToggleChatState();
            });
        }
    }, LV_EVENT_CLICKED, nullptr);
}

void EmotionEyeScreen::ResetEyeGeometry() {
    if (!left_eye_ || !right_eye_) return;
    lv_anim_delete(left_eye_, nullptr);
    lv_anim_delete(right_eye_, nullptr);
    if (mouth_) lv_anim_delete(mouth_, nullptr);

    lv_obj_set_size(left_eye_, eye_width_, eye_height_);
    lv_obj_set_size(right_eye_, eye_width_, eye_height_);
    lv_obj_align(left_eye_, LV_ALIGN_CENTER, -eye_spacing_ - (eye_width_ / 2), -15);
    lv_obj_align(right_eye_, LV_ALIGN_CENTER, eye_spacing_ + (eye_width_ / 2), -15);
    lv_obj_set_style_radius(left_eye_, eye_radius_, 0);
    lv_obj_set_style_radius(right_eye_, eye_radius_, 0);
    lv_obj_set_style_bg_color(left_eye_, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_bg_color(right_eye_, lv_color_hex(0x00E5FF), 0);

    if (left_blush_) lv_obj_set_style_bg_opa(left_blush_, LV_OPA_60, 0);
    if (right_blush_) lv_obj_set_style_bg_opa(right_blush_, LV_OPA_60, 0);

    if (mouth_) {
        lv_obj_set_size(mouth_, 20, 10);
        lv_obj_align(mouth_, LV_ALIGN_CENTER, 0, 48);
        lv_obj_set_style_bg_color(mouth_, lv_color_hex(0x00E5FF), 0);
    }
}

void EmotionEyeScreen::TriggerBlink() {
    if (!left_eye_ || !right_eye_) return;
    if (!lvgl_port_lock(100)) return;

    // Squeeze height from eye_height_ to 4 and back
    lv_anim_t a_left;
    lv_anim_init(&a_left);
    lv_anim_set_var(&a_left, left_eye_);
    lv_anim_set_values(&a_left, eye_height_, 4);
    lv_anim_set_duration(&a_left, 120);
    lv_anim_set_reverse_duration(&a_left, 120);
    lv_anim_set_exec_cb(&a_left, [](void* var, int32_t val) {
        lv_obj_set_height(static_cast<lv_obj_t*>(var), val);
    });
    lv_anim_set_path_cb(&a_left, lv_anim_path_ease_in_out);
    lv_anim_start(&a_left);

    lv_anim_t a_right = a_left;
    lv_anim_set_var(&a_right, right_eye_);
    lv_anim_start(&a_right);

    lvgl_port_unlock();
}

void EmotionEyeScreen::TriggerLookAround() {
    if (!left_eye_ || !right_eye_) return;
    if (!lvgl_port_lock(100)) return;

    int offset_x = (esp_random() % 2 == 0) ? 22 : -22;

    int left_base_x = -eye_spacing_ - (eye_width_ / 2);
    int right_base_x = eye_spacing_ + (eye_width_ / 2);

    lv_anim_t a_left;
    lv_anim_init(&a_left);
    lv_anim_set_var(&a_left, left_eye_);
    lv_anim_set_values(&a_left, left_base_x, left_base_x + offset_x);
    lv_anim_set_duration(&a_left, 300);
    lv_anim_set_playback_duration(&a_left, 300);
    lv_anim_set_playback_delay(&a_left, 600);
    lv_anim_set_exec_cb(&a_left, [](void* var, int32_t val) {
        lv_obj_set_x(static_cast<lv_obj_t*>(var), val);
    });
    lv_anim_set_path_cb(&a_left, lv_anim_path_ease_in_out);
    lv_anim_start(&a_left);

    lv_anim_t a_right = a_left;
    lv_anim_set_var(&a_right, right_eye_);
    lv_anim_set_values(&a_right, right_base_x, right_base_x + offset_x);
    lv_anim_start(&a_right);

    lvgl_port_unlock();
}

void EmotionEyeScreen::SetEmotion(EyeEmotion emotion) {
    current_emotion_ = emotion;
    if (!left_eye_ || !right_eye_ || !is_visible_) return;
    if (!lvgl_port_lock(150)) return;

    ResetEyeGeometry();

    switch (emotion) {
        case EyeEmotion::Idle:
            // Default cyan rounded rectangle
            lv_obj_set_style_bg_color(left_eye_, lv_color_hex(0x00E5FF), 0);
            lv_obj_set_style_bg_color(right_eye_, lv_color_hex(0x00E5FF), 0);
            break;

        case EyeEmotion::Listening:
            // Big rounded eager eyes, brighter electric cyan
            lv_obj_set_size(left_eye_, 85, 115);
            lv_obj_set_size(right_eye_, 85, 115);
            lv_obj_set_style_radius(left_eye_, 38, 0);
            lv_obj_set_style_radius(right_eye_, 38, 0);
            lv_obj_set_style_bg_color(left_eye_, lv_color_hex(0x00FFAA), 0);
            lv_obj_set_style_bg_color(right_eye_, lv_color_hex(0x00FFAA), 0);
            break;

        case EyeEmotion::Thinking: {
            // Amber/Orange eyes shifting left and right in thought
            lv_obj_set_style_bg_color(left_eye_, lv_color_hex(0xFFB300), 0);
            lv_obj_set_style_bg_color(right_eye_, lv_color_hex(0xFFB300), 0);
            lv_obj_set_size(left_eye_, 75, 75);
            lv_obj_set_size(right_eye_, 75, 75);
            lv_obj_set_style_radius(left_eye_, 25, 0);
            lv_obj_set_style_radius(right_eye_, 25, 0);

            // Repeating wave animation while thinking
            int left_base_x = -eye_spacing_ - (75 / 2);
            int right_base_x = eye_spacing_ + (75 / 2);

            lv_anim_t anim_left;
            lv_anim_init(&anim_left);
            lv_anim_set_var(&anim_left, left_eye_);
            lv_anim_set_values(&anim_left, left_base_x - 18, left_base_x + 18);
            lv_anim_set_duration(&anim_left, 450);
            lv_anim_set_playback_duration(&anim_left, 450);
            lv_anim_set_repeat_count(&anim_left, LV_ANIM_REPEAT_INFINITE);
            lv_anim_set_path_cb(&anim_left, lv_anim_path_ease_in_out);
            lv_anim_set_exec_cb(&anim_left, [](void* var, int32_t val) {
                lv_obj_set_x(static_cast<lv_obj_t*>(var), val);
            });
            lv_anim_start(&anim_left);

            lv_anim_t anim_right = anim_left;
            lv_anim_set_var(&anim_right, right_eye_);
            lv_anim_set_values(&anim_right, right_base_x - 18, right_base_x + 18);
            lv_anim_start(&anim_right);
            break;
        }

        case EyeEmotion::Speaking: {
            // Animated happy talk mouth and blush
            lv_obj_set_style_bg_color(left_eye_, lv_color_hex(0x00FF88), 0);
            lv_obj_set_style_bg_color(right_eye_, lv_color_hex(0x00FF88), 0);
            lv_obj_set_size(left_eye_, 75, 55);
            lv_obj_set_size(right_eye_, 75, 55);
            lv_obj_set_style_radius(left_eye_, 25, 0);
            lv_obj_set_style_radius(right_eye_, 25, 0);

            if (left_blush_) lv_obj_set_style_bg_opa(left_blush_, LV_OPA_90, 0);
            if (right_blush_) lv_obj_set_style_bg_opa(right_blush_, LV_OPA_90, 0);

            if (mouth_) {
                lv_obj_set_style_bg_color(mouth_, lv_color_hex(0xFF4081), 0);
                lv_anim_t a_mouth;
                lv_anim_init(&a_mouth);
                lv_anim_set_var(&a_mouth, mouth_);
                lv_anim_set_values(&a_mouth, 10, 24);
                lv_anim_set_duration(&a_mouth, 220);
                lv_anim_set_playback_duration(&a_mouth, 220);
                lv_anim_set_repeat_count(&a_mouth, LV_ANIM_REPEAT_INFINITE);
                lv_anim_set_exec_cb(&a_mouth, [](void* var, int32_t val) {
                    lv_obj_set_height(static_cast<lv_obj_t*>(var), val);
                });
                lv_anim_start(&a_mouth);
            }
            break;
        }

        case EyeEmotion::Happy: {
            // Happy squint eyes / crescent curve & high blush
            lv_obj_set_style_bg_color(left_eye_, lv_color_hex(0x00FF88), 0);
            lv_obj_set_style_bg_color(right_eye_, lv_color_hex(0x00FF88), 0);
            lv_obj_set_size(left_eye_, 80, 45);
            lv_obj_set_size(right_eye_, 80, 45);
            lv_obj_set_style_radius(left_eye_, 22, 0);
            lv_obj_set_style_radius(right_eye_, 22, 0);

            if (left_blush_) lv_obj_set_style_bg_opa(left_blush_, LV_OPA_100, 0);
            if (right_blush_) lv_obj_set_style_bg_opa(right_blush_, LV_OPA_100, 0);

            if (mouth_) {
                lv_obj_set_size(mouth_, 28, 14);
                lv_obj_set_style_bg_color(mouth_, lv_color_hex(0xFF4081), 0);
            }
            break;
        }

        case EyeEmotion::Sleepy:
            // Low height, dimmed eyes
            lv_obj_set_style_bg_color(left_eye_, lv_color_hex(0x37474F), 0);
            lv_obj_set_style_bg_color(right_eye_, lv_color_hex(0x37474F), 0);
            lv_obj_set_size(left_eye_, 65, 18);
            lv_obj_set_size(right_eye_, 65, 18);
            if (left_blush_) lv_obj_set_style_bg_opa(left_blush_, LV_OPA_20, 0);
            if (right_blush_) lv_obj_set_style_bg_opa(right_blush_, LV_OPA_20, 0);
            break;
    }

    lvgl_port_unlock();
}

void EmotionEyeScreen::SetEmotionByName(const std::string& name) {
    if (name == "listening") {
        SetEmotion(EyeEmotion::Listening);
    } else if (name == "thinking" || name == "waiting") {
        SetEmotion(EyeEmotion::Thinking);
    } else if (name == "speaking") {
        SetEmotion(EyeEmotion::Speaking);
    } else if (name == "happy") {
        SetEmotion(EyeEmotion::Happy);
    } else if (name == "sleepy") {
        SetEmotion(EyeEmotion::Sleepy);
    } else {
        SetEmotion(EyeEmotion::Idle);
    }
}

void EmotionEyeScreen::Show() {
    if (eye_screen_ == nullptr) return;
    if (lvgl_port_lock(200)) {
        main_screen_ = lv_screen_active();
        lv_screen_load(eye_screen_);
        is_visible_ = true;
        ResetEyeGeometry();
        lvgl_port_unlock();
        StartIdleAnimationTimer();
    }
}

void EmotionEyeScreen::Hide() {
    if (!is_visible_) return;
    StopIdleAnimationTimer();
    if (lvgl_port_lock(200)) {
        ResetEyeGeometry();
        if (main_screen_ != nullptr) {
            lv_screen_load(main_screen_);
        }
        lvgl_port_unlock();
    }
    is_visible_ = false;
}
