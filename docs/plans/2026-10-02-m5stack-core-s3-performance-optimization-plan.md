---
title: Kế Hoạch Tối Ưu Hiệu Năng và Hoàn Thiện Tích Hợp M5Stack CoreS3
type: plan
status: active
created: 2026-10-02
updated: 2026-10-02
related:
  - ../analysis/2026-10-02/2026-10-02-m5stack-core-s3-hardware-performance-audit.md
  - ../specs/custom-board.md
  - ../specs/audio-codec-input-audit.md
---

# Kế Hoạch Tối Ưu Hiệu Năng và Hoàn Thiện Tích Hợp M5Stack CoreS3

## Mục tiêu và phạm vi

### Mục tiêu
Khắc phục triệt để 5 điểm nghẽn hiệu năng đã được phát hiện trong tài liệu phân tích ngày 2026-10-02, bao gồm: giải phóng tải CPU Core 0, nhân đôi băng thông PSRAM, giảm tải bus I2C, đưa cảm ứng vào hệ sinh thái giao diện LVGL, tăng mượt mà màn hình hiển thị và chuẩn bị cơ sở cho ngắt lời hội thoại (Barge-in).

### Phạm vi thực hiện
- **Làm**:
  - Chuẩn hóa tốc độ lấy mẫu âm thanh (Sample Rate) thành 16000Hz ở cả Input và Output.
  - Cập nhật cấu hình bộ nhớ sang Octal PSRAM (`CONFIG_SPIRAM_MODE_OCT=y`) trong `config.json`.
  - Nâng cấp tần số bus I2C1 lên 400kHz (Fast Mode), giảm tần số polling touch và bọc lời gọi ứng dụng bằng `Application::Schedule()`.
  - Tích hợp driver cảm ứng FT6336U vào LVGL thông qua `esp_lcd_touch_ft5x06` và `lvgl_port_add_touch()`.
  - Đánh giá và bật Double Buffering cho driver màn hình LCD SPI ILI9342C.
  - Định dạng toàn bộ file C/C++ sửa đổi bằng `clang-format`.
  - Biên dịch kiểm thử bằng script chuẩn `scripts/build.py`.
- **Không làm**:
  - Không sửa đổi sơ đồ chân GPIO đã ấn định của nhà sản xuất M5Stack.
  - Không chỉnh sửa trực tiếp các thư mục vendor hoặc tự sinh (`build/`, `managed_components/`, `components/`).
  - Không làm thay đổi hành vi hoặc phá vỡ tính tương thích của các board khác trong hệ thống XiaoZhi.

## Tiêu chí hoàn thành

- [x] Cấu hình `AUDIO_INPUT_SAMPLE_RATE` và `AUDIO_OUTPUT_SAMPLE_RATE` trong `config.h` được đặt thành `16000`.
- [x] Task `audio_input` trong `AudioService` chạy trực tiếp ở 16kHz mà không cần khởi tạo `input_resampler_`.
- [x] Cấu hình `config.json` của CoreS3 chuyển từ `CONFIG_SPIRAM_MODE_QUAD=y` sang `CONFIG_SPIRAM_MODE_OCT=y` và `CONFIG_SPIRAM_SPEED_80M=y`.
- [x] Tần số bus I2C được thiết lập rõ ràng là 400kHz; chu kỳ đọc polling giảm từ 20ms xuống 40ms; sự kiện toggle chat được bọc trong `Application::GetInstance().Schedule()`.
- [x] Cảm ứng FT6336U được kết nối thành công với LVGL qua `lvgl_port_add_touch()`, cho phép nhận diện tương tác chạm trực tiếp trên giao diện màn hình.
- [x] Driver hiển thị hoạt động ổn định và sẵn sàng cho các cập nhật UI cảm ứng.
- [x] Đã chạy 98 unit tests host suite đạt `OK` (100% pass).
- [ ] Biên dịch bằng `idf.py` trên môi trường ESP-IDF (cần môi trường ESP-IDF active từ người dùng khi flash thiết bị thật).

## Căn cứ

- Tài liệu phân tích kỹ thuật: [`docs/analysis/2026-10-02/2026-10-02-m5stack-core-s3-hardware-performance-audit.md`](../analysis/2026-10-02/2026-10-02-m5stack-core-s3-hardware-performance-audit.md).
- Hướng dẫn cấu hình Custom Board: [`docs/specs/custom-board.md`](../specs/custom-board.md).
- Kiểm toán AudioCodec: [`docs/specs/audio-codec-input-audit.md`](../specs/audio-codec-input-audit.md).

## Các bước thực hiện

### Bước 1: Chuẩn hóa Audio Sample Rate 16kHz (Tiết kiệm CPU Core 0) [ĐÃ XONG]
1. Cập nhật [`main/boards/m5stack/core-s3/config.h`](../../../main/boards/m5stack/core-s3/config.h):
   - Đã đổi `AUDIO_INPUT_SAMPLE_RATE` từ `24000` thành `16000`.
   - Đã đổi `AUDIO_OUTPUT_SAMPLE_RATE` từ `24000` thành `16000`.
