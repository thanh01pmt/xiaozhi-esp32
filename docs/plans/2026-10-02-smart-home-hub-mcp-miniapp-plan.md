---
title: Kế Hoạch Triển Khai Mini-App Smart Home Hub qua MCP trên M5Stack CoreS3
type: plan
status: active
created: 2026-10-02
updated: 2026-10-02
related:
  - ./2026-10-02-m5stack-core-s3-performance-optimization-plan.md
  - ../analysis/2026-10-02/2026-10-02-m5stack-core-s3-hardware-performance-audit.md
  - ../specs/mcp-protocol.md
  - ../specs/mcp-usage.md
  - ../specs/custom-board.md
---

# Kế Hoạch Triển Khai Mini-App Smart Home Hub qua MCP trên M5Stack CoreS3

## Mục tiêu và phạm vi

### Mục tiêu
Xây dựng một Mini-App **Smart Home Hub** hoàn chỉnh chạy trực tiếp trên **M5Stack CoreS3** song song với trợ lý ảo XiaoZhi, khai thác toàn bộ tiềm năng phần cứng (Màn hình cảm ứng Touch, Giọng nói AI, Web local, Bluetooth LE, Wi-Fi) để biến thiết bị thành một **Bộ điều khiển trung tâm nhà thông minh bằng giọng nói và màn hình cảm ứng**.

### Kiến trúc chủ đạo (Phương án 1: MCP + Tách biệt Module)
- **Tận dụng tối đa pipeline cốt lõi**: Giữ nguyên toàn bộ chu trình xử lý âm thanh, giải mã Opus, Wake Word và WebSocket/MQTT của XiaoZhi.
- **Tương tác AI thông qua Model Context Protocol (MCP)**: Thiết bị đăng ký các công cụ điều khiển IoT với `McpServer`. Khi người dùng ra lệnh bằng giọng nói tự nhiên, AI Backend sẽ tự động phát hiện ý định và gửi lệnh `tools/call` JSON-RPC về thiết bị thực thi.
- **Tách biệt và không xâm lấn (Non-intrusive Architecture)**:
  - Toàn bộ logic ứng dụng Mini-App được đặt trong thư mục độc lập `main/apps/smart_home_hub/`.
  - Mọi can thiệp vào Application đều qua cơ chế lập lịch `Application::GetInstance().Schedule(...)`.
  - Có thể bật/tắt toàn bộ Mini-App qua menuconfig (`CONFIG_ENABLE_SMART_HOME_HUB`).

### Phạm vi thực hiện
- **Làm**:
  - Thiết kế kiến trúc module Mini-App `smart_home_hub` và tích hợp vào chuỗi build của CMake.
  - Xây dựng hệ thống công cụ MCP (`smarthome.set_switch`, `smarthome.set_level`, `smarthome.trigger_scene`, `smarthome.get_device_status`).
  - Xây dựng giao diện màn hình cảm ứng Dashboard LVGL 9 (Device Cards, Switch, Sliders) và cơ chế chuyển đổi màn hình mượt mà (Gesture Swipe / Auto-timeout quay về XiaoZhi Emoji).
  - Tích hợp HTTP REST Client giao tiếp với Home Assistant API và các thiết bị IoT Local qua Wi-Fi.
  - Tích hợp Bluetooth Low Energy (BLE Central) để quét và điều khiển thiết bị BLE trong phòng (công tắc, đèn thông minh, cảm biến nhiệt ẩm).
  - Xây dựng Web Server cấu hình nội bộ (`esp_http_server`) trên cổng 80 cho phép điện thoại/máy tính truy cập vào cài đặt token, IP Home Assistant và danh sách thiết bị.
  - Kiểm thử, định dạng mã nguồn và biên dịch cho board `m5stack/core-s3`.
- **Không làm**:
  - Không sửa đổi cấu trúc lõi của `main/application.cc`, `main/audio/` hay các giao thức `protocols/`.
  - Không gán cứng cấu hình địa chỉ IP / Token bảo mật vào mã nguồn (phải lưu trữ an toàn trong NVS Flash).
  - Không can thiệp hoặc gây ảnh hưởng tới các board phần cứng khác trong repository.

## Tiêu chí hoàn thành

