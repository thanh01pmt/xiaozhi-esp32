# docs/ — Hướng dẫn cho Agent

Tài liệu này quy định **cách đặt, đặt tên và viết** mọi tài liệu trong `docs/`. Agent PHẢI đọc file này trước khi tạo hoặc sửa bất kỳ file nào trong thư mục này.

## 1. Bản đồ thư mục

| Thư mục     | Chứa gì                                                   | Câu hỏi nó trả lời                | Tính chất                     |
| ----------- | --------------------------------------------------------- | --------------------------------- | ----------------------------- |
| `specs/`    | Đặc tả kỹ thuật, giao thức, kiến trúc board, code style  | "Hệ thống/ngoại vi/giao thức này hoạt động theo chuẩn kỹ thuật nào?" | Sống, cập nhật theo chuẩn hệ thống |
| `adrs/`     | Architecture Decision Record — các quyết định đã chốt     | "Chúng ta đã chọn gì, và vì sao?" | Bất biến sau khi `accepted`   |
| `analysis/` | Phân tích, nghiên cứu, so sánh, khảo sát dựa trên dữ kiện | "Ta biết gì về vấn đề này?"       | Là ảnh chụp tại một thời điểm |
| `ideas/`    | Ý tưởng thô, chưa cam kết thực hiện                       | "Nếu ta thử X thì sao?"           | Nhẹ, cho phép chưa chín       |
| `plans/`    | Kế hoạch thực thi cụ thể, có bước và tiêu chí xong        | "Làm thế nào và theo thứ tự nào?" | Sống, cập nhật trạng thái     |
| `handover/` | Bàn giao phiên làm việc, tiến độ dở dang và bước tiếp theo| "Phiên trước làm tới đâu, làm gì tiếp?" | Ảnh chụp chuyển giao phiên làm việc |

## 2. Chọn thư mục nào?

Đi theo thứ tự, dừng ở câu trả lời "có" đầu tiên:

1. Đây là **đặc tả kỹ thuật, giao thức mạng/âm thanh, kiến trúc board, hay chuẩn code**? → `specs/`
2. Đây là **quyết định đã chốt** (chọn A thay vì B, có lý do và hệ quả)? → `adrs/`
3. Đây là **kế hoạch làm việc** (có các bước, thứ tự, người/việc, tiêu chí hoàn thành)? → `plans/`
4. Đây là **phân tích dựa trên dữ kiện** (so sánh, đánh giá, nghiên cứu, kết quả khảo sát)? → `analysis/`
5. Đây là **ý tưởng chưa được kiểm chứng hay cam kết**? → `ideas/`
6. Đây là **bàn giao phiên làm việc** (tóm tắt kết quả, việc đã xong, việc dở dang, nợ kỹ thuật, hướng dẫn cho Agent kế tiếp)? → `handover/`

Nếu một nội dung thuộc nhiều loại, **tách thành nhiều file** và liên kết chéo, không gộp chung.
Nếu không chắc, ưu tiên `ideas/` và ghi rõ điều chưa chắc trong file.

## 3. Quy ước chung

### Đặt tên file và cấu trúc thư mục

- `kebab-case`, chữ thường, tiếng Anh không dấu, ngắn gọn, mô tả nội dung (không đặt `notes.md`, `final-v2.md`).
- `specs/`: `<kebab-case-slug>.md` — tên chủ đề đặc tả kỹ thuật (hoặc hậu tố `_zh.md` cho tài liệu song ngữ).
- `adrs/`: `NNNN-slug.md` — `NNNN` là số thứ tự 4 chữ số tăng dần. Liệt kê thư mục để lấy số kế tiếp, không được đoán, không được dùng lại số.
- `analysis/`, `ideas/`, `plans/`: `YYYY-MM-DD-slug.md` — ngày là **ngày tạo** và không đổi khi cập nhật. Với `analysis/`, cho phép nhóm theo thư mục con ngày `analysis/YYYY-MM-DD/YYYY-MM-DD-slug.md`.
- `handover/`: **Chia thư mục con theo ngày**: `handover/YYYY-MM-DD/YYYY-MM-DD-slug.md` (hoặc `handover/YYYY-MM-DD/slug.md`) — giúp gom gọn các phiên bàn giao theo từng ca/ngày làm việc.

### Front matter (bắt buộc ở mọi file)

```yaml
---
title: Tiêu đề ngắn gọn
type: adr | analysis | idea | plan | handover | spec
status: <xem bảng trạng thái bên dưới>
created: YYYY-MM-DD
updated: YYYY-MM-DD
related: [] # đường dẫn tương đối tới các doc liên quan
---
```

