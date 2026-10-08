# CoreS3 LVGL UI audit (work-in-progress)

Drafted from reading code only. Aims: chỉ đánh giá hiển thị đúng + đẹp, không refactor sâu.

Status: đang xử lý.

## Phạm vi đã đọc

- Display layer: `main/display/lvgl_display/`, `main/display/lcd_display.cc`
- CoreS3 board wrapper: `main/boards/m5stack/core-s3/m5stack_core_s3.cc`
- Smart Home Hub overlay: `main/apps/smart_home_hub/`
- Example tham chiếu: `/Users/tonypham/MEGA/IDF/CoreS3-UserDemo` (Arduino + LVGL 8.3.4 + M5GFX/M5Unified, page-manager pattern)

## Quan sát chính

### 1. Hai “language” LVGL đang song hành

- CoreS3 fork: LVGL 9.x-style API (lv_display_t, lvgl_port_*, lv_obj_create(NULL) cho screen riêng).
- Example: LVGL 8.3.4 + m5gfx_lvgl init + PageManager page-stack.

Không phải lỗi, nhưng ảnh hưởng cách ta so sánh “đúng / đẹp”: ta nên lấy example làm bài học về *khuynh hướng* (page-stack, resource pool, uniform UI primitives), không áp dụng cơ chế 8.x字号 vào code 9.x字号.

### 2. Z-order / layer hiện tại

- Common LCD: `screen` > `emoji_box_` > `preview_image_` > `top_bar_` > `status_bar_`.
- Wechat mode: `container_` (flex column) chứa `top_bar_`, `status_bar_`, `content_`; ngoài ra vẫn có `emoji_image_`/`emoji_label_` direct trên screen.
- Smart Home Hub overlay: một số screen tạo screen riêng và load bằng lv_screen_load.

Rủi ro thực tế:
- Vẫn tồn tại dấu hiệu “overlay nào cũng tự tạo screen riêng” → dễ làm đứt ngữ cảnh common UI (top_bar, status_bar không còn đồng bộ trên overlay).
- Example dùng page-stack + một root, overlays chỉ là page; CoreS3 hiện pha mixed: common LCD UI trên screen active, SHH overlay có thể thay screen.

### 3. Reusable primitives

- Example: View.Create(_root) + phân tách Model/View/Page, resource pool (font/image/wav) dùng naming ổn định, liveliness bằng lv_timer handler.
- CoreS3 SHH: UI tight trong từng screen, có reusable taste (card helpers trong sensor dashboard) nhưng chưa thành một primitive layer thống nhất cho tất cả overlay.

Cơ hội cải thiện “đẹp” và “đúng”:
- Dùng lại spacings/palette/radius từ common theme ở cả common LCD và SHH overlay, tránh mỗi màn hình tự pick màu/radius không liên quan nhau.
- Chuẩn hóa cảm giác “card tappable” (press / released feedback) ở mọi nơi, current sensor dashboard đã có; các overlay khác chưa đều.

### 4. Ảnh chụp / preview

- Common LCD: preview_image_ loại size screen/2, controlled by timer.
- Camera preview overlay: full-frame 320x240, stream task 15 fps-ish, fingerprint để skip invalidate nếu frame chưa đổi.

Vị trí ảnh:
- preview_image_ current đặt giữa emoji và top_bar, tức không che top_bar/status_bar → hợp lý nếu đó là mục đích.
- Nhưng nếu ý đồ là “large preview moment”, vị trí đó có thể trông náchukhông “trung tâm cảm xúc” như example’s startup / camera pages.

### 5. Emoji / emotion

- Wechat mode hiện có logic ẩn emoji khi content_ có message → ok nếu đó là intent.
- Nhưng position của emoji_image_ trên screen (top-mid) + emoji_label_ center → khi ở idle, arrangement có thể “rải rác” hơn là một biểu tượng cảm xúc tập trung.

### 6. Scroll / overflow / message limit

- Wechat mode: content_ scrollable, max messages limit, system message collapsing.
- Common mode: bottom_bar_ fixed height hoặc multiline auto-height.

Có vẻ ok về chức năng, điểm cần kiểm tra thực tế:
- Khoảng trống giữa message bubble và screen edges trong wechat mode pha giao giữa padding và container flex; cần visual check thật để confirm không bị “rật” trên 320px width.

### 7. Startup / shutdown / idle