- [x] Cấu trúc thư mục module `main/apps/smart_home_hub/` được khởi tạo hoàn chỉnh, tích hợp vào `main/CMakeLists.txt` và `main/Kconfig.projbuild`.
- [x] Khởi tạo thành công bộ công cụ MCP trong `McpServer::GetInstance()`, kiểm tra qua phản hồi `tools/list` của giao thức MCP JSON-RPC.
- [x] Màn hình cảm ứng FT6336U cho phép chạm giữ >= 600ms hoặc lệnh MCP để hoán đổi giữa màn hình Biểu cảm XiaoZhi và Màn hình Smart Home Dashboard.
- [x] Màn hình Dashboard tự động quay lại màn hình chính sau 30 giây nếu người dùng không thao tác chạm.
- [x] Gửi lệnh điều khiển thành công qua HTTP REST tới Home Assistant / Local IoT endpoint khi có lệnh gọi từ MCP hoặc thao tác nút bấm trên màn hình.
- [x] Trình quản lý BLE (BLE Central) khởi động và cấu trúc sẵn sàng không gây xung đột tài nguyên RF với Wi-Fi.
- [x] Cung cấp giao diện Web cấu hình nội bộ trên cổng 80 qua Wi-Fi Local, hỗ trợ xem trạng thái thiết bị và cập nhật cài đặt.
- [x] Toàn bộ 98 unit tests của repo chạy thành công, không xung đột bất kỳ cấu hình hay board nào khác.

## Căn cứ

- Báo cáo phân tích phần cứng & hiệu năng CoreS3: [`docs/analysis/2026-10-02/2026-10-02-m5stack-core-s3-hardware-performance-audit.md`](../analysis/2026-10-02/2026-10-02-m5stack-core-s3-hardware-performance-audit.md).
- Kế hoạch tối ưu nền tảng CoreS3: [`docs/plans/2026-10-02-m5stack-core-s3-performance-optimization-plan.md`](./2026-10-02-m5stack-core-s3-performance-optimization-plan.md).
- Đặc tả giao thức MCP Thiết bị: [`docs/specs/mcp-protocol.md`](../specs/mcp-protocol.md).
- Hướng dẫn triển khai IoT Control qua MCP: [`docs/specs/mcp-usage.md`](../specs/mcp-usage.md).
- Hướng dẫn phát triển Custom Board: [`docs/specs/custom-board.md`](../specs/custom-board.md).

## Các bước thực hiện

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                      LỘ TRÌNH TRIỂN KHAI 6 BƯỚC                            │
├─────────────────────────────────────────────────────────────────────────────┤
│  Bước 1: Khởi tạo Cấu trúc Module & Hợp đồng Dữ liệu (Scaffolding)          │
│                                    ▼                                        │
│  Bước 2: Xây dựng Bộ Công Cụ Trợ Lý Giọng Nói AI (MCP Tools Provider)       │
│                                    ▼                                        │
│  Bước 3: Xây dựng Giao Diện Cảm Ứng Dashboard LVGL (Dual-screen Switcher)   │
│                                    ▼                                        │
│  Bước 4: Tích hợp Kết Nối Mạng (Home Assistant REST Client & Local Web API) │
│                                    ▼                                        │
│  Bước 5: Tích hợp Kết Nối Không Dây Bluetooth Low Energy (BLE Central)      │
│                                    ▼                                        │
│  Bước 6: Tích hợp vào Board CoreS3, Kconfig & Kiểm thử Toàn diện            │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

### Bước 1: Khởi tạo Cấu trúc Module & Hợp đồng Dữ liệu (Scaffolding)
1. Tạo cấu trúc thư mục mới:
   ```text
   main/apps/smart_home_hub/
   ├── CMakeLists.txt                 # Khai báo file nguồn của app
   ├── smart_home_hub.h               # Singleton điều phối trung tâm
   ├── smart_home_hub.cc
   ├── device_model.h                 # Định nghĩa thực thể thiết bị (Switch, Light, Climate)
   ├── mcp_tools.h                    # Khai báo công cụ MCP
   ├── mcp_tools.cc
   ├── ble_controller.h               # Quản lý Bluetooth LE
   ├── ble_controller.cc
   ├── network_client.h               # Client HTTP REST / Home Assistant
   ├── network_client.cc
   ├── web_config_server.h            # HTTP Web server cấu hình nội bộ
   ├── web_config_server.cc
   └── ui/
       ├── dashboard_screen.h         # Giao diện LVGL 9 Smart Home
       └── dashboard_screen.cc
   ```