### Trạng thái hợp lệ

| Loại     | Giá trị `status`                                      |
| -------- | ----------------------------------------------------- |
| spec     | `draft` → `active` → `deprecated`                     |
| adr      | `proposed` → `accepted` → `superseded` / `deprecated` |
| analysis | `draft` → `final` → `outdated`                        |
| idea     | `seed` → `exploring` → `promoted` / `dropped`         |
| plan     | `draft` → `active` → `done` / `cancelled`             |
| handover | `active` → `archived`                                 |

### Ngôn ngữ và định dạng

- Nội dung viết bằng **tiếng Việt**; giữ nguyên thuật ngữ kỹ thuật tiếng Anh, tên riêng, tên biến, đường dẫn, lệnh.
- Mỗi file có đúng một `# H1` (trùng `title`), sau đó dùng `##`, `###`.
- Dùng đường dẫn tương đối khi liên kết: `[ADR-0003](../adrs/0003-use-postgres.md)`.
- Khối code luôn ghi rõ ngôn ngữ (` ```ts `, ` ```bash `).
- Ngày dùng định dạng ISO `YYYY-MM-DD`.

## 4. Template theo loại

### `specs/` — Đặc tả kỹ thuật

```markdown
---
title: <Tên đặc tả kỹ thuật>
type: spec
status: active
created: YYYY-MM-DD
updated: YYYY-MM-DD
related: []
---

# <Tên đặc tả kỹ thuật>

## 1. Tổng quan & Phạm vi

Mô tả vai trò, mục tiêu và phạm vi kỹ thuật của tài liệu đặc tả.

## 2. Kiến trúc & Nguyên lý hoạt động

Mô tả cơ chế hoạt động, luồng xử lý hoặc cấu trúc phần cứng/phần mềm.

## 3. Đặc tả chi tiết

Trình bày rõ ràng: giao diện API, sơ đồ chân GPIO, thông số bus, định dạng gói tin/payload hoặc quy chuẩn mã nguồn.

## 4. Ràng buộc & Tương thích

Các ràng buộc về phần cứng, bộ nhớ, phiên bản ESP-IDF, hoặc tính tương thích giữa các biến thể board.
```

Quy tắc riêng:

- Là tài liệu tham chiếu chuẩn xác (living spec), cập nhật khi kiến trúc phần cứng, giao thức hoặc quy chuẩn dự án phát triển.
- Giữ vững tính trung thực và khả năng kiểm chứng đối chiếu với mã nguồn thực tế.

### `adrs/` — Quyết định

```markdown
---
title: <Quyết định, viết dạng khẳng định>
type: adr
status: proposed
created: YYYY-MM-DD
updated: YYYY-MM-DD
related: []
---

# <Quyết định>

## Bối cảnh

Vấn đề đang gặp, ràng buộc, lực đẩy. Không nêu giải pháp ở đây.

## Các phương án đã cân nhắc

1. **Phương án A** — ưu / nhược
2. **Phương án B** — ưu / nhược

## Quyết định

Chọn phương án nào, viết rõ ràng một câu.

## Lý do

Vì sao phương án này thắng trong bối cảnh trên.

## Hệ quả

- Tích cực:
- Tiêu cực / đánh đổi:
- Việc cần làm tiếp theo (liên kết tới `plans/` nếu có):
```

Quy tắc riêng:

- Một ADR = một quyết định.
- Sau khi `accepted`, **không sửa nội dung quyết định**. Muốn đổi, tạo ADR mới, đặt ADR cũ thành `superseded` và ghi `Superseded by: [ADR-XXXX](...)` ở đầu file cũ.
- Chỉ được sửa lỗi chính tả, link hỏng, hoặc cập nhật `status`.

### `analysis/` — Phân tích

```markdown
---
title: <Chủ đề phân tích>
type: analysis
status: draft
created: YYYY-MM-DD
updated: YYYY-MM-DD
related: []
---

# <Chủ đề phân tích>

## Câu hỏi cần trả lời

Một hoặc vài câu hỏi cụ thể.

## Tóm tắt kết luận

3–5 dòng, đọc xong là nắm được.

## Phương pháp và nguồn dữ liệu

Dữ liệu lấy từ đâu, thời điểm nào, giới hạn gì.

## Phát hiện

Trình bày dữ kiện trước, diễn giải sau. Tách rõ **sự thật** và **suy luận**.

## Khuyến nghị / Bước tiếp theo

Nếu dẫn tới quyết định → gợi ý tạo ADR. Nếu dẫn tới hành động → gợi ý tạo plan.
```

