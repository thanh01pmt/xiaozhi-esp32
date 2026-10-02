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

# Hướng Dẫn Cài Đặt Môi Trường, Script Tự Động và Khắc Phục Sự Cố

Tài liệu này tổng hợp toàn bộ các lưu ý về thiết lập công cụ, script tự động hoá nạp firmware, môi trường build, sự cố tương thích phiên bản ESP-IDF v6.1 và giải pháp cho phần cứng M5Stack CoreS3.

---

## 1. Sử Dụng Script Tự Động Hoá 1 Click (Recommended)

Để không phải gõ nhiều lệnh dài phức tạp, dự án đã có sẵn script tự động:
📁 `docs/setup/flash_cores3.sh`

```bash
# Nạp firmware rồi tự kiểm tra cảm biến (mặc định cổng /dev/cu.usbmodem2101)
./docs/setup/flash_cores3.sh

# Hoặc chỉ định rõ cổng USB và hành động:
./docs/setup/flash_cores3.sh /dev/cu.usbmodem2101 all     # Build + Flash + Verify
./docs/setup/flash_cores3.sh /dev/cu.usbmodem2101 build   # Chỉ Build
./docs/setup/flash_cores3.sh /dev/cu.usbmodem2101 flash   # Chỉ Flash
./docs/setup/flash_cores3.sh /dev/cu.usbmodem2101 verify  # Chỉ kiểm tra cảm biến (không cần build lại)
./docs/setup/flash_cores3.sh /dev/cu.usbmodem2101 monitor # Chỉ mở Monitor

# Đổi ngôn ngữ (mặc định vi-VN)
LANGUAGE=en-US ./docs/setup/flash_cores3.sh

# Cho phép chờ lâu hơn khi máy khởi động chậm (mặc định 25 giây)
VERIFY_TIMEOUT=60 ./docs/setup/flash_cores3.sh /dev/cu.usbmodem2101 verify
```
*(Khi xem monitor, nhấn `Ctrl + ]` để thoát).*

`all` **không** tự mở monitor vì monitor chạy ở chế độ tương tác và sẽ nuốt mất log mà bước verify cần đọc. Chạy `monitor` riêng nếu cần theo dõi thủ công.

### Bước `verify` làm gì

Sau khi nạp (hoặc khi chạy riêng), script dùng `esptool` đặt máy về chế độ chạy, mở cổng serial ở 115200 và dò dòng mà driver in ra:

```
==========================================================
 Kết quả khởi tạo cảm biến
----------------------------------------------------------
 OK    PMIC (AXP2101)       : yes
 OK    LTR-553ALS (light)   : yes
 FAIL  BMI270 (imu)         : no
----------------------------------------------------------
  • SensorMonitor: BMI270 not found at 0x69
==========================================================
```

Script trả về exit code khác 0 nếu bất kỳ cảm biến nào không lên, nên dùng được trong CI. Nếu không thấy dòng `SensorMonitor ready` trong `VERIFY_TIMEOUT` giây, script in ra 20 dòng log cuối để dò.

Script build bằng lệnh này — ngôn ngữ là **tham số lúc build**, không phải option của board:

```bash
python3 scripts/build.py m5stack/core-s3 --name m5stack-core-s3 --language vi-VN
```

Liệt kê ngôn ngữ / model từ khoá thức dậy hợp lệ:

```bash
python3 scripts/build.py --list-languages
python3 scripts/build.py --list-wake-words
```

---

## 2. Yêu cầu Môi trường & Công cụ Bắt buộc

Dự án XiaoZhi yêu cầu tối thiểu **ESP-IDF v6.0.1**, khuyến nghị dùng **ESP-IDF v6.1** (IDF 5.x không còn được hỗ trợ).

