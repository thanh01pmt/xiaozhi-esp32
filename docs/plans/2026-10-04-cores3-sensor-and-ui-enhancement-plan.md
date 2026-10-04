---
title: Kế hoạch nâng cấp cảm biến và giao diện M5Stack Core S3 theo chuẩn UserDemo
type: plan
status: active
created: 2026-10-04
updated: 2026-10-04
related: []
---

# Kế hoạch nâng cấp cảm biến và giao diện M5Stack Core S3 theo chuẩn UserDemo

## Mục tiêu và phạm vi

- Mục tiêu:
  1. Cấu hình và sửa đổi driver phần cứng (`SensorMonitor`) trong `xiaozhi-esp32` dựa trên thiết kế chuẩn đã kiểm chứng từ `CoreS3-UserDemo` (AXP2101, BMI270, BMM150, LTR-553ALS).
  2. Nâng cấp các màn hình thẻ cảm biến (`SensorCardScreen`) và dashboard (`SensorDashboardScreen`) trong `main/apps/smart_home_hub/ui` để hiển thị trực quan, mượt mà giống như các ứng dụng gốc (`AppPower`, `AppIMU`, `AppMic`, `AppCamera`).
  3. Đảm bảo tích hợp hoàn hảo với kiến trúc XiaoZhi hiện tại (không phá vỡ Audio/AEC/Wake Word, không tràn phân vùng flash, LVGL 9 rendering mượt mà).

- Không làm:
  - Không nhúng ồ ạt các file ảnh bitmap 5.5MB vào flash (gây tràn dung lượng phân vùng 4MB).
  - Không sửa đổi driver các board khác trong repo.

## Tiêu chí hoàn thành

- [x] Cảm biến IMU BMI270 khởi tạo thành công bằng driver chính hãng Bosch Sensortec (`bmi2_sec_init`, `bmi2_set_sensor_config`, `bmi2_sensor_enable`, `bmi2_get_sensor_data`), không còn lỗi upload blob thủ công.
- [x] Dữ liệu gia tốc (Accelerometer), con quay (Gyroscope), Roll, Pitch và Tilt Degrees được tính toán và cập nhật theo thời gian thực.
- [x] Thông số nguồn AXP2101 (VBAT mV, VBUS mV, VSYS mV, TDIE nhiệt độ die, trạng thái cắm sạc) đọc chính xác theo các thanh ghi ADC của `AppPowerModel`.
- [x] Màn hình cảm biến chuyển động có thước thủy bọt nước cân bằng (Spirit Level: vòng chuẩn, tâm chữ thập, bọt nước nổi di chuyển theo góc Roll/Pitch, đổi màu khi đạt cân bằng) bằng vector procedural LVGL 9 không tốn flash.
- [x] Màn hình pin hiển thị chi tiết đồ họa: thanh đo phần trăm, điện áp VBAT, nguồn sạc VBUS, điện áp hệ thống VSYS và nhiệt độ PMIC.
- [x] Màn hình ngoại vi và cổng kết nối (`Peripherals`): hiển thị bảng ma trận trạng thái chip nội bộ I2C (PMIC, IMU, Touch, ALS, Amp, ADC, Expander, RTC) và sơ đồ chân Port A, Port B, Port C cùng nguồn 5V.
- [x] Màn hình Quét và Kết nối Wi-Fi (`WifiConfigScreen`): danh sách AP scan thời gian thực, hiển thị RSSI dạng icon vạch sóng và dBm, modal nhập mật khẩu bàn phím ảo LVGL, lưu NVS và kết nối ngay, kèm nút bật Web Portal AP mode (`192.168.4.1`).
- [x] Nút "SCAN" nhanh trên thẻ Wi-Fi trong `SensorCardScreen` để chuyển nhanh vào màn hình quét Wi-Fi.
- [x] Giữ toàn bộ typography và nhãn kỹ thuật bằng tiếng Anh ASCII chuẩn để tránh vỡ bố cục và lỗi font ký tự.
- [x] Firmware biên dịch thành công 100% không lỗi.