Quy tắc riêng:

- Mọi con số, khẳng định phải có nguồn hoặc cách kiểm chứng. Không bịa số liệu.
- Ghi rõ ngày, vì analysis có hạn sử dụng. Khi lỗi thời, đổi `status: outdated` thay vì xóa.

### `ideas/` — Ý tưởng

```markdown
---
title: <Tên ý tưởng>
type: idea
status: seed
created: YYYY-MM-DD
updated: YYYY-MM-DD
related: []
---

# <Tên ý tưởng>

## Ý tưởng trong một câu

## Vấn đề / cơ hội nó nhắm tới

## Hình dung sơ bộ

Nó hoạt động thế nào, ai dùng.

## Giả định cần kiểm chứng

- [ ] Giả định 1
- [ ] Giả định 2

## Rủi ro / câu hỏi mở

## Bước nhỏ nhất để thử
```

Quy tắc riêng:

- Cho phép chưa hoàn chỉnh, nhưng phải nói rõ **chưa biết gì**.
- Khi ý tưởng được kiểm chứng: đặt `status: promoted` và liên kết tới analysis/plan/ADR sinh ra từ nó. Khi bỏ: đặt `dropped` kèm lý do một dòng, không xóa file.

### `plans/` — Kế hoạch

```markdown
---
title: <Mục tiêu của kế hoạch>
type: plan
status: draft
created: YYYY-MM-DD
updated: YYYY-MM-DD
related: []
---

# <Mục tiêu của kế hoạch>

## Mục tiêu và phạm vi

Làm gì, **không** làm gì.

## Tiêu chí hoàn thành

- [ ] Tiêu chí kiểm tra được 1
- [ ] Tiêu chí kiểm tra được 2

## Căn cứ

Liên kết tới ADR / analysis / idea nền tảng.

## Các bước

1. [ ] Bước 1 — đầu ra cụ thể
2. [ ] Bước 2 — phụ thuộc bước 1

## Rủi ro và cách xử lý

## Nhật ký cập nhật

- YYYY-MM-DD: <thay đổi trạng thái hoặc điều chỉnh quan trọng>
```

Quy tắc riêng:

- Mỗi bước phải có đầu ra kiểm tra được; tránh bước mơ hồ kiểu "nghiên cứu thêm".
- Tick checkbox và ghi nhật ký khi tiến triển; cập nhật `updated`.
- Plan không chứa quyết định kiến trúc mới. Nếu phát sinh, tạo ADR rồi liên kết.

### `handover/` — Bàn giao phiên làm việc

```markdown
---
title: <Tên bàn giao - ngắn gọn>
type: handover
status: active
created: YYYY-MM-DD
updated: YYYY-MM-DD
related: []
---

# <Tên bàn giao>

## 1. Bối cảnh & Mục tiêu phiên làm việc

Mục tiêu của phiên làm việc vừa qua là gì, xuất phát từ yêu cầu hay bối cảnh nào.

## 2. Những việc đã hoàn thành

Liệt kê chính xác những gì đã thực hiện, kèm đường dẫn file cụ thể và minh chứng kiểm chứng (test, log, typecheck):
- [x] Việc 1: mô tả kết quả, file liên quan `file/path.ts`
- [x] Việc 2: ...

## 3. Trạng thái hiện tại & Việc đang dang dở

Những việc đang làm dở, chưa hoàn tất hoặc còn cần làm tiếp trong phiên sau:
- [ ] Việc dở dang 1: hiện trạng đến đâu, còn thiếu gì để xong
- [ ] Việc dở dang 2: ...

## 4. Lưu ý kỹ thuật & Nợ kỹ thuật (Gotchas & Technical Debt)

- Các điểm bẫy kỹ thuật, lỗi lint/typecheck chưa giải quyết, thông số cấu hình, cổng kết nối, biến môi trường.
- Các cam kết hoặc ràng buộc không được vi phạm (ví dụ: cấm dùng emoji UI, chỉ dùng NineRouter gateway).

## 5. Hướng dẫn hành động cho Agent kế tiếp (Next Actions)

Các bước tuần tự cụ thể mà Agent phiên kế tiếp cần bắt tay vào làm ngay:
1. Bước 1: ...
2. Bước 2: ...

## 6. Lệnh kiểm tra & xác minh (Verification Commands)

Các câu lệnh terminal dùng để kiểm tra tính toàn vẹn (typecheck, lint, test, benchmark):
```bash
npm run typecheck
```

## 7. File & Tài liệu Tham Chiếu Chính (Key References)

Bảng hoặc danh sách link markdown tương đối tới các file mã nguồn, template và tài liệu liên quan trực tiếp:
- [`path/to/source.ts`](../relative/path/to/source.ts) — Mô tả vai trò của file trong phiên
- [`docs/refs/templates/TEMPLATE.md`](../relative/docs/refs/templates/TEMPLATE.md) — Hợp đồng mẫu áp dụng
```