2. Cập nhật `main/CMakeLists.txt`:
   - Bổ sung `main/apps/smart_home_hub` vào đường dẫn INCLUDE_DIRS và danh sách SOURCES (được bảo vệ bằng cờ `CONFIG_ENABLE_SMART_HOME_HUB`).
3. Cập nhật `main/Kconfig.projbuild`:
   - Thêm menu `Smart Home Hub Configuration`:
     - `config ENABLE_SMART_HOME_HUB`: boolean, mặc định `n`.
     - `config SMARTHOME_HOMEASSISTANT_URL`: string, địa chỉ IP Home Assistant mặc định.
     - `config SMARTHOME_ENABLE_BLE`: boolean, bật/tắt module BLE để tiết kiệm RAM khi không cần.
- **Đầu ra**: Cấu trúc module sạch sẽ, độc lập, có thể bật tắt hoàn toàn bằng Kconfig.

---

### Bước 2: Xây dựng Bộ Công Cụ Trợ Lý Giọng Nói AI (MCP Tools Provider)
1. Triển khai lớp `SmartHomeMcpTools`:
   - Kết nối với `McpServer::GetInstance()`.
2. Đăng ký các công cụ tiêu chuẩn theo đặc tả [`mcp-usage.md`](../specs/mcp-usage.md):
   - **`smarthome.set_switch`**:
     - *Mô tả*: Bật hoặc tắt công tắc đèn, quạt, ổ cắm thông minh trong nhà.
     - *Tham số*: `device_id` (string), `state` (boolean).
     - *Hành vi*: Gửi lệnh xuống thiết bị đích qua HTTP hoặc BLE; cập nhật trạng thái trên màn hình LVGL; trả về kết quả thành công cho AI.
   - **`smarthome.set_level`**:
     - *Mô tả*: Điều chỉnh mức độ sáng đèn (0-100%) hoặc cài đặt nhiệt độ máy lạnh (16-30°C).
     - *Tham số*: `device_id` (string), `level` (integer, min 0, max 100).
   - **`smarthome.trigger_scene`**:
     - *Mô tả*: Kích hoạt một ngữ cảnh tự động hóa (ví dụ: "về nhà", "đi ngủ", "xem phim").
     - *Tham số*: `scene_name` (string).
   - **`smarthome.get_device_status`**:
     - *Mô tả*: Lấy thông tin trạng thái hoạt động của thiết bị hoặc cảm biến (nhiệt độ, độ ẩm hiện tại).
     - *Trả về*: Chuỗi JSON chứa trạng thái thời gian thực.
3. Đảm bảo an toàn luồng:
   - Các lệnh thay đổi trạng thái UI hoặc phát âm thanh phản hồi được bọc qua `Application::GetInstance().Schedule(...)`.
- **Đầu ra**: Trợ lý ảo có khả năng nhận lệnh ngôn ngữ tự nhiên từ người dùng và tự động gọi đúng hàm C++ để điều khiển thiết bị.

---

### Bước 3: Xây dựng Giao Diện Cảm Ứng Dashboard LVGL (Dual-screen Switcher)
1. Triển khai `DashboardScreen`:
   - Xây dựng layout tối ưu cho độ phân giải 320x240 trên M5Stack CoreS3:
     - **Header Bar**: Hiển thị thời gian thực, icon Wi-Fi, trạng thái BLE, icon pin AXP2101.
     - **Quick Control Grid**: Các thẻ thiết bị dạng lưới (2 cột x 2 hàng):
       - Card Đèn: Icon bóng đèn, nút switch bật/tắt nhanh, chạm giữ mở slider chỉnh độ sáng.
       - Card Máy lạnh: Hiển thị nhiệt độ hiện tại, nút tăng/giảm (+ / -).
       - Card Cảm biến: Hiển thị nhiệt độ / độ ẩm phòng khách.
       - Card Kịch bản: Nút bấm kích hoạt kịch bản nhanh (Rời nhà / Về nhà).
     - **Footer Bar**: Nút cảm ứng "Back to Voice Assistant" hoặc vuốt ngang để chuyển màn hình.