### A. Cài đặt các công cụ hệ thống (Host Tooling trên macOS)
```bash
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

### D. Thiết lập IDE / clangd (loại bỏ lỗi giả)
```bash
# Sinh database lệnh biên dịch (tức thì, không build lại, không đụng CMake cache)
ninja -C build -t compdb > build/compile_commands.json
```

File `.clangd` ở thư mục gốc đã trỏ sẵn clangd tới `build/compile_commands.json` (file này được `.gitignore` bỏ qua nên chỉ có tác dụng trên máy của bạn).

Trong VS Code, **clangd phải là bản của Espressif**, không phải bản clangd mặc định — chỉ bản này hiểu `--target=xtensa-esp32s3-elf`, `-mlongcalls` và specs picolibc:

```jsonc
// .vscode/settings.json
{ "clangd.path": "~/.espressif/tools/esp-clangd/esp-21.1.3_20260408/esp-clangd/bin/clangd" }
```

**Sai dấu hiệu:** nếu clangd báo `Unknown argument '-mlongcalls'`, `'sys/features.h' file not found`, `No type named 'string' in namespace 'std'`, hoặc gợi ý hàm LVGL 8 đã bị xoá (ví dụ `lv_chart_set_series_ext_y_array`) thì clangd **chưa** đọc được `compile_commands.json` — nghĩa là nó đang đoán mò include path. Chạy lại lệnh `ninja -C build -t compdb ...` sau khi đổi `config.json` hoặc `CMakeLists.txt`.

---

## 3. Các Vấn đề Kỹ thuật Đã Xử Lý (Troubleshooting & Hardware Gotchas)

### 3.1. Lỗi PSRAM trên M5Stack CoreS3 (`PSRAM chip is not connected, or wrong PSRAM line mode`)
- **Hiện tượng**:
  ```text
  E (33) octal_psram: PSRAM chip is not connected, or wrong PSRAM line mode
  E cpu_start: Failed to init external RAM!
  abort() was called at PC 0x420057dd on core 0
  ```
- **Nguyên nhân**: M5Stack CoreS3 (bản tiêu chuẩn) sử dụng chế độ giao tiếp **Quad SPI PSRAM (4-line SPI)**. Khi cấu hình `CONFIG_SPIRAM_MODE_OCT=y` (8-line OPI) hoặc ép xung 80MHz, MSPI timing calibration bị lỗi (`MSPI Timing: tuning fail`), gây crash bootloader liên tục (bootloop).
- **Giải pháp**: Cấu hình trong `main/boards/m5stack/core-s3/config.json`:
  ```json
  "sdkconfig_append": [
      "CONFIG_SPIRAM=y",
      "CONFIG_SPIRAM_MODE_QUAD=y",
      "CONFIG_CAMERA_GC0308=y"
  ]
  ```

### 3.2. Lần build đầu tiên bị treo / mất 20–30 phút
- **Hiện tượng**: Lệnh dừng rất lâu ở bước `NOTE: Processing 74 dependencies: [1/74] ... [65/74] lvgl/lvgl ...`.
- **Nguyên nhân**: Dự án dùng **74 managed components** (LVGL 9, ESP-SR AI models ~50MB, esp32-camera, codec...). Lần đầu tiên build, IDF Component Manager phải tải toàn bộ 74 gói này về `~/.espressif/component_cache`.
- **Giải pháp**: Không tắt terminal. Sau khi tải xong lần đầu, file `dependencies.lock` được sinh ra. **Từ lần build thứ 2 trở đi, bước này chỉ mất 0 giây**.

### 3.3. Lỗi tương thích API GDMA giữa ESP-IDF v6.1 và `78__uart-uhci`
- **Hiện tượng**: `error: 'gdma_get_alignment_constraints' was not declared in this scope`.
- **Giải pháp**: Cập nhật hàm gọi `gdma_get_channel_alignment_constraints` trong `managed_components/78__uart-uhci/src/uart_uhci.cc`.

### 3.4. Lỗi thứ tự Designated Initializer trong C++20 (`-std=gnu++26`)
- **Hiện tượng**: `error: designator order for field 'esp_lcd_panel_io_i2c_config_t::scl_speed_hz' does not match declaration order`.
- **Giải pháp**: Gán tường minh các trường theo thứ tự khai báo trong `esp_lcd_io_i2c.h` hoặc khởi tạo rỗng `{}` trước khi gán.

### 3.5. Khai báo component phụ thuộc vào `PRIV_REQUIRES`
- **Hiện tượng**: `Compilation failed because network_client.cc includes esp_http_client.h...`.
- **Giải pháp**: Thêm `esp_http_client` và `esp_http_server` vào `PRIV_REQUIRES` của `main/CMakeLists.txt`.

### 3.6. Lỗi nạp flash bằng `flash_args` (`No such file or directory: bootloader/bootloader.bin`)
- **Hiện tượng**: Chạy `python3 -m esptool ... @build/flash_args` từ thư mục gốc bị báo không tìm thấy file.
- **Giải pháp**: `flash_args` được sinh ra cho ngữ cảnh bên trong thư mục `build/`. Khi nạp từ thư mục gốc, phải trỏ đường dẫn đầy đủ:
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

### 3.7. Firmware bị mất tiếng Việt sau khi build (`Fail: test_language_and_wake_word_are_not_board_config_options`)
- **Hiện tượng**: `python3 -m unittest discover -s scripts/tests` báo fail ở test trên, hoặc firmware nạp lên nói tiếng Trung.
- **Nguyên nhân**: Có `CONFIG_LANGUAGE_VI_VN=y` nằm trong `sdkconfig_append` của `main/boards/m5stack/core-s3/config.json`. Repo coi ngôn ngữ và model từ khoá thức dậy là **tham số build**, không phải option của board, nên test cấm chúng ở `config.json`.
- **Giải pháp**: Xoá dòng `CONFIG_LANGUAGE_*` khỏi `config.json`, build bằng cờ `--language` (xem mục 1). Khi đổi ngôn ngữ, nhớ build lại chứ chỉ flash lại binary cũ sẽ không có tác dụng.

---

## 4. Thiết lập Wi-Fi Mặc Định Không Cần Hotspot / Server

Để bo mạch sau khi nạp tự động kết nối thẳng vào Wi-Fi nhà bạn (bỏ qua bước phát hotspot cấu hình AP):

1. Mở file `main/wifi_secrets.h` (file này đã được đưa vào `.gitignore`, an toàn tuyệt đối):
   ```c
   #pragma once

   #define DEFAULT_WIFI_SSID     "Ten_Wifi_Cua_Ban"
   #define DEFAULT_WIFI_PASSWORD "Mat_Khau_Wifi"
   ```
2. Mã nguồn trong `main/boards/common/wifi_board.cc` sẽ tự động đọc cấu hình này và lưu vào bộ nhớ NVS trong lần khởi động đầu tiên.
