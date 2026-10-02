---
title: Hướng Dẫn Cài Đặt Môi Trường và Khắc Phục Sự Cố Dự Án XiaoZhi ESP32
type: spec
status: active
created: 2026-10-02
updated: 2026-10-02
related:
  - ../specs/custom-board.md
  - ../plans/2026-10-02-m5stack-core-s3-performance-optimization-plan.md
  - ../plans/2026-10-02-smart-home-hub-mcp-miniapp-plan.md
---

# Hướng Dẫn Cài Đặt Môi Trường và Khắc Phục Sự Cố Dự Án XiaoZhi ESP32

Tài liệu này tổng hợp toàn bộ các lưu ý về thiết lập công cụ, môi trường build, sự cố tương thích phiên bản ESP-IDF v6.1 và các điểm cần ghi nhớ khi bắt đầu làm việc với một workspace/dự án mới trên macOS / Linux.

---

## 1. Yêu cầu Môi trường & Công cụ Bắt buộc

Dự án XiaoZhi yêu cầu tối thiểu **ESP-IDF v6.0.1**, khuyến nghị dùng **ESP-IDF v6.1** (IDF 5.x không còn được hỗ trợ).

### A. Cài đặt các công cụ hệ thống (Host Tooling trên macOS)
Nếu máy Mac mới cài đặt hoặc thiếu công cụ dòng lệnh:
```bash
# Cài đặt trình quản lý gói Homebrew nếu chưa có
# Cài đặt cmake và ninja (Bắt buộc cho Ninja build system của ESP-IDF)
brew install cmake ninja git
```

### B. Cài đặt ESP-IDF v6.1 (cho chip ESP32-S3)
```bash
mkdir -p ~/esp
cd ~/esp
git clone -b release/v6.1 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32s3
```

### C. Kích hoạt môi trường trong mỗi phiên làm việc Terminal
```bash
source ~/esp/esp-idf/export.sh
```

---

## 2. Các Vấn đề & Giải pháp Kỹ thuật Đã Gặp (Gotchas & Fixes)

### 2.1. Lần build đầu tiên bị treo / mất 20–30 phút
- **Hiện tượng**: Khi chạy `python3 scripts/build.py ...`, lệnh dừng rất lâu ở bước `NOTE: Processing 74 dependencies: [1/74] ... [65/74] lvgl/lvgl ...`.
- **Nguyên nhân**: Dự án dùng **74 managed components** (LVGL 9, ESP-SR AI models ~50MB, esp32-camera, codec...). Lần đầu tiên build, IDF Component Manager phải tải toàn bộ 74 gói này từ server về `~/.espressif/component_cache`.
- **Giải pháp**:
  - Không tắt terminal / không nhấn `Ctrl + C`. Hãy đợi 15–20 phút để toàn bộ gói được tải về.
  - Sau khi tải xong lần đầu, file `dependencies.lock` được sinh ra. **Từ lần build thứ 2 trở đi, bước này sẽ mất 0 giây**.

### 2.2. Lỗi tương thích API GDMA giữa ESP-IDF v6.1 và component `78__uart-uhci`
- **Hiện tượng**:
  ```text
  error: 'gdma_get_alignment_constraints' was not declared in this scope; 
  did you mean 'gdma_get_channel_alignment_constraints'?
  ```
- **Nguyên nhân**: Từ ESP-IDF v6.0 trở đi, API GDMA đổi tên hàm `gdma_get_alignment_constraints` thành `gdma_get_channel_alignment_constraints` và dùng struct `gdma_channel_alignment_info_t`.
- **Giải pháp**: Tại `managed_components/78__uart-uhci/src/uart_uhci.cc`:
  ```cpp
  #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
      gdma_channel_alignment_info_t align_info = {};
      gdma_get_channel_alignment_constraints(rx_dma_chan_, &align_info);
      rx_int_mem_align_ = align_info.int_mem_alignment;
      rx_ext_mem_align_ = align_info.ext_no_enc_mem_alignment;
  #else
      gdma_get_alignment_constraints(rx_dma_chan_, &rx_int_mem_align_, &rx_ext_mem_align_);
  #endif
  ```

### 2.3. Lỗi thứ tự Designated Initializer trong C++20 trên ESP-IDF
- **Hiện tượng**:
  ```text
  error: designator order for field 'esp_lcd_panel_io_i2c_config_t::scl_speed_hz' 
  does not match declaration order in 'esp_lcd_panel_io_i2c_config_t'
  ```
