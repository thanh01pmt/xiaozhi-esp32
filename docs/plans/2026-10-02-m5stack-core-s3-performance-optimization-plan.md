---
title: Kế Hoạch Tối Ưu Hiệu Năng và Hoàn Thiện Tích Hợp M5Stack CoreS3
type: plan
status: draft
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

- [ ] Cấu hình `AUDIO_INPUT_SAMPLE_RATE` và `AUDIO_OUTPUT_SAMPLE_RATE` trong `config.h` được đặt thành `16000`.
- [ ] Task `audio_input` trong `AudioService` chạy trực tiếp ở 16kHz mà không cần khởi tạo `input_resampler_`.
- [ ] Cấu hình `config.json` của CoreS3 chuyển từ `CONFIG_SPIRAM_MODE_QUAD=y` sang `CONFIG_SPIRAM_MODE_OCT=y`.
- [ ] Tần số bus I2C được thiết lập rõ ràng là 400kHz; chu kỳ đọc polling giảm từ 20ms xuống 40ms; sự kiện toggle chat được bọc trong `Application::GetInstance().Schedule()`.
- [ ] Cảm ứng FT6336U được kết nối thành công với LVGL qua `lvgl_port_add_touch()`, cho phép nhận diện tương tác chạm trực tiếp trên giao diện màn hình.
- [ ] Driver hiển thị hỗ trợ buffer DMA mượt mà hơn.
- [ ] Lệnh kiểm tra định dạng `clang-format --dry-run -Werror <files>` đạt yêu cầu sạch sẽ.
- [ ] Biên dịch thành công với lệnh: `python3 scripts/build.py m5stack/core-s3 --name m5stack-core-s3`.

## Căn cứ

- Tài liệu phân tích kỹ thuật: [`docs/analysis/2026-10-02/2026-10-02-m5stack-core-s3-hardware-performance-audit.md`](../analysis/2026-10-02/2026-10-02-m5stack-core-s3-hardware-performance-audit.md).
- Hướng dẫn cấu hình Custom Board: [`docs/specs/custom-board.md`](../specs/custom-board.md).
- Kiểm toán AudioCodec: [`docs/specs/audio-codec-input-audit.md`](../specs/audio-codec-input-audit.md).

## Các bước thực hiện

### Bước 1: Chuẩn hóa Audio Sample Rate 16kHz (Tiết kiệm CPU Core 0)
1. Cập nhật [`main/boards/m5stack/core-s3/config.h`](../../../main/boards/m5stack/core-s3/config.h):
   - Đổi `AUDIO_INPUT_SAMPLE_RATE` từ `24000` thành `16000`.
   - Đổi `AUDIO_OUTPUT_SAMPLE_RATE` từ `24000` thành `16000`.
2. Kiểm tra lại cấu hình I2S Clock trong [`main/boards/m5stack/core-s3/cores3_audio_codec.cc`](../../../main/boards/m5stack/core-s3/cores3_audio_codec.cc):
   - Đảm bảo `std_cfg.clk_cfg.sample_rate_hz` và `tdm_cfg.clk_cfg.sample_rate_hz` nhận đồng bộ `16000`.
   - Kiểm tra `EnableInput()` và `EnableOutput()` cấu hình frame size đúng ở 16kHz.
- **Đầu ra**: Task `audio_input` chạy native 16kHz, biến `input_resampler_` nhận giá trị `nullptr`, giải phóng ~10% CPU Core 0.

### Bước 2: Kích hoạt Octal SPIRAM (Mở khóa băng thông bộ nhớ)
1. Cập nhật [`main/boards/m5stack/core-s3/config.json`](../../../main/boards/m5stack/core-s3/config.json):
   - Thay thế `CONFIG_SPIRAM_MODE_QUAD=y` bằng `CONFIG_SPIRAM_MODE_OCT=y`.
   - Bổ sung cấu hình timing/tần số tối ưu cho Octal PSRAM nếu cần.
- **Đầu ra**: Firmware khởi tạo PSRAM ở chế độ 8-bit bus, tăng gấp đôi thông lượng đọc/ghi cho LVGL cache và Audio buffers.

### Bước 3: Tối ưu Bus I2C & Đảm bảo Thread-safety cho Cảm ứng
1. Cập nhật `InitializeI2c()` trong [`main/boards/m5stack/core-s3/m5stack_core_s3.cc`](../../../main/boards/m5stack/core-s3/m5stack_core_s3.cc):
   - Cấu hình rõ ràng `scl_speed_hz = 400000` (Fast Mode 400kHz) cho bus I2C1.
2. Cập nhật hàm `InitializeFt6336TouchPad()`:
   - Điều chỉnh chu kỳ `esp_timer_start_periodic` từ `20 * 1000` (20ms) thành `40 * 1000` (40ms) để giảm 50% số lần ngắt và chiếm dụng bus.