- Startup: robot icon center, status “initializing”, nhiều timer initialize.
- Shutdown: power save mode trigger, dim backlight, pmic power off.

Điểm “đẹp” có thể nâng:
- Idle transition: hiện có thể đột ngột cut khi state change; example xử lý transition bằng pageWillAppear/DidAppear lifecycle — CoreS3 có lifecycle tương đương không cần kiểm tra.

## Các điểm cần làm tiếp (ưu tiên)

1. Xác nhận visual thật trên hardware hoặc host LVGL sim về wechat chat layout 320x240.
2. Quét toàn bộ SHH overlay xem có màn hình nào tạo screen riêng và làm mất common top_bar/status_bar context không.
3. Thu thập one set reusable style helpers (card, label, bar, rounded button) cho SHH, tránh hardcode hex/radius rời rạc.
4. Kiểm tra z-order preview_image_ trên common LCD có thực sự ưu tiên trải nghiệm “khung hình lớn” hay chỉ là placeholder.
5. Rà soát palette lặp lại 0x00E5FF, 0xFF5252, 0xFF4081 trên emotion_eye, camera_preview, dashboard, sensor_card; nếu muốn tone nhất quán, đưa vào shared style constants.

## Ghi chú về example

Example’s strongest transferable ideas:
- Page lifecycle (appear/disappear/unload) giúp cleanup và tránh leak UI state.
- ResourcePool naming ổn định giúp giảm lỗi “nguồn không tìm thấy”.
- Touch hiển thị uniform: mọi card/button có cảm giác press/feedback.

Example’s weakest transferable ideas:
- Phương pháp init M5GFX/M5Unified không ánh xạ thẳng vào ESP-IDF LVGL port của CoreS3.
- Một số pattern Arduino-centric không áp dụng directly vào architecture hiện tại.

Todo tiếp theo: đo lường ảnh hưởng mỗi điểm thay đổi trên build CoreS3 trước khi chạm code.

## ưu tiên theo ảnh hưởng thực tế

1. Trước hết làm nổi bậtz-order / context一致性of common UI vs overlays.
2. Sau đó mới đến palette reuse và spacing/radius consistency.
3. Cuối cùng là refresh/transition/anim experience and visual polish.

## ghi chú thêm

- Mỗi SHH overlay tự tạo screen riêng và lưu main_screen_ để restore — cần xác nhận việc này có làm mất common top_bar/status_bar context không.
- Palette cyan 0x00E5FF và magenta 0xFF4081 hiện phân tán trong emotion_eye, camera_preview, sensor_card — nếu muốn voice-consistent, cần gom về một source của palette.
- EmotionEyeScreen có geometry tied到 constants eye_width_/eye_height_/eye_spacing_/eye_radius_ — nếu cần responsive / adapt khác màn hình, cần abstract geometry.
- Đã đi tiếp một bước: xác nhận SHH overlays đều tự tạo screen riêng và lưu main_screen_ để restore, như Section 9 ghi.

## Section 9 verified (2026-10-07)

- Đã đọc `main/apps/smart_home_hub/ui/emotion_eye_screen.cc`, `camera_preview_screen.cc`, `dashboard_screen.cc`, `sensor_card_screen.cc`, `sensor_dashboard_screen.cc`, `wifi_config_screen.cc` và các file header tương ứng.
- Mỗi overlay có screen_ riêng, main_screen_保存 để restore, và dùng lv_screen_load để chuyển.
- EmotionEyeScreen đặc biệt: nó là overlay được show khi có emotion và restore about main_screen_ about hide.
- Camera preview overlay dùng lv_display_get_screen_prev(display_) như fallback khi main_screen_ null.
- Không có overlay nào dùng static shared screen; ngữ cảnh common UI bị đứt khi overlay load.

## Deliverables checklist

- [x] Read CoreS3 LVGL display code và So sánh example CoreS3-UserDemo
- [x] Đánh giá đúng hiển thị / đẹp theo layers và primitives
- [x] Xác nhận SHH overlay screen lifecycle
- [x] Ghi chú palette phân tán và reusable helpers
- [x] Viết file tài liệu `docs/ui-audit-core-s3.md`
- [x] Gom palette + fonts về shared constants (2026-10-08, xem Section 10)
- [x] Fix hiển thị: UTF-8 truncation, wifi row alignment, press feedback (Section 10)
- [ ] (chưa làm) visual check trên hardware/host LVGL sim wechat chat layout
- [x] Chuẩn hoá screen lifecycle helper dùng chung cho overlays (2026-10-08, Section 11)