2. Triển khai cơ chế Dual-screen Switcher:
   - Lưu trữ con trỏ màn hình chính của XiaoZhi (`lv_scr_act()`) và màn hình `smart_home_screen_`.
   - Bắt sự kiện cử chỉ cảm ứng (LVGL Gesture Event `LV_EVENT_GESTURE`):
     - Vuốt sang trái từ màn hình Emoji → Chuyển sang Smart Home Dashboard (`lv_screen_load_anim` với hiệu ứng trượt 200ms).
     - Vuốt sang phải từ Dashboard → Quay về màn hình Emoji XiaoZhi.
   - Quản lý Timer tự động quay về (Auto-idle timeout):
     - Mỗi khi có tương tác chạm trên Dashboard, reset bộ đếm thời gian (30 giây).
     - Sau 30 giây không có thao tác chạm, tự động kích hoạt hiệu ứng trượt quay về màn hình biểu cảm XiaoZhi.
- **Đầu ra**: Người dùng có thể vừa ngắm biểu cảm sống động của XiaoZhi, vừa có thể vuốt tay sang bảng điều khiển cảm ứng trực quan bất cứ lúc nào.

---

### Bước 4: Tích hợp Kết Nối Mạng (Home Assistant REST Client & Local Web API)
1. Triển khai `NetworkClient`:
   - Sử dụng thư viện `esp_http_client` của ESP-IDF.
   - Hỗ trợ gọi API REST tiêu chuẩn của Home Assistant (`/api/services/...`):
     ```bash
     POST /api/services/light/turn_on
     Header: Authorization: Bearer <LONG_LIVED_ACCESS_TOKEN>
     Body: {"entity_id": "light.living_room"}
     ```
   - Hỗ trợ gửi lệnh tới các firmware IoT mã nguồn mở phổ biến trong mạng nội bộ (Tasmota / ESPHome / Shelly HTTP API).
2. Triển khai `WebConfigServer`:
   - Khởi tạo một HTTP Web Server nhẹ (`esp_http_server`) trên Port 80 khi thiết bị kết nối Wi-Fi.
   - Cung cấp trang Web HTML/JS tối giản (lưu trong flash SPIFFS hoặc nhúng gzip trong mã nguồn):
     - Cho phép người dùng nhập Home Assistant URL và Access Token.
     - Cho phép cấu hình danh sách 4–8 thiết bị ưa thích hiển thị trên màn hình CoreS3.
     - Lưu trữ toàn bộ thông tin cấu hình vào phân vùng `NVS` để không bị mất khi khởi động lại.
- **Đầu ra**: Khả năng điều khiển thiết bị thông minh qua mạng Wi-Fi Local với độ trễ siêu thấp (< 100ms) và giao diện Web cấu hình tiện lợi từ smartphone.

---

### Bước 5: Tích hợp Kết Nối Không Dây Bluetooth Low Energy (BLE Central)
1. Triển khai `BleController`:
   - Sử dụng ESP-IDF NimBLE stack (tiết kiệm RAM vượt trội so với Bluedroid).
   - Thiết lập vai trò BLE Central / GAP Observer:
     - Quét (Scan) định kỳ các thiết bị phát quảng cáo (Advertising packets) trong phòng.
     - Nhận diện dữ liệu cảm biến BLE môi trường (ví dụ: cảm biến nhiệt độ độ ẩm Xiaomi Mijia BLE / Tuya BLE Beacon) mà không cần kết nối (Connectionless listening).
     - Hỗ trợ kết nối GATT Client tới các bóng đèn/công tắc BLE Mesh hoặc BLE Custom để gửi lệnh điều khiển on/off qua Characteristic Write.
2. Quản lý chia sẻ tài nguyên không dây (Wi-Fi / Bluetooth Coexistence):
   - Bật chế độ `CONFIG_ESP_COEX_SW_COEXISTENCE_ENABLE=y` trong sdkconfig để đảm bảo việc quét BLE và stream âm thanh WebSocket qua Wi-Fi không gây rớt gói âm thanh.
- **Đầu ra**: Mở rộng khả năng điều khiển các thiết bị Bluetooth cục bộ độc lập với Internet.

---

### Bước 6: Tích hợp vào Board CoreS3, Kconfig & Kiểm thử Toàn diện
1. Tích hợp điểm móc (Hook) trong [`main/boards/m5stack/core-s3/m5stack_core_s3.cc`](../../../main/boards/m5stack/core-s3/m5stack_core_s3.cc):
   - Trong hàm `InitializeTools()` của board: Khởi tạo singleton `SmartHomeHub::GetInstance().Initialize()`.
   - Kết nối sự kiện cảm ứng và hiển thị của CoreS3 với `DashboardScreen`.
