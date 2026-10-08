#ifndef EMOTION_EYE_SCREEN_H
#define EMOTION_EYE_SCREEN_H

#include <lvgl.h>
#include <esp_timer.h>
#include <string>
#include "shh_overlay_screen.h"

enum class EyeEmotion {
    Idle,       // Normal open eyes with random blinking
    Listening,  // Wide open, curious, vibrant cyan
    Thinking,   // Moving sideways / pondering, waiting for LLM
    Speaking,   // Happy squint / subtle bounce while talking
    Happy,      // Inverted crescent happy eyes
    Sleepy      // Half-closed eyelids
};

class EmotionEyeScreen : public ShhOverlayScreen {
public:
    EmotionEyeScreen();
    ~EmotionEyeScreen();

    void Initialize(lv_display_t* display = nullptr);
    void Show();
    void Hide();

    void SetEmotion(EyeEmotion emotion);
    void SetEmotionByName(const std::string& name);
    EyeEmotion GetCurrentEmotion() const { return current_emotion_; }

    lv_obj_t* GetScreen() { return eye_screen_; }

private:
    void CreateUI();
    void SetupEyeStyles();
    void ResetEyeGeometry();
    void StartIdleAnimationTimer();
    void StopIdleAnimationTimer();

    static void OnBlinkTimer(void* arg);
    void TriggerBlink();
    void TriggerLookAround();

    lv_obj_t* eye_screen_ = nullptr;

    // Eye & Face objects (Kawaii Face Style)
    lv_obj_t* left_eye_ = nullptr;
    lv_obj_t* right_eye_ = nullptr;
    lv_obj_t* left_blush_ = nullptr;
    lv_obj_t* right_blush_ = nullptr;
    lv_obj_t* mouth_ = nullptr;

    // Default eye dimensions (320x240 display)
    int eye_width_ = 65;
    int eye_height_ = 90;
    int eye_radius_ = 28;
    int eye_spacing_ = 48; // Space between center and eyes

    EyeEmotion current_emotion_ = EyeEmotion::Idle;
    esp_timer_handle_t blink_timer_ = nullptr;
};

#endif // EMOTION_EYE_SCREEN_H