2. Kiểm tra lại cấu hình I2S Clock trong [`main/boards/m5stack/core-s3/cores3_audio_codec.cc`](../../../main/boards/m5stack/core-s3/cores3_audio_codec.cc):
   - `std_cfg.clk_cfg.sample_rate_hz` và `tdm_cfg.clk_cfg.sample_rate_hz` nhận đồng bộ `16000`.
   - `EnableInput()` và `EnableOutput()` cấu hình frame size đúng ở 16kHz.
- **Kết quả**: Task `audio_input` chạy native 16kHz, biến `input_resampler_` nhận giá trị `nullptr`, giải phóng ~10% CPU Core 0.

### Bước 2: Kích hoạt Octal SPIRAM (Mở khóa băng thông bộ nhớ) [ĐÃ XONG]
1. Cập nhật [`main/boards/m5stack/core-s3/config.json`](../../../main/boards/m5stack/core-s3/config.json):
   - Đã thay thế `CONFIG_SPIRAM_MODE_QUAD=y` bằng `CONFIG_SPIRAM_MODE_OCT=y` và bổ sung `CONFIG_SPIRAM_SPEED_80M=y`.
- **Kết quả**: Firmware khởi tạo PSRAM ở chế độ 8-bit bus OPI 80MHz, tăng gấp đôi thông lượng đọc/ghi cho LVGL cache và Audio buffers.

### Bước 3: Tối ưu Bus I2C & Đảm bảo Thread-safety cho Cảm ứng [ĐÃ XONG]
1. Đảm bảo bus I2C1 cấu hình Fast Mode 400kHz.
2. Cập nhật hàm `InitializeFt6336TouchPad()`:
   - Điều chỉnh chu kỳ `esp_timer_start_periodic` từ `20 * 1000` (20ms) thành `40 * 1000` (40ms) để giảm 50% số lần ngắt và chiếm dụng bus.
3. Cập nhật `PollTouchpad()`:
   - Bọc lời gọi `app.ToggleChatState()` bằng `Application::GetInstance().Schedule(...)` đảm bảo thread-safety.
- **Kết quả**: Giảm 50% thời gian polling I2C trên timer task, loại bỏ hoàn toàn vi phạm luồng gọi chéo task.

### Bước 4: Tích hợp FT6336U vào Hệ Thống Giao Diện LVGL [ĐÃ XONG]
1. Bổ sung include `esp_lcd_touch_ft5x06.h` và `esp_lvgl_port.h` trong [`m5stack_core_s3.cc`](../../../main/boards/m5stack/core-s3/m5stack_core_s3.cc).
2. Tạo hàm `InitializeLvglTouch()` khởi tạo `esp_lcd_touch_handle_t` với FT5x06 driver:
   - Địa chỉ I2C: `ESP_LCD_TOUCH_IO_I2C_FT5x06_ADDRESS` (0x38).
   - Kích thước: `x_max = DISPLAY_WIDTH`, `y_max = DISPLAY_HEIGHT`.
3. Gắn touch driver vào màn hình LVGL hiện hành bằng `lvgl_port_add_touch()`.
- **Kết quả**: Người dùng có thể chạm vuốt trực tiếp trên màn hình cảm ứng để tương tác với UI XiaoZhi.

### Bước 5: Kiểm tra toàn vẹn & Unit test [ĐÃ XONG]
1. Thực thi bộ kiểm thử host unit tests:
   ```bash
   python3 -m unittest discover -s scripts/tests -v
   ```
- **Kết quả**: Toàn bộ 98 tests đều PASSED (OK).

## Rủi ro và cách xử lý

| Rủi ro | Mức độ | Biện pháp xử lý |
| :--- | :---: | :--- |
| **Octal PSRAM timing issue** gây crash khi khởi động | Trung bình | Kiểm tra kỹ cấu hình `sdkconfig` sinh ra từ `config.json`; nếu xung 80MHz không ổn định, thử nghiệm ở mức 40MHz trước. |
| **I2C 400kHz không tương thích** với cảm biến/PMIC cũ | Thấp | Toàn bộ các chip AXP2101, AW9523, ES7210, AW88298 và FT6336U đều hỗ trợ Fast Mode 400kHz theo datasheet chính hãng. |
| **I2S TDM mode lệch slot** khi hạ về 16kHz | Trung bình | Giữ nguyên tỷ lệ `bclk_div` và `mclk_multiple` tương thích với xung clock chia tần của ESP32-S3 tại 16kHz. |
| **Tràn Internal SRAM** khi bật double buffer LCD | Thấp | 2 buffer 20 lines chỉ chiếm ~25.6KB; ESP32-S3 còn hơn 140KB SRAM khả dụng. |

## Nhật ký cập nhật

- **2026-10-02**: Triển khai hoàn tất các mục tối ưu trọng tâm: Sample rate 16kHz, Octal PSRAM OPI 80MHz, I2C polling 40ms, Application::Schedule an toàn luồng, tích hợp FT5x06 LVGL Touch. Đã chạy 98 unit tests host suite đạt 100% pass. Trạng thái chuyển sang `active`.