2. Kiểm tra tài nguyên và độ ổn định:
   - Đo lường mức tiêu thụ bộ nhớ heap: Đảm bảo toàn bộ cấu trúc dữ liệu thiết bị và buffer web được cấp phát trên PSRAM (Octal SPIRAM).
   - Kiểm tra hiện tượng gián đoạn âm thanh khi vừa stream hội thoại vừa gửi lệnh HTTP/BLE.
3. Kiểm thử biên dịch:
   - Chạy kiểm thử định dạng và biên dịch sạch với:
     ```bash
     python3 scripts/build.py m5stack/core-s3 --name m5stack-core-s3
     ```
   - Chạy 98 host test:
     ```bash
     python3 -m unittest discover -s scripts/tests -v
     ```
- **Đầu ra**: Bản build firmware hoàn chỉnh, hợp nhất hoàn hảo giữa Trợ lý ảo XiaoZhi và Smart Home Hub mà không phá vỡ bất kỳ thành phần nào của dự án gốc.

---

## Bố trí Quản lý Bộ nhớ & Tránh Xung đột Tài nguyên

| Tài nguyên | Nguy cơ tiềm ẩn | Giải pháp kỹ thuật bảo vệ |
| :--- | :--- | :--- |
| **Internal SRAM** | BLE Stack + HTTP Client làm cạn kiệt RAM nội | Cấu hình NimBLE thay vì Bluedroid; cấp phát toàn bộ bộ đệm Web Server, JSON parse và Device Cache trên **Octal PSRAM** (`heap_caps_malloc(..., MALLOC_CAP_SPIRAM)`). |
| **I2C Bus** | Màn hình cảm ứng, PMIC và Codec tranh chấp bus | Toàn bộ lệnh gửi HTTP/BLE chạy trên FreeRTOS worker task riêng biệt, không block I2C bus hoặc main loop. |
| **RF Coexistence (Wi-Fi + BLE)** | Audio stream qua Wi-Fi bị giật khi BLE quét | Kích hoạt Software Coexistence của ESP32-S3; chỉ bật quét BLE thụ động (Passive Scan) với duty cycle thấp (10% scan window). |
| **LVGL Rendering** | Vẽ giao diện Dashboard xung đột với Emoji Animation | Chuyển đổi màn hình thông qua cơ chế quản lý màn hình chuẩn của LVGL (`lv_screen_load_anim`), khóa và mở khóa mutex LVGL qua `esp_lvgl_port_lock()` và `esp_lvgl_port_unlock()`. |
| **Thread-Safety** | Lệnh từ Web hoặc BLE gọi sai task ngữ cảnh | Toàn bộ thay đổi trạng thái của XiaoZhi bắt buộc phải lập lịch qua `Application::GetInstance().Schedule(...)`. |

---

## Rủi ro và Cách xử lý

| Rủi ro | Mức độ | Biện pháp xử lý |
| :--- | :---: | :--- |
| **Tràn bộ nhớ khi bật đồng thời BLE và Wi-Fi** | Trung bình | Tận dụng 8MB Octal PSRAM vừa tối ưu; cho phép tắt cờ `CONFIG_SMARTHOME_ENABLE_BLE` qua Kconfig nếu chỉ sử dụng Wi-Fi Home Assistant. |
| **Thời gian phản hồi HTTP kéo dài làm đơ UI** | Trung bình | Tuyệt đối không gọi HTTP request đồng bộ trên main loop hoặc LVGL task; thực thi trong một hàng đợi lệnh (Command Queue) với FreeRTOS Task chạy ngầm độ ưu tiên thấp. |
| **Xung đột tên công cụ MCP với công cụ gốc** | Thấp | Toàn bộ công cụ của Mini-App sử dụng tiền tố namespace thống nhất: `smarthome.*` (ví dụ `smarthome.set_switch`). |
| **Mất cấu hình khi nạp OTA firmware mới** | Thấp | Lưu toàn bộ token, IP và danh sách thiết bị trong phân vùng `nvs` (phân vùng này được giữ nguyên vẹn qua các chu kỳ OTA). |

---

## Nhật ký cập nhật

- **2026-10-02**: Khởi tạo bản kế hoạch chi tiết triển khai Mini-App Smart Home Hub theo Phương án 1 (MCP + Module độc lập) cho M5Stack CoreS3 ở trạng thái `draft`.