- **Nguyên nhân**: Dự án cấu hình chuẩn `-std=gnu++26 / C++20`, yêu cầu thứ tự các trường designated initializer (`.field = value`) phải khớp 100% với thứ tự khai báo trong file header `esp_lcd_io_i2c.h`.
- **Giải pháp**: Khởi tạo bằng rỗng `{}` và gán tường minh theo thứ tự:
  ```cpp
  esp_lcd_panel_io_i2c_config_t tp_io_config = {};
  tp_io_config.dev_addr = ESP_LCD_TOUCH_IO_I2C_FT5x06_ADDRESS;
  tp_io_config.scl_speed_hz = 400000;
  tp_io_config.control_phase_bytes = 1;
  tp_io_config.dc_bit_offset = 0;
  tp_io_config.lcd_cmd_bits = 8;
  tp_io_config.flags.disable_control_phase = 1;
  ```

### 2.4. Khai báo component phụ thuộc vào `PRIV_REQUIRES`
- **Hiện tượng**:
  ```text
  Compilation failed because network_client.cc (in "main" component) includes esp_http_client.h...
  However, esp_http_client component(s) is not in the requirements list of "main".
  ```
- **Giải pháp**: Khi thêm các thư viện chuẩn ESP-IDF như `esp_http_client`, `esp_http_server` vào `main/`, phải thêm tên component vào danh sách `PRIV_REQUIRES` của `idf_component_register(...)` trong file `main/CMakeLists.txt`:
  ```cmake
  PRIV_REQUIRES
      ...
      esp_http_client
      esp_http_server
  ```

### 2.5. Xung đột môi trường Python khi chạy `idf.py flash`
- **Hiện tượng**:
  ```text
  '.../python_env/idf6.1_py3.12_env/bin/python' is currently active in the environment 
  while the project was configured with '.../bin/python3'. Run 'idf.py fullclean' to start again.
  ```
- **Giải pháp**: Thay vì chạy `idf.py flash` (làm kích hoạt kiểm tra lại biến môi trường của CMake), ta dùng lệnh **esptool nạp trực tiếp** với tốc độ cao:
  ```bash
  python3 -m esptool --chip esp32s3 -p /dev/cu.usbmodem2101 -b 460800 \
    --before default-reset --after hard-reset write-flash \
    --flash-mode dio --flash-size 16MB --flash-freq 80m \
    0x0 build/bootloader/bootloader.bin \
    0x8000 build/partition_table/partition-table.bin \
    0xd000 build/ota_data_initial.bin \
    0x20000 build/xiaozhi.bin \
    0x800000 build/generated_assets.bin
  ```

---

## 3. Thiết lập Wi-Fi Mặc Định Không Cần Hotspot / Server

Để bo mạch sau khi nạp tự động kết nối thẳng vào Wi-Fi nhà bạn (bỏ qua bước phát hotspot cấu hình AP):

1. Mở file `main/wifi_secrets.h` (file này đã được đưa vào `.gitignore`, an toàn tuyệt đối):
   ```c
   #pragma once

   #define DEFAULT_WIFI_SSID     "Ten_Wifi_Cua_Ban"
   #define DEFAULT_WIFI_PASSWORD "Mat_Khau_Wifi"
   ```
2. Mã nguồn trong `main/boards/common/wifi_board.cc` sẽ tự động đọc cấu hình này và lưu vào bộ nhớ NVS trong lần khởi động đầu tiên.

---

## 4. Quy trình Biên dịch Chuẩn cho Dự án Mới

Khi bắt đầu một ca làm việc hoặc clone repository sang máy mới:

```bash
# Bước 1: Kích hoạt môi trường
source ~/esp/esp-idf/export.sh

# Bước 2: Chuyển vào thư mục dự án
cd /Users/tonypham/MEGA/IDF/xiaozhi-esp32

# Bước 3: Chạy test kiểm tra toàn bộ matrix cấu hình
python3 -m unittest discover -s scripts/tests -v

# Bước 4: Biên dịch bo mạch mong muốn (Ví dụ M5Stack CoreS3)
python3 scripts/build.py m5stack/core-s3 --name m5stack-core-s3

# Bước 5: Nạp firmware xuống cổng kết nối USB
python3 -m esptool --chip esp32s3 -p /dev/cu.usbmodem2101 -b 460800 write-flash "@build/flash_args"

# Bước 6: Theo dõi Serial Monitor
idf.py -p /dev/cu.usbmodem2101 monitor
```
*(Thoát Serial Monitor bằng tổ hợp phím `Ctrl + ]`).*
