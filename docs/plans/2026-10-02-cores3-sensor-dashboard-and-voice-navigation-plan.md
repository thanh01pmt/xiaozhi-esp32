---
title: Kế hoạch triển khai Màn hình Cảm biến, Camera và Điều hướng Giọng nói AI trên M5Stack CoreS3
type: plan
status: active
created: 2026-10-02
updated: 2026-10-02
related:
  - docs/analysis/2026-10-02/2026-10-02-m5stack-core-s3-hardware-performance-audit.md
  - docs/plans/2026-10-02-m5stack-core-s3-performance-optimization-plan.md
---

# Kế hoạch triển khai Màn hình Cảm biến, Camera và Điều hướng Giọng nói AI trên M5Stack CoreS3

## Mục tiêu và phạm vi

### Mục tiêu
1. **Quản lý & Điều hướng Màn hình bằng Giọng nói (Voice Screen Navigation)**:
   - Cho phép AI liệt kê toàn bộ danh sách màn hình khả dụng (`main`, `sensors`, `smarthome`, `camera`).
   - Cho phép người dùng ra lệnh bằng giọng nói để chuyển đổi giữa các màn hình bất kỳ lúc nào.
2. **Hệ thống Cảm biến Toàn diện (Full Hardware Sensor Telemetry)**:
   - Thu thập toàn bộ cảm biến phần cứng của M5Stack CoreS3:
     - Nguồn & Pin: AXP2101 PMIC (`0x34`) - Mức pin %, sạc/xả, nhiệt độ bo mạch.
     - Cảm biến Ánh sáng & Tiệm cận: LTR-553ALS (`0x23`) - Cường độ sáng Ambient Lux và Proximity.
     - Cảm biến Chuyển động & Gia tốc IMU 6 trục: BMI270 (`0x69`) - Gia tốc X/Y/Z, Con quay hồi chuyển (Gyroscope), nhận diện tư thế máy (nằm phẳng, thẳng đứng, dốc nghiêng).
     - Kết nối Mạng: Wi-Fi RSSI, SSID, Kênh truyền sóng, IP v4.
     - Tài nguyên Hệ thống: Bộ nhớ nội SRAM rảnh, PSRAM rảnh, Tần số CPU, Thời gian Uptime.
   - Thiết kế giao diện **Sensor Dashboard** hiển thị trực quan, hiện đại bằng LVGL 9 trên màn hình ILI9342C của M5Stack CoreS3.
3. **AI Grounding & Tương tác Cảm biến Toàn diện**:
   - Cung cấp các công cụ MCP để AI truy cập dữ liệu cảm biến (toàn bộ hoặc chuyên sâu theo từng loại: pin, nhiệt độ, ánh sáng lux, chuyển động tư thế, mạng, tài nguyên hệ thống) để trả lời các câu hỏi thực tế của người dùng.
4. **Tích hợp Camera GC0308 & Nhận diện hình ảnh qua Giọng nói**:
   - Hỗ trợ công cụ camera cho AI (`self.camera.take_photo`), cho phép chụp hình và nhận diện thị giác thông minh.
   - Hỗ trợ chuyển sang màn hình camera xem trước hoặc chụp phân tích ảnh qua khẩu lệnh `ui.switch_screen`.

### Phạm vi
- Áp dụng trên bo mạch **M5Stack CoreS3** (ESP32-S3, AXP2101 PMIC, AW9523, ILI9342C, FT6336U, GC0308 DVP Camera, LTR-553ALS, BMI270).
- Tích hợp an toàn trên bus I2C master dùng chung của ESP-IDF v6, không làm nghẽn luồng âm thanh và không gây cạn kiệt bộ nhớ.

## Tiêu chí hoàn thành

- [x] Tạo module `SensorMonitor` đọc dữ liệu thời gian thực từ AXP2101, LTR-553ALS, BMI270, Wi-Fi và bộ nhớ RAM.
- [x] Thiết kế màn hình `SensorDashboardScreen` bằng LVGL 9 hiển thị các thẻ cảm biến (Pin & Nguồn, Nhiệt độ & Ánh sáng Lux, Wi-Fi & Mạng, Bộ nhớ RAM & Tư thế IMU).
- [x] Xây dựng bộ điều hướng màn hình quản lý 4 màn hình (`main`, `sensors`, `smarthome`, `camera`).
- [x] Đăng ký bộ MCP Tools:
  - `ui.list_screens`: Liệt kê các màn hình có thể mở (kèm mô tả tiếng Việt).
  - `ui.switch_screen`: Chuyển màn hình theo tên (`main`, `sensors`, `smarthome`, `camera`).
  - `sensor.get_all_sensors`: Đọc tổng hợp dữ liệu toàn bộ cảm biến CoreS3.
  - `sensor.get_sensor_data`: Đọc chuyên sâu từng loại cảm biến (`battery`, `temperature`, `light`, `motion`, `network`, `system`).