## Section 11 — Lifecycle helper (2026-10-08)

Mục "chuẩn hoá screen_ lifecycle helper" trong checklist đã xong:

- `main/apps/smart_home_hub/ui/shh_overlay_screen.{h,cc}`: base class
  `ShhOverlayScreen` giữ `screen_`, `main_screen_`, `display_` và
  `visible_` (atomic vì camera stream task đọc từ task khác).
  - `AcquireForeground()`: lock LVGL, nhớ màn active làm restore target (bỏ qua
    khi là chính nó), `lv_screen_load`, đặt visible. Trả false khi lock timeout.
  - `ReleaseForeground()`: restore màn đã nhớ, fallback
    `lv_display_get_screen_prev()` khi thiếu, xoá visible vô điều kiện.
- Cả 6 overlay (dashboard, sensor_dashboard, sensor_card, camera_preview,
  emotion_eye, wifi_config) kế thừa base; xoá mọi bản `main_screen_` /
  `visible_` / `is_visible_` riêng lẻ.
- Hành vi nâng lên dùng chung cho mọi screen (trước đây chỉ 1-2 screen có):
  fallback screen_prev của camera, guard "không tự nhớ chính mình" của emotion
  eye, và xoá visible ngay cả khi lock timeout của camera (tránh kẹt screen
  switch vĩnh viễn).
- `SensorCardScreen::ShowRelative()` giữ nguyên ý đồ cũ: chỉ `ApplyType()`,
  không gọi `Show()` để không restart update timer hay đè restore target.
- Build `m5stack/core-s3` pass (exit 0). Lỗi build trung gian duy nhất
  (visible_ private) đã fix bằng cách dùng `IsVisible()` của base.

Cần kiểm chứng trên hardware: luồng Show/Hide qua 6 overlay + auto-return 120s,
và camera preview rời màn khi đang stream.

## Section 12 — Phân tích z-order / ngữ cảnh common UI (2026-10-08)