## Các bước

1. [x] **Bước 1 — Chuẩn hóa driver phần cứng (`SensorMonitor`)**:
   - Cập nhật đọc đầy đủ các thanh ghi ADC AXP2101 theo `AppPowerModel` (VBAT 0x34, VBUS 0x38, VSYS 0x3A, TDIE 0x3C, VBUS present 0x00).
   - Tích hợp driver Bosch BMI270 chính thức từ `managed_components/espressif__bmi270_sensor` vào `main/apps/smart_home_hub/sensor_monitor.cc`.
   - Thêm tính toán Roll, Pitch trong `sensor_math.h` và cập nhật cấu trúc `CoreS3SensorSnapshot` và hàm `GetAllSensorsJson()`.
   - Thêm quét và giám sát I2C ngoại vi và trạng thái nguồn cổng ngoại vi (`PortPeripheralData`).
2. [x] **Bước 2 — Nâng cấp giao diện hiển thị (`SensorCardScreen`)**:
   - Thêm widget Thước thủy Spirit Level (khung tròn 70x70, tâm chữ thập, vòng giới hạn cân bằng, bọt nước 16x16 tự động di động theo độ nghiêng Roll & Pitch, chuyển màu xanh lá khi cân bằng < 4°).
   - Hiển thị đầy đủ thông số gia tốc 3 trục X/Y/Z, góc quay R/P và tốc độ góc Z.
   - Thêm chế độ hiển thị 2 cột chi tiết cho Pin: VBAT, VBUS, VSYS, TDIE và trạng thái nguồn USB.
   - Thêm thẻ `Peripherals` (PORTS & BUS) hiển thị chip IC ma trận I2C và Port A/B/C.
   - Thêm nút chuyển sang màn hình quét Wi-Fi trên thẻ Network.
   - Tối ưu hóa chu kỳ cập nhật lên 100ms (10 Hz) để bọt nước di chuyển cực kỳ mượt mà.
3. [x] **Bước 3 — Bổ sung màn hình Quét & Kết nối Wi-Fi (`WifiConfigScreen`)**:
   - Quét mạng Wi-Fi thời gian thực qua `esp_wifi_scan_start` và sắp xếp theo tín hiệu mạnh nhất.
   - Danh sách cuộn trực quan với trạng thái đã lưu (`Saved`) màu xanh lá và icon vạch sóng.
   - Hộp thoại Modal nhập mật khẩu với bàn phím cảm ứng ảo LVGL (`lv_keyboard`).
   - Kết nối và lưu NVS tự động qua `SsidManager` và `WifiManager`.
   - Nút bật Web Portal AP mode chuyển sang điểm phát cấu hình web trực quan.
4. [ ] **Bước 4 — Nạp thử nghiệm trên phần cứng thực tế**:
   - Chờ người dùng cắm M5Stack CoreS3 qua cổng USB-C.
   - Chạy lệnh nạp `./docs/setup/flash_cores3.sh /dev/cu.usbmodem* all` hoặc `python3 $IDF_PATH/tools/idf.py -p <PORT> flash monitor`.

## Nhật ký cập nhật

- 2026-10-04: Khởi tạo kế hoạch nâng cấp toàn diện dựa trên tài nguyên `CoreS3-UserDemo`.
- 2026-10-04: Hoàn thành bước 1 (Driver BMI270 & AXP2101 ADC) và bước 2 (Giao diện Spirit Level & Power Monitor trên SensorCardScreen). Biên dịch thành công `xiaozhi.bin`.
- 2026-10-04: Thêm màn hình Peripheral Ports & Bus và màn hình Wi-Fi Scan & Connect (`WifiConfigScreen`) với bàn phím ảo và Web Portal AP. Chuyển toàn bộ typography sang tiếng Anh ASCII chuẩn.