- [x] Biên dịch sạch (clean build) không lỗi cảnh báo hay xung đột I2C.
- [ ] Nạp firmware và kiểm tra tương tác đàm thoại AI thực tế trên thiết bị.

## Căn cứ
- [m5stack_core_s3.cc](file:///Users/tonypham/MEGA/IDF/xiaozhi-esp32/main/boards/m5stack/core-s3/m5stack_core_s3.cc)
- [sensor_monitor.h](file:///Users/tonypham/MEGA/IDF/xiaozhi-esp32/main/apps/smart_home_hub/sensor_monitor.h)
- [sensor_dashboard_screen.h](file:///Users/tonypham/MEGA/IDF/xiaozhi-esp32/main/apps/smart_home_hub/ui/sensor_dashboard_screen.h)
- [mcp_tools.cc](file:///Users/tonypham/MEGA/IDF/xiaozhi-esp32/main/apps/smart_home_hub/mcp_tools.cc)

## Các bước thực hiện

1. [x] **Bước 1: Triển khai Module Cảm biến `SensorMonitor` đa cảm biến**
   - Đọc AXP2101: Mức pin %, dòng sạc/xả, nhiệt độ bo mạch.
   - Giao tiếp I2C với LTR-553ALS (`0x23`): Khởi động chế độ Active, đọc thanh ghi ALS Channel 0 & 1, tính toán Lux và khoảng cách Proximity.
   - Giao tiếp I2C với BMI270 (`0x69`): Khởi động Power Control, cấu hình Accel/Gyro, đọc dữ liệu 6 trục, phân tích góc nghiêng và tư thế máy (Flat/Nằm phẳng, Upright/Đứng, Tilted/Nghiêng).
   - Đọc Network: SSID, RSSI, IP, Channel.
   - Đọc System: Heap SRAM còn lại, PSRAM còn lại, CPU frequency, Uptime.
   - Đóng gói dữ liệu thành Struct snapshot và xuất chuỗi JSON chuẩn hóa cho AI.
2. [x] **Bước 2: Xây dựng Giao diện LVGL `SensorDashboardScreen`**
   - Thiết kế giao diện Dark UI hiện đại (nền đen 0x0F0F14, 4 card bo góc viền tinh tế).
   - Card 1: Pin & Trạng thái sạc nguồn.
   - Card 2: Nhiệt độ bo mạch & Cường độ ánh sáng Lux (LTR-553ALS).
   - Card 3: Tên Wi-Fi SSID, RSSI sóng và Kênh mạng.
   - Card 4: Dung lượng RAM khả dụng & Tư thế máy / IMU (BMI270).
   - Cơ chế tự động làm tươi mỗi 1 giây và tự động ẩn về màn hình trợ lý sau 20 giây không tương tác.
3. [x] **Bước 3: Tích hợp Bộ điều hướng Màn hình & Tích hợp Camera**
   - Thống nhất cơ chế hiển thị giữa Màn hình Trợ lý chính, Smart Home Dashboard, Sensor Dashboard và Camera.
   - Đăng ký công cụ `ui.list_screens` và `ui.switch_screen`.
   - Kết nối với `Board::GetInstance().GetCamera()` khi chuyển sang chế độ `camera`.
4. [x] **Bước 4: Đăng ký MCP Sensor Grounding Tools**
   - Đăng ký `sensor.get_all_sensors` và `sensor.get_sensor_data`.
   - Mô tả schema chi tiết bằng tiếng Việt để AI LLM hiểu rõ ngữ cảnh phần cứng của M5Stack CoreS3.
5. [ ] **Bước 5: Build, Flash & Xác minh hoạt động thực tế**
   - Biên dịch thành công với toolchain ESP-IDF v6.
   - Hướng dẫn nạp firmware và câu lệnh kiểm tra mẫu:
     - *"M5Stack CoreS3 có những màn hình nào?"*
     - *"Mở màn hình cảm biến"*
     - *"Ánh sáng phòng hiện tại thế nào, máy có đang nằm phẳng không?"*
     - *"Chụp một bức ảnh và cho tôi biết bạn thấy gì"*
     - *"Quay lại màn hình trợ lý"*

## Rủi ro và cách xử lý
- **Xung đột bus I2C dùng chung**:
  - AXP2101, AW9523, FT6336U, LTR-553ALS và BMI270 đều nằm trên I2C bus Port 1 (SDA GPIO 12, SCL GPIO 11).
  - Sử dụng API `i2c_master_bus_add_device` chuẩn của ESP-IDF v6 với cấu hình tốc độ 100kHz an toàn.
  - Các thao tác đọc cảm biến đều có timeout và kiểm tra trả lời ACK/NACK tránh crash hệ thống nếu chip chưa sẵn sàng.

## Nhật ký cập nhật
- 2026-10-02: Khởi tạo tài liệu kế hoạch chi tiết theo chuẩn `docs/`.
- 2026-10-02: Bổ sung cảm biến LTR-553ALS, BMI270 và Camera GC0308 vào kế hoạch; hoàn thành code implementation; cập nhật tiêu chí xong và nhật ký.
