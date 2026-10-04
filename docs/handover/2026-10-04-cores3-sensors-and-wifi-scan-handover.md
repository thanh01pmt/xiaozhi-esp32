---
title: Bàn giao phiên làm việc - Nâng cấp cảm biến, Thước thủy Spirit Level, Thẻ ngoại vi và Màn hình Quét kết nối Wi-Fi trên CoreS3
type: handover
status: active
created: 2026-10-04
updated: 2026-10-04
related:
  - ../plans/2026-10-04-cores3-sensor-and-ui-enhancement-plan.md
  - ./2026-10-04-smart-home-hub-vietnam-audio-handover.md
---

# Bàn giao phiên làm việc - Nâng cấp cảm biến, Thước thủy Spirit Level, Thẻ ngoại vi và Màn hình Quét kết nối Wi-Fi trên CoreS3

## 1. Bối cảnh & Mục tiêu phiên làm việc

Phiên làm việc tiếp nhận yêu cầu nâng cấp toàn diện các tính năng phần cứng và màn hình cho **M5Stack CoreS3 (ESP32-S3)** dựa trên dự án tham chiếu chuẩn [CoreS3-UserDemo](file:///Users/tonypham/MEGA/IDF/CoreS3-UserDemo):
1. **Chuẩn hóa driver cảm biến IMU Bosch BMI270**: Sử dụng driver chính hãng Sensortec từ ESP component registry thay vì upload blob thủ công dễ gây lỗi bộ nhớ.
2. **Đo đạc nguồn AXP2101 PMIC**: Đọc trực tiếp các thanh ghi ADC (VBAT, VBUS, VSYS, TDIE) theo cơ chế của `AppPowerModel`.
3. **Màn hình cảm biến chuyển động mượt mà**: Thiết kế thước thủy bọt nước vector procedural (Spirit Level: tâm chữ thập, vòng giới hạn cân bằng, bọt nước nổi di chuyển theo Roll/Pitch ở tần số 10 Hz).
4. **Màn hình ngoại vi (Peripherals & Ports)**: Bổ sung thẻ hiển thị ma trận phát hiện chip I2C nội bộ và trạng thái các cổng Grove Port A, B, C cùng bus nguồn 5V.
5. **Màn hình Quét & Kết nối Wi-Fi (`WifiConfigScreen`)**: Bổ sung màn hình quét danh sách mạng Wi-Fi thời gian thực, hộp thoại nhập mật khẩu với bàn phím cảm ứng ảo LVGL 9, tự động lưu NVS và kết nối ngay, kèm nút bật Web Portal AP mode (`192.168.4.1`).
6. **Chuẩn hóa Typography**: Giữ toàn bộ nhãn kỹ thuật và UI bằng tiếng Anh ASCII chuẩn để tránh vỡ bố cục và lỗi font ký tự trên màn hình 320x240.

---

## 2. Những việc đã hoàn thành

- [x] **Driver phần cứng BMI270 & AXP2101** ([`main/apps/smart_home_hub/sensor_monitor.cc`](file:///Users/tonypham/MEGA/IDF/xiaozhi-esp32/main/apps/smart_home_hub/sensor_monitor.cc)):
  - Khởi tạo IMU chính xác bằng Bosch Sensortec driver (`bmi2_sec_init`, `bmi2_set_sensor_config`, `bmi2_sensor_enable`, `bmi2_get_sensor_data`).
  - Thêm phép tính Roll, Pitch và độ nghiêng trong [`main/apps/smart_home_hub/sensor_math.h`](file:///Users/tonypham/MEGA/IDF/xiaozhi-esp32/main/apps/smart_home_hub/sensor_math.h).
  - Đọc thanh ghi ADC AXP2101: `0x34` (VBAT), `0x38` (VBUS), `0x3A` (VSYS), `0x3C` (TDIE), `0x00` (VBUS present).
  - Quét ma trận I2C cho 8 chip ngoại vi nội bộ (`pmic_ok`, `imu_ok`, `light_ok`, `touch_ok`, `amp_ok`, `mic_adc_ok`, `io_exp_ok`, `rtc_ok`).
- [x] **Giao diện Thẻ Cảm biến (`SensorCardScreen`)** ([`main/apps/smart_home_hub/ui/sensor_card_screen.cc`](file:///Users/tonypham/MEGA/IDF/xiaozhi-esp32/main/apps/smart_home_hub/ui/sensor_card_screen.cc)):
  - Tích hợp thước thủy vector procedural Spirit Level (70x70 pixel, tâm chữ thập mờ 50%, vòng tròn chuẩn 28x28, bọt nước 16x16 di động mượt mà và chuyển màu xanh lá khi cân bằng < 4°).
  - Thẻ Pin hiển thị 2 cột chi tiết: `BATTERY (VBAT, VBUS)` và `SYSTEM (VSYS, TDIE)`.
  - Bổ sung thẻ `Peripherals` (`PORTS & BUS`) hiển thị trạng thái IC nội bộ và sơ đồ chân Port A (I2C), Port B (GPIO/ADC/DAC), Port C (UART) và đường nguồn 5V.
  - Bổ sung nút bấm `SCAN` trên thẻ Wi-Fi để chuyển thẳng sang màn hình quét Wi-Fi.
- [x] **Màn hình Quét & Kết nối Wi-Fi (`WifiConfigScreen`)** ([`main/apps/smart_home_hub/ui/wifi_config_screen.cc`](file:///Users/tonypham/MEGA/IDF/xiaozhi-esp32/main/apps/smart_home_hub/ui/wifi_config_screen.cc)):
  - Tự động gọi `esp_wifi_scan_start` trong nền và hiển thị danh sách AP sắp xếp theo RSSI mạnh nhất.
  - Hiển thị trực quan: Icon sóng Wi-Fi, tên SSID, cường độ dBm, đánh dấu mạng đã lưu (`Saved`) màu xanh lá cây.
  - Modal kết nối: Chạm vào AP để mở ô nhập mật khẩu, tích hợp bàn phím ảo LVGL `lv_btnmatrix` (chữ số, chữ cái, phím lùi, xóa nhanh, phím cách, OK).
  - Tự động lưu NVS qua `SsidManager` và kết nối với `WifiManager`.
  - Nút `Web Portal` giúp kích hoạt nhanh Config AP (`192.168.4.1`) cho người dùng muốn cấu hình từ trình duyệt.
- [x] **Khôi phục Màn hình Mắt Hoạt hình (Eye View) làm mặc định tuyệt đối**:
  - `SmartHomeHub::LoadSettings()`: Khắc phục triệt để giá trị cũ trong NVS (`def_screen="chat"`), cưỡng chế `DefaultScreenMode::Eyes` và ghi lại NVS.
  - `Application::Initialize()` & `Application::HandleActivationDoneEvent()`: Gọi `SmartHomeHub::ReturnToDefaultScreen()` / `ShowEmotionEyes()` ngay khi khởi động và sau kích hoạt.
  - Tất cả alias chuyển màn hình (`"main"`, `"tro_ly"`, `"xiaozhi"`, `"chinh"`, `"eyes"`, `"eye"`) đều chuyển đến `ShowEmotionEyes()`.
- [x] **Cấu hình thời gian hiển thị tối thiểu 120 giây không tương tác**:
  - Chuẩn hóa toàn bộ bộ đếm thời gian tự động trở về trên mọi màn hình:
    - [`main/apps/smart_home_hub/ui/dashboard_screen.cc`](file:///Users/tonypham/MEGA/IDF/xiaozhi-esp32/main/apps/smart_home_hub/ui/dashboard_screen.cc): 120s (trước là 30s).
    - [`main/apps/smart_home_hub/ui/sensor_dashboard_screen.cc`](file:///Users/tonypham/MEGA/IDF/xiaozhi-esp32/main/apps/smart_home_hub/ui/sensor_dashboard_screen.cc): 120s (trước là 20s).
    - [`main/apps/smart_home_hub/ui/sensor_card_screen.cc`](file:///Users/tonypham/MEGA/IDF/xiaozhi-esp32/main/apps/smart_home_hub/ui/sensor_card_screen.cc): 120s (trước là 15s).
    - [`main/apps/smart_home_hub/ui/camera_preview_screen.cc`](file:///Users/tonypham/MEGA/IDF/xiaozhi-esp32/main/apps/smart_home_hub/ui/camera_preview_screen.cc): 120s (trước là 30s).
    - [`main/apps/smart_home_hub/ui/wifi_config_screen.cc`](file:///Users/tonypham/MEGA/IDF/xiaozhi-esp32/main/apps/smart_home_hub/ui/wifi_config_screen.cc): Bổ sung `esp_timer` 120s, tự động reset khi gõ phím / quét Wi-Fi.
  - Mọi thao tác chạm hoặc nhập liệu đều tự động reset timer 120s. Khi hết 120s không tương tác, hệ thống tự động gọi `SmartHomeHub::ReturnToDefaultScreen()`, chuyển về màn hình biểu cảm mắt hoạt hình.
- [x] **Rà soát tương tác cảm ứng (chống chồng chéo sự kiện)**:
  - Nguyên nhân bấm SCAN bị đẩy về mắt: `Application` xử lý state `idle` luôn gọi `ReturnToDefaultScreen()` (`main/application.cc`). Nay chỉ gọi khi `!SmartHomeHub::IsOverlayVisible()`.
  - Bỏ "chạm bất kỳ đâu để thoát" ở `SensorDashboardScreen` và `SensorCardScreen` (vẫn giữ vuốt trái/phải).
  - Thoát về mắt bằng cảm ứng: **giữ 1.5s vùng góc dưới-trái** (56x28 px, vô hình, trên `lv_layer_top`, `SmartHomeHub::CreateHomeHoldZone()`). Nút Back/Close của Wi-Fi và Dashboard cũng về thẳng màn hình mắt.
  - Timeout 120s dùng `lv_display_get_inactive_time()` (`SmartHomeHub::InactivityRemainingUs()`): mọi lần chạm ở đâu cũng reset.
  - Chưa kiểm thử trên phần cứng.
- [x] **Tích hợp Điều hướng & MCP Server** ([`main/apps/smart_home_hub/smart_home_hub.cc`](file:///Users/tonypham/MEGA/IDF/xiaozhi-esp32/main/apps/smart_home_hub/smart_home_hub.cc)):
  - Hỗ trợ các lệnh chuyển màn hình: `"wifi_config"`, `"wifi_scan"`, `"scan_wifi"`, `"peripherals"`, `"ports"`.
  - Cập nhật danh sách màn hình trong `ListScreensJson()` và công cụ MCP.
- [x] **Kiểm thử và xác minh**:
  - Chạy toàn bộ 99/99 bài kiểm tra host unit tests: `Ran 99 tests in 6.414s ... OK`.
  - Biên dịch firmware ESP-IDF v6.1 thành công 100%: `xiaozhi.bin` dung lượng 2.84MB (còn trống 28% phân vùng app).

---

## 3. Trạng thái hiện tại & Việc đang dang dở

- [x] Toàn bộ mã nguồn driver cảm biến và màn hình hiển thị đã hoàn tất và biên dịch thành công.
- [ ] **Thử nghiệm trên phần cứng thực tế**: Chờ cắm cáp USB-C kết nối M5Stack CoreS3 với máy tính để nạp firmware và theo dõi log Serial.

---

## 4. Lưu ý kỹ thuật & Nợ kỹ thuật (Gotchas & Technical Debt)

1. **LVGL 9 Widget Configuration**:
   - `CONFIG_LV_USE_LIST` và `CONFIG_LV_USE_KEYBOARD` không được bật mặc định trong `sdkconfig`.
   - Do đó, `WifiConfigScreen` được xây dựng bằng `lv_obj_create` kết hợp flex layout column và `lv_btnmatrix_create` (Button Matrix). Cách tiếp cận này đảm bảo tương thích tuyệt đối trên mọi board mà không cần sửa đổi `sdkconfig`.
2. **Quản lý bộ nhớ user_data trong LVGL**:
   - Các chuỗi heap `std::string` cấp phát cho từng dòng button trong danh sách AP được giải phóng tự động qua sự kiện `LV_EVENT_DELETE` trên button tương ứng, đảm bảo không có rò rỉ bộ nhớ (memory leak) khi quét mạng nhiều lần.
3. **Typography & Font Montserrat**:
   - Màn hình 320x240 LCD của CoreS3 hiển thị font `lv_font_montserrat_14` với ký tự ASCII rất sắc nét và vừa vặn. Tuyệt đối không dùng tiếng Việt có dấu ở các nhãn chỉ số kỹ thuật để tránh hiện tượng tràn khung hoặc ký tự chữ nhật trống (missing glyphs).

---

## 5. Hướng dẫn hành động cho Agent kế tiếp (Next Actions)

1. Khi người dùng kết nối M5Stack CoreS3 qua cổng USB-C, kiểm tra cổng serial bằng lệnh:
   ```bash
   ls /dev/cu.usbmodem*
   ```
2. Thực hiện nạp toàn bộ firmware:
   ```bash
   source ~/esp/esp-idf/export.sh
   python3 $IDF_PATH/tools/idf.py -p /dev/cu.usbmodem* flash monitor
   ```
3. Kiểm tra màn hình:
   - Thử nghiệm thước thủy Spirit Level bằng cách nghiêng bo mạch theo các hướng.
   - Thử nghiệm thẻ Network và bấm nút `SCAN` để kiểm tra quét mạng Wi-Fi và bàn phím ảo.
   - Thử nghiệm thẻ `PORTS & BUS` để xem trạng thái phát hiện các chip I2C.

---

## 6. Lệnh kiểm tra & xác minh (Verification Commands)

```bash
# 1. Chạy host-side unit tests
python3 -m unittest discover -s scripts/tests -v

# 2. Biên dịch firmware
source ~/esp/esp-idf/export.sh
python3 $IDF_PATH/tools/idf.py build
```