3. Cập nhật `PollTouchpad()`:
   - Bọc lời gọi `app.ToggleChatState()` bằng:
     ```cpp
     Application::GetInstance().Schedule([]() {
         Application::GetInstance().ToggleChatState();
     });
     ```
- **Đầu ra**: Bus I2C chạy nhanh gấp 4 lần, giảm 50% thời gian polling, loại bỏ hoàn toàn vi phạm luồng gọi chéo task.

### Bước 4: Tích hợp FT6336U vào Hệ Thống Giao Diện LVGL
1. Bổ sung include `esp_lcd_touch_ft5x06.h` và `esp_lvgl_port.h` trong [`m5stack_core_s3.cc`](../../../main/boards/m5stack/core-s3/m5stack_core_s3.cc).
2. Tạo đối tượng `esp_lcd_touch_handle_t` sử dụng cấu hình bus I2C1 hiện có:
   - Địa chỉ I2C: `0x38`.
   - Kích thước: `x_max = DISPLAY_WIDTH`, `y_max = DISPLAY_HEIGHT`.
3. Gắn touch driver vào màn hình LVGL hiện hành bằng `lvgl_port_add_touch()`:
   ```cpp
   lvgl_port_touch_cfg_t touch_cfg = {};
   touch_cfg.disp = display_->GetLvglDisplay();
   touch_cfg.handle = tp_handle;
   lvgl_port_add_touch(&touch_cfg);
   ```
4. Giữ lại hoặc hợp nhất cử chỉ chạm bật/tắt hội thoại thông qua event callback của LVGL.
- **Đầu ra**: Người dùng có thể chạm vuốt trực tiếp trên màn hình cảm ứng để tương tác với UI XiaoZhi.

### Bước 5: Tối ưu Bộ Đệm Màn Hình LCD (Double Buffering)
1. Khảo sát việc cấu hình `double_buffer = true` trong hàm khởi tạo màn hình `SpiLcdDisplay` hoặc tại cấu hình LCD của CoreS3.
2. Đánh giá lượng RAM nội (Internal SRAM) tiêu thụ khi dùng 2 buffer `320 * 20` (tổng 25.6KB).
- **Đầu ra**: Tăng độ mượt hiển thị và loại bỏ hiện tượng giật/chớp hình khi render các biểu cảm động (GIF/Emoji).

### Bước 6: Định dạng mã nguồn & Biên dịch kiểm chứng
1. Chạy công cụ kiểm tra định dạng:
   ```bash
   clang-format -i main/boards/m5stack/core-s3/*.cc main/boards/m5stack/core-s3/*.h
   clang-format --dry-run -Werror main/boards/m5stack/core-s3/*.cc main/boards/m5stack/core-s3/*.h
   ```
2. Thực thi lệnh biên dịch chính thức:
   ```bash
   python3 scripts/build.py m5stack/core-s3 --name m5stack-core-s3
   ```
3. Chạy bộ kiểm thử host test để đảm bảo không gãy cấu trúc build:
   ```bash
   python3 -m unittest discover -s scripts/tests -v
   ```
- **Đầu ra**: Toàn bộ mã nguồn sạch lỗi cú pháp, biên dịch sinh file `.bin` hoàn chỉnh sẵn sàng nạp.

## Rủi ro và cách xử lý

| Rủi ro | Mức độ | Biện pháp xử lý |
| :--- | :---: | :--- |
| **Octal PSRAM timing issue** gây crash khi khởi động | Trung bình | Kiểm tra kỹ cấu hình `sdkconfig` sinh ra từ `config.json`; nếu xung 80MHz không ổn định, thử nghiệm ở mức 40MHz trước. |
| **I2C 400kHz không tương thích** với cảm biến/PMIC cũ | Thấp | Toàn bộ các chip AXP2101, AW9523, ES7210, AW88298 và FT6336U đều hỗ trợ Fast Mode 400kHz theo datasheet chính hãng. |
| **I2S TDM mode lệch slot** khi hạ về 16kHz | Trung bình | Giữ nguyên tỷ lệ `bclk_div` và `mclk_multiple` tương thích với xung clock chia tần của ESP32-S3 tại 16kHz. |
| **Tràn Internal SRAM** khi bật double buffer LCD | Thấp | 2 buffer 20 lines chỉ chiếm ~25.6KB; ESP32-S3 còn hơn 140KB SRAM khả dụng. |

## Nhật ký cập nhật

- **2026-10-02**: Khởi tạo kế hoạch tối ưu toàn diện phần cứng & hiệu năng M5Stack CoreS3 ở trạng thái `draft`.
