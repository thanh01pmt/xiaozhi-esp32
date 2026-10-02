# Tài Liệu Dự Án Xiaozhi-ESP32

Thư mục này lưu trữ toàn bộ tài liệu kiến trúc, phân tích, ý tưởng và kế hoạch thực thi của dự án **Xiaozhi-ESP32**.
Mọi quy ước về tạo, đặt tên và cấu trúc tài liệu tuân thủ nghiêm ngặt theo [Hướng Dẫn Cho Agent (AGENT.md)](./AGENT.md).

## Danh mục tài liệu

### 1. Đặc Tả Kỹ Thuật (Specs)
- [Đặc tả kiến trúc Custom Board](./specs/custom-board.md) — Hướng dẫn thêm và cấu hình bo mạch/biến thể phần cứng mới.
- [Kiểm toán kênh đầu vào AudioCodec](./specs/audio-codec-input-audit.md) — Báo cáo chuẩn hóa định dạng mic và reference cho AEC.
- [Giao thức WebSocket](./specs/websocket.md) — Đặc tả giao thức truyền thông thời gian thực và luồng âm thanh.
- [Giao thức MQTT & UDP Audio](./specs/mqtt-udp.md) — Đặc tả kênh điều khiển MQTT và truyền tải âm thanh UDP.
- [Giao thức Thiết bị MCP](./specs/mcp-protocol.md) — Đặc tả Model Context Protocol cho công cụ mở rộng AI trên thiết bị.
- [Hướng dẫn sử dụng MCP](./specs/mcp-usage.md) — Hướng dẫn tích hợp công cụ và gọi hàm MCP.
- [Quy chuẩn mã nguồn (Code Style)](./specs/code_style.md) — Quy tắc định dạng, quy ước đặt tên và quản lý mã nguồn.
- [Cấu hình mạng BluFi](./specs/blufi.md) — Đặc tả cấu hình Wi-Fi qua Bluetooth Low Energy.
- [Hệ thống âm báo (Notify)](./specs/notify.md) — Cơ chế phát âm thông báo và cảnh báo trạng thái.
- [Cơ chế Dynamic Glyph Push](./specs/glyph-push.md) — Đặc tả nạp font và glyph động cho màn hình.

### 2. Quyết Định Kiến Trúc (ADRs)

### 3. Kế Hoạch Thực Thi (Plans)
- [2026-10-02: Kế Hoạch Tối Ưu Hiệu Năng và Hoàn Thiện Tích Hợp M5Stack CoreS3](./plans/2026-10-02-m5stack-core-s3-performance-optimization-plan.md) *(Active)*
- [2026-10-02: Kế Hoạch Triển Khai Mini-App Smart Home Hub qua MCP trên M5Stack CoreS3](./plans/2026-10-02-smart-home-hub-mcp-miniapp-plan.md) *(Active)*

### 4. Phân Tích & Nghiên Cứu (Analysis)
- [2026-10-02: Đánh Giá Phần Cứng và Phân Tích Hiệu Năng Đồng Thời trên M5Stack CoreS3](./analysis/2026-10-02/2026-10-02-m5stack-core-s3-hardware-performance-audit.md) *(Final)*

### 5. Ý Tưởng (Ideas)

### 6. Bàn Giao Ca Làm Việc (Handover)
