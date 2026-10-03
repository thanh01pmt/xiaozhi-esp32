---
title: Bàn giao phiên làm việc - Smart Home Hub, Điều khiển Home Assistant & Tích hợp Âm thanh Tiếng Việt trên CoreS3
type: handover
status: active
created: 2026-10-04
updated: 2026-10-04
related:
  - ../plans/2026-10-02-smart-home-hub-mcp-miniapp-plan.md
---

# Bàn giao phiên làm việc - Smart Home Hub, Điều khiển Home Assistant & Tích hợp Âm thanh Tiếng Việt trên CoreS3

## 1. Bối cảnh & Mục tiêu phiên làm việc

Phiên làm việc tập trung giải quyết các bài toán thực tế trên thiết bị **M5Stack CoreS3 (ESP32-S3)**:
1. Tự động đồng bộ danh sách thực thể (Entities/Devices) từ Home Assistant về thiết bị, thay vì hardcode.
2. Hiển thị danh sách thiết bị nhà thông minh trực quan (Dashboard), tối ưu tên và icon cho kích thước màn hình nhỏ của CoreS3.
3. Giải quyết vấn đề phát nhạc/tin tức: Thay vì chỉ phát nhạc tiếng Trung từ Cloud mặc định của XiaoZhi, hỗ trợ phát trực tiếp âm thanh/radio tiếng Việt (VOV1, VOV Giao thông) và nhạc qua Home Assistant / Music Assistant (YouTube Music).
4. Thiết lập hạ tầng Docker trên Oracle VM (`140.245.127.64`) bao gồm Music Assistant, PO Token Server, và Audio Stream Gateway để phục vụ luồng stream Ogg/Opus cho thiết bị ESP32.

---

## 2. Những việc đã hoàn thành

Liệt kê chính xác những gì đã thực hiện, kèm đường dẫn file cụ thể và minh chứng kiểm chứng:

