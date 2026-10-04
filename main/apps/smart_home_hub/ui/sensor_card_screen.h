#ifndef SENSOR_CARD_SCREEN_H
#define SENSOR_CARD_SCREEN_H

#include <lvgl.h>
#include <esp_timer.h>
#include <string>
#include <functional>

enum class SensorCardType {
    Temperature,
    Battery,
    Light,
    Motion,
    Peripherals,
    Network,
    System,
    kCount
};

class SensorCardScreen {
public:
    SensorCardScreen();
    ~SensorCardScreen();

    void Initialize(lv_display_t* display);
    void Show(SensorCardType type);
    void Hide();
    // Move to the neighbouring card without going back to XiaoZhi, so the
    // six screens behave like one swipeable row.
    void ShowRelative(int delta);
    bool IsVisible() const { return visible_; }
    void UpdateData();
    void SetOpenWifiConfigCallback(std::function<void()> cb) { on_open_wifi_config_ = std::move(cb); }

private:
    void CreateUI();
    void ApplyType(SensorCardType type);
    void BuildIcon(SensorCardType type);
    void ResetAutoReturnTimer();
    void PushChartSample(int value);

    static void OnUpdateTimer(void* arg);
    static void OnAutoReturnTimeout(void* arg);

    lv_display_t* display_ = nullptr;
    lv_obj_t* main_screen_ = nullptr;
    lv_obj_t* screen_ = nullptr;
    lv_obj_t* frame_ = nullptr;

    // Status bar
    lv_obj_t* clock_label_ = nullptr;
    lv_obj_t* date_label_ = nullptr;
    lv_obj_t* wifi_label_ = nullptr;
    lv_obj_t* bat_label_ = nullptr;

    // Body
    lv_obj_t* icon_box_ = nullptr;
    lv_obj_t* title_label_ = nullptr;
    lv_obj_t* value_label_ = nullptr;
    lv_obj_t* unit_label_ = nullptr;
    lv_obj_t* sub_label_ = nullptr;
    lv_obj_t* bar_ = nullptr;

    // Sparkline. The newest sample is drawn at the right edge (circular update).
    lv_obj_t* chart_ = nullptr;
    lv_chart_series_t* chart_series_ = nullptr;
    lv_obj_t* chart_max_ = nullptr;
    lv_obj_t* chart_mid_ = nullptr;
    lv_obj_t* chart_min_ = nullptr;

    // Axis / detail block
    lv_obj_t* left_column_ = nullptr;
    lv_obj_t* right_column_ = nullptr;
    lv_obj_t* wifi_bars_[4] = {nullptr, nullptr, nullptr, nullptr};
    lv_obj_t* wifi_scan_btn_ = nullptr;

    // Spirit level widget for IMU
    lv_obj_t* spirit_outer_ = nullptr;
    lv_obj_t* spirit_cross_h_ = nullptr;
    lv_obj_t* spirit_cross_v_ = nullptr;
    lv_obj_t* spirit_inner_ring_ = nullptr;
    lv_obj_t* spirit_bubble_ = nullptr;

    // Footer
    lv_obj_t* dots_[static_cast<int>(SensorCardType::kCount)] = {};

    static constexpr int kChartPoints = 48;
    int32_t chart_min_v_ = 0;
    int32_t chart_max_v_ = 100;

    SensorCardType current_type_ = SensorCardType::Temperature;
    esp_timer_handle_t update_timer_ = nullptr;
    esp_timer_handle_t auto_return_timer_ = nullptr;
    bool visible_ = false;
    std::function<void()> on_open_wifi_config_;
};

#endif // SENSOR_CARD_SCREEN_H
