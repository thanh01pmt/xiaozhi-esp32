#ifndef SENSOR_CARD_SCREEN_H
#define SENSOR_CARD_SCREEN_H

#include <lvgl.h>
#include <esp_timer.h>
#include <string>

enum class SensorCardType {
    Temperature,
    Battery,
    Light,
    Motion,
    Network,
    System
};

class SensorCardScreen {
public:
    SensorCardScreen();
    ~SensorCardScreen();

    void Initialize(lv_display_t* display);
    void Show(SensorCardType type);
    void Hide();
    bool IsVisible() const { return visible_; }
    void UpdateData();

private:
    lv_display_t* display_ = nullptr;
    lv_obj_t* main_screen_ = nullptr;
    lv_obj_t* screen_ = nullptr;
    lv_obj_t* card_box_ = nullptr;
    lv_obj_t* title_label_ = nullptr;
    lv_obj_t* value_label_ = nullptr;
    lv_obj_t* detail_label_ = nullptr;
    lv_obj_t* hint_label_ = nullptr;

    SensorCardType current_type_ = SensorCardType::Temperature;
    esp_timer_handle_t update_timer_ = nullptr;
    esp_timer_handle_t auto_return_timer_ = nullptr;
    bool visible_ = false;

    void CreateUI();
    void ResetAutoReturnTimer();
    static void OnUpdateTimer(void* arg);
    static void OnAutoReturnTimeout(void* arg);
};

#endif // SENSOR_CARD_SCREEN_H