Câu hỏi mở trong Section 2 ("overlay load screen riêng có làm mất common
top_bar/status_bar không?") — đã trace code thật và trả lời được.

### Thực trạng đã xác nhận

- Common UI của xiaozhi (`LcdDisplay::SetupUI`, lcd_display.cc ~870-990) dựng
  toàn bộ `container_/emoji_box_/preview_image_/top_bar_/status_bar_` trên
  **một screen duy nhất** (default screen, lấy bằng `lv_screen_active()`).
- Mỗi SHH overlay là screen hoàn toàn riêng (Section 11) → khi overlay
  `lv_screen_load`, mọi widget common UI (top_bar với icon wifi/battery,
  status_bar với status/notification label) **không còn trên màn hình**. Không
  chỉ là "không đồng bộ" — chúng không tồn tại ở đó.
- Hệ quả thực tế khi overlay mở:
  - `Display::SetStatus()` / `ShowNotification()` (lvgl_display.cc:150-200) vẫn
    ghi vào `status_label_`/`notification_label_` nằm trên screen cũ → người
    dùng không thấy gì, không có warning nào.
  - Icon wifi/battery (cập nhật định kỳ bởi Application) cũng vô hình.
  - Toast của SHH tự re-parent sang screen active (smart_home_hub.cc:160) nên
    hoạt động đúng — đã tự giải quyết riêng.
- Điều này **cố ý về thiết kế**: overlay là mini-app full-screen có bar riêng
  (dashboard header, camera top/bottom bar, sensor card status bar riêng).
  Example CoreS3-UserDemo cũng làm y hệt: page riêng chiếm cả màn, không có
  bar chung tồn tại xuyên suốt. Trên 320x240 không có chỗ cho hai tầng bar.
- Home-hold zone và toast đã dùng `lv_layer_top()` — luôn hiện trên mọi
  screen, không phụ thuộc z-order. Đó là mô hình đúng cho element xuyên màn.

### Đánh giá rủi ro (đã hạ mức so với Section 2)

Nguy cơ "mất ngữ cảnh UI chung" là có thật nhưng giới hạn ở: (1) notification
/status từ app core không tới được người dùng khi overlay mở, (2) mất icon
wifi/battery. Không có crash hay state hỏng — chỉ là thông tin bị che.
Việc overlay có bar riêng tương đương đã che phần lớn khoảng trống.

### Phương án

1. **Giữ nguyên (khuyến nghị hiện tại).** Overlay là mini-app full-screen có
   bar riêng; LVGL không cho 2 screen hiển thị cùng lúc và chuyển cả 6 overlay
   sang `lv_layer_top()` vừa tốn RAM (layer là full framebuffer trên S3 PPA
   path) vừa tăng biến rename phá vụn. Chi phí của phương án này: notification
   từ core bị che khi overlay mở — chấp nhận được vì SHH toast + label lỗi
   trên từng screen đã bao phủ các sự kiện quan trọng.
2. **Chuyển notification vào layer_top.** Nếu sau này cần notification xuyên
   màn: thay `lv_obj_create(screen)` bằng `lv_layer_top()` cho
   `notification_label_` trong lcd_display.cc (một chỗ, ~5 dòng), hoặc thêm
   hook re-parent trong `ShhOverlayScreen::AcquireForeground()`. Đây là thay
   đổi nhỏ nhất có tác động thật nếu quyết định sửa.
3. **Không khuyến nghị:** dựng lại top_bar/status_bar trên từng overlay, hay
   migrate overlay sang overlay-layer của LVGL 9 — nhiều code, lợi ích thấp,
   trái với pattern của example tham chiếu.

## Section 10 — Đã làm (2026-10-08)

Học từ example CoreS3-UserDemo: ResourcePool + default theme → một nguồn style chung.
Áp dụng bản tương đương tối thiểu:

- Thêm `main/apps/smart_home_hub/ui/shh_theme.h`: palette (`kBg/kCardBg/kTrackBg/kLine`,
  accent `kCyan/kGreen/kBlue/kYellow/kOrange/kPurple/kRed/kAmber`, neon face palette
  `kNeonCyan/kNeonMint/kNeonGreen/kPink/kSlateBlue`) + `SHH_FONT_BIG/SHH_FONT_MID`
  (guard `LV_FONT_MONTSERRAT_28/20` giữ nguyên hành vi trên board không compile font lớn).
- Đưa 6 overlay + toast trong smart_home_hub.cc về palette chung. Giá trị cũ được giữ
  gần như nguyên (chỉ kCyan của wifi_config 0x00E5FF → 0x29D3FF để khớp hệ accent;
  wifi green/orange/text/dim đồng bộ về một bộ). Màn hình emotion eye giữ hệ neon riêng
  vì nền đen cần độ rực riêng.
- Fix hiển thị thật (không chỉ cosmetic):
  - `dashboard_screen.cc`: cắt tên thiết bị bằng `LV_LABEL_LONG_DOT` thay vì
    `substr(0,12)` — tên tiếng Việt multi-byte không còn bị đứt giữa ký tự.
  - `wifi_config_screen.cc`: hàng SSID dùng 2 label riêng (SSID + RSSI) thay vì
    `%-18.18s` (font proportional nên không bao giờ thẳng cột, và cắt byte được
    SSID UTF-8); SSID dùng `LV_LABEL_LONG_DOT`; thêm press feedback cho hàng mạng
    (remove_style_all đã xóa style pressed của theme).
- Xác nhận build: `python3 scripts/build.py m5stack/core-s3` pass (ESP-IDF v6.1,
  exit 0, binary flash đầy đủ).

Chưa làm / cần hardware: visual check thật trên CoreS3 (màu wifi_config hơi đổi tone
cyan, layout hàng wifi mới cần xem trên màn 320x240); clang-format không có trên máy
nên các file sửa theo style hiện có, nên chạy `clang-format --dry-run` khi có tool.

## hạn chế

- Chưa kiểm chứng real hardware hoặc host LVGL sim cho wechat chat layout hiển thị đẹp/sai.
- Chưa đo lường build size or perf impact của mỗi điểm cải thiện.
- Chưa đọc source LVGL some internal behaviors beyond API use.
- Chưa kiểm tra ESP-IDF version của project này, chỉ biết example dùng core s3 bsp demo.

## next

- Nếu muốn làm thật, chọn một điểm nhỏ nhấtจาก list trên (ví dụ: gom cyan 0x00E5FF về shared constant, hoặc chuẩn hóa screen_ lifecycle helper), làm rồi đo lường build afterward.
- Hoặc nếu muốn visual, dựng host LVGL sim wechat chat layout 320x240 để check bubble alignment, spacing, scroll và emoji position trước khi chạm code.