- [x] **Đồng bộ danh sách thiết bị động từ Home Assistant:**
  - File: [`main/apps/smart_home_hub/network_client.cc`](file:///Users/tonypham/MEGA/IDF/xiaozhi-esp32/main/apps/smart_home_hub/network_client.cc) & [`main/apps/smart_home_hub/smart_home_hub.cc`](file:///Users/tonypham/MEGA/IDF/xiaozhi-esp32/main/apps/smart_home_hub/smart_home_hub.cc)
  - Triển khai `FetchEntitiesFromHomeAssistant` qua HTTP REST API `GET /api/states`.
  - Tự động gọi đồng bộ ngay khi thiết bị kết nối Wi-Fi thành công trong [`main/boards/common/wifi_board.cc`](file:///Users/tonypham/MEGA/IDF/xiaozhi-esp32/main/boards/common/wifi_board.cc).
  - Đăng ký công cụ MCP `smarthome.sync_devices` cho phép ra lệnh giọng nói "Đồng bộ thiết bị từ Home Assistant".

- [x] **Tối ưu hiển thị Dashboard trên màn hình M5Stack CoreS3:**
  - File: [`main/apps/smart_home_hub/ui/dashboard_screen.cc`](file:///Users/tonypham/MEGA/IDF/xiaozhi-esp32/main/apps/smart_home_hub/ui/dashboard_screen.cc)
  - Điều chỉnh font chữ, icon và layout thẻ hiển thị phù hợp với độ phân giải màn hình 320x240.
  - Khi hỏi *"Các thiết bị trong nhà"* -> Tool `smarthome.get_device_status` tự động mở Dashboard và liệt kê trạng thái lên màn hình.

- [x] **Hạ tầng Docker trên Oracle Cloud (`140.245.127.64`):**
  - Container `music-assistant` (port 8095): Đã cài đặt và vá bypass kiểm tra tài khoản YouTube Premium (`_user_has_ytm_premium` luôn trả về `True`).
  - Container `yt-po-token` (port 4416): Đang chạy `brainicism/bgutil-ytdlp-pot-provider` cấp PO Token.
  - Container `audio-stream-gateway` (port 8096): Ứng dụng Flask + ffmpeg live transcode luồng HLS m3u8 (VOV Giao Thông, VOV1, VOV3) sang chuẩn Ogg Opus mono 16kHz tương thích phần cứng CoreS3.
  - Home Assistant Custom View: Đã tạo `custom_components/audio_gateway` trong Home Assistant map route công khai qua Cloudflare tunnel `https://ha.orchable.app/api/audio_gateway/stream`.

- [x] **Bộ công cụ MCP Điều khiển Âm thanh & Radio:**
  - File: [`main/apps/smart_home_hub/mcp_tools.cc`](file:///Users/tonypham/MEGA/IDF/xiaozhi-esp32/main/apps/smart_home_hub/mcp_tools.cc)
  - `media.play_vietnam_radio`: Phát VOV Giao thông Hà Nội, VOV Giao thông HCM, VOV1 trực tiếp ra loa thiết bị.
  - `media.play_audio_url`: Phát âm thanh trực tiếp từ URL.
  - `media.stop_audio`: Dừng phát âm thanh.
  - `media.play_home_assistant`: Gửi lệnh phát bài hát/video qua Home Assistant media_player.

- [x] **Biên dịch và nạp (flash) firmware thành công vào CoreS3:**
  - Biên dịch sạch `xiaozhi.bin` (0x2d2350 bytes, trống 28% app partition).
  - Nạp thành công qua cổng `/dev/cu.usbmodem2101`.

---

- [x] **Tích hợp YouTube Music & Giải pháp vượt cơ chế chặn Bot (PO-Token + Deno):**
  - Đã tích hợp `ytmusicapi` và module giải mã thử thách JS (`deno` + `bgutil-ytdlp-pot-provider` + cookie Netscape) vào container `audio-stream-gateway` trên Oracle VM.
  - Kiểm tra thực tế xác nhận luồng stream âm thanh YouTube Music được phân giải trực tiếp và live-transcode thành chuẩn Ogg Opus mono 16kHz trả về qua endpoint `https://ha.orchable.app/api/audio_gateway/stream?q=<tên_bài>`.
  - Đã thêm công cụ MCP thiết bị: `media.play_youtube` vào [`main/apps/smart_home_hub/mcp_tools.cc`](file:///Users/tonypham/MEGA/IDF/xiaozhi-esp32/main/apps/smart_home_hub/mcp_tools.cc) với prompt ưu tiên cao để khi người dùng yêu cầu bài hát cụ thể, Cloud AI sẽ gọi trực tiếp tool này thay vì tool mặc định `search_music` của Cloud XiaoZhi.

---

## 3. Trạng thái hiện tại & Việc đang dang dở

Những việc đang làm dở, chưa hoàn tất hoặc còn cần làm tiếp trong phiên sau:

- [ ] **Nạp bản build mới xuống CoreS3:**
  - Firmware đã được biên dịch thành công (`xiaozhi.bin` bao gồm MCP tool `media.play_youtube`).
  - Cần đưa CoreS3 về Download Mode (giữ nút nguồn bên hông khoảng 3 giây hoặc cắm lại cáp Type-C) để nạp bản firmware mới vào thiết bị qua cổng `/dev/cu.usbmodem2101`.
- [ ] **Thử nghiệm giọng nói thực tế:**
  - Ra lệnh: *"Bật bài Lạc Trôi Sơn Tùng"* hoặc *"Bật bài Cơn Mưa Ngang Qua"* để xác nhận thiết bị kích hoạt `media.play_youtube` và phát nhạc trực tiếp ra loa.

---

## 4. Lưu ý kỹ thuật & Nợ kỹ thuật (Gotchas & Technical Debt)

- **Định dạng âm thanh gốc của đài FM:** Tất cả các đài VOV hiện tại đã dừng luồng mp3/ogg tĩnh và chuyển sang luồng phân đoạn HLS `.m3u8` (ví dụ `https://play.vovgiaothong.vn/live/gthn/playlist.m3u8`). ESP32 không thể tải trực tiếp file m3u8 nếu không có bộ đệm HLS client. Vì vậy luồng bắt buộc phải đi qua `audio-stream-gateway` để chuyển thành Ogg Opus chunked stream.
- **Bypass YouTube Premium trong Music Assistant:** Mỗi khi container `music-assistant` pull image mới, logic `_user_has_ytm_premium` sẽ bị reset về mặc định yêu cầu Premium. Cần lưu ý chạy lại sed patch nếu recreate container.
- **Bảo mật Cloudflare Ingress:** Endpoint stream của `audio_gateway` đặt tại `https://ha.orchable.app/api/audio_gateway/stream`, tận dụng route công khai qua Cloudflare Tunnel không cần mở port firewall công khai trực tiếp.

---

## 5. Hướng dẫn hành động cho Agent kế tiếp (Next Actions)

Các bước tuần tự cụ thể mà Agent phiên kế tiếp cần bắt tay vào làm ngay:

1. **Bước 1**: Mở Serial Monitor của CoreS3 (`./docs/setup/flash_cores3.sh /dev/cu.usbmodem2101 monitor`) để theo dõi khi người dùng ra lệnh "Bật đài VOV Giao Thông".
2. **Bước 2**: Xác nhận xem `NotifyPlayer` có nhận được gói tin Ogg từ `https://ha.orchable.app/api/audio_gateway/stream` và giải mã ra I2S codec thành công không.
3. **Bước 3**: Nếu người dùng muốn nghe nhạc YouTube trực tiếp trên loa CoreS3: kết nối `audio-stream-gateway` vào luồng output của Music Assistant server (`http://music-assistant:8095`).

---

## 6. Lệnh kiểm tra & xác minh (Verification Commands)

```bash
# Kiểm tra stream Ogg Opus từ gateway
curl -s "https://ha.orchable.app/api/audio_gateway/stream?station=vov_giaothong" | head -c 100 | xxd

# Monitor log thiết bị CoreS3
./docs/setup/flash_cores3.sh /dev/cu.usbmodem2101 monitor

# Kiểm tra log container gateway trên Oracle VM
ssh -i /Users/tonypham/MEGA/WebApp/the-second-brain/Secrets/oracle-advanced-compute/ssh-key-2026-05-29.key ubuntu@140.245.127.64 'docker logs --tail 30 audio-stream-gateway'
```