Quy tắc riêng:

- **Bắt buộc ghi link các file tham chiếu chính**: File bàn giao PHẢI liệt kê đường dẫn tương đối (dạng link markdown có thể click được `[filename](path)`) tới tất cả các file mã nguồn, file template và tài liệu kiến trúc liên quan được phân tích/sửa đổi trong phiên làm việc. Không được chỉ nhắc tên file trơ trọi mà không kèm link.
- Phải trung thực tuyệt đối về tình trạng mã nguồn ("File IS State"): việc gì chưa xong thì ghi rõ chưa xong, không báo cáo khống.
- Mỗi khi có phiên làm việc lớn hoặc kết thúc ca làm việc phức tạp, tạo một file bàn giao mới.
- Khi một file handover mới được tạo và bao quát toàn bộ nội dung của phiên cũ, cập nhật file cũ thành `status: archived`.

## 5. Vòng đời tài liệu

```
idea ──► analysis ──► adr ──► plan
 (thử)    (hiểu)     (chốt)   (làm)
   ▲        ▲          ▲        ▲
   └────────┴──────────┴────────┴──► handover (bàn giao trạng thái giữa các ca)
```

- Không bắt buộc đi đủ chuỗi; một quyết định nhỏ có thể đi thẳng vào ADR.
- Khi chuyển giai đoạn, doc sau **liên kết ngược** doc trước qua `related`, và doc trước cập nhật `status`/`related` trỏ tới doc sau.
- `handover/` đóng vai trò cầu nối xuyên suốt (session bridge), ghi nhận ảnh chụp trạng thái thực tế của dự án để Agent tiếp theo tiếp nhận ngữ cảnh ngay lập tức mà không bị mất mát thông tin.

## 6. Nguyên tắc viết

- **Đọc một mình vẫn hiểu**: người mở file mà không có ngữ cảnh cuộc trò chuyện vẫn phải nắm được. Không viết "như đã nói ở trên", "theo yêu cầu của bạn".
- **Kết luận trước, chi tiết sau**: phần tóm tắt nằm đầu.
- **Ngắn mà đủ**: mỗi doc phục vụ một mục đích. Cắt phần lan man.
- **Phân biệt rõ** sự thật, suy luận, giả định, ý kiến.
- **Không bịa**: thiếu thông tin thì ghi "Chưa rõ" hoặc "Cần xác nhận", không suy diễn thành sự thật.
- Ghi lý do, không chỉ ghi kết quả.

## 7. Không được làm

- Không tạo thư mục mới trong `docs/` khi chưa có yêu cầu rõ ràng (ngoại trừ các thư mục hợp lệ: `specs/`, `adrs/`, `analysis/`, `ideas/`, `plans/`, `handover/` và các thư mục con theo ngày trong `handover/YYYY-MM-DD/` hoặc `analysis/YYYY-MM-DD/`).
- Không đặt file ở gốc `docs/` (trừ README này).
- Không xóa doc cũ; đổi `status` (`superseded`, `outdated`, `dropped`, `cancelled`).
- Không sửa nội dung ADR đã `accepted`.
- Không copy nguyên văn tài liệu bên ngoài; tóm tắt và dẫn nguồn.
- Không để lại `TODO`, placeholder `<...>` chưa điền trong file đã chuyển khỏi `draft`/`seed`/`proposed`.

## 8. Checklist trước khi lưu

- [ ] Đúng thư mục theo mục 2?
- [ ] Tên file đúng quy ước, số ADR không trùng?
- [ ] Front matter đầy đủ, `status` thuộc giá trị hợp lệ?
- [ ] Có H1 trùng `title` và đủ các mục của template?
- [ ] Link tương đối trỏ tới file có thật?
- [ ] Đã cập nhật `related`/`status` của doc liên quan?
- [ ] Không còn placeholder, không có số liệu thiếu nguồn?
