#include "mcp_tools.h"
#include "smart_home_hub.h"
#include "sensor_monitor.h"
#include "application.h"
#include <esp_log.h>

#define TAG "SH_McpTools"

void SmartHomeMcpTools::RegisterTools(SmartHomeHub* hub) {
    auto& mcp = McpServer::GetInstance();

    // 1. Tool bat/tat cong tac thiet bi
    mcp.AddTool("smarthome.set_switch",
        "Bật hoặc tắt công tắc đèn, quạt, ổ cắm hoặc thiết bị thông minh.\n"
        "device_id: tên hoặc id thiết bị (ví dụ: 'den_phong_khach', 'quat', 'light.living_room')\n"
        "state: true để bật, false để tắt.",
        PropertyList({
            Property("device_id", kPropertyTypeString),
            Property("state", kPropertyTypeBoolean)
        }),
        [hub](const PropertyList& props) -> ReturnValue {
            std::string device_id = props["device_id"].value<std::string>();
            bool state = props["state"].value<bool>();
            ESP_LOGI(TAG, "MCP Tool set_switch: %s -> %d", device_id.c_str(), state);
            bool success = hub->SetDeviceState(device_id, state);
            return success;
        });

    // 2. Tool dieu chinh do sang hoac nhiet do
    mcp.AddTool("smarthome.set_level",
        "Điều chỉnh độ sáng đèn (0-100%) hoặc cài đặt nhiệt độ điều hòa (16-30 độ C).\n"
        "device_id: id thiết bị\n"
        "level: giá trị số cần đặt",
        PropertyList({
            Property("device_id", kPropertyTypeString),
            Property("level", kPropertyTypeInteger, 0, 100)
        }),
        [hub](const PropertyList& props) -> ReturnValue {
            std::string device_id = props["device_id"].value<std::string>();
            int level = props["level"].value<int>();
            ESP_LOGI(TAG, "MCP Tool set_level: %s -> %d", device_id.c_str(), level);
            bool success = hub->SetDeviceLevel(device_id, level);
            return success;
        });

    // 3. Tool kich hoat kich ban
    mcp.AddTool("smarthome.trigger_scene",
        "Kích hoạt một kịch bản hoặc ngữ cảnh nhà thông minh (ví dụ: 've_nha', 'di_ngu', 'xem_phim').",
        PropertyList({
            Property("scene_name", kPropertyTypeString)
        }),
        [hub](const PropertyList& props) -> ReturnValue {
            std::string scene = props["scene_name"].value<std::string>();
            ESP_LOGI(TAG, "MCP Tool trigger_scene: %s", scene.c_str());
            bool success = hub->TriggerScene(scene);
            return success;
        });

    // 4. Tool lay danh sach va trang thai thiet bi (dong thoi hien thi len man hinh)
    mcp.AddTool("smarthome.get_device_status",
        "Lấy danh sách và trạng thái hiện tại của tất cả các thiết bị trong nhà thông minh, đồng thời tự động mở giao diện điều khiển lên màn hình CoreS3.",
        PropertyList(),
        [hub](const PropertyList& props) -> ReturnValue {
            Application::GetInstance().Schedule([hub]() {
                hub->ShowDashboard();
            });
            return hub->GetDeviceStatusJson();
        });

    // 4b. Tool dong bo thiet bi tu Home Assistant
    mcp.AddTool("smarthome.sync_devices",
        "Đồng bộ danh sách tất cả các thiết bị và công tắc từ máy chủ Home Assistant về thiết bị.",
        PropertyList(),
        [hub](const PropertyList& props) -> ReturnValue {
            bool ok = hub->SyncDevicesFromHomeAssistant();
            return ok ? "Đã đồng bộ thành công danh sách thiết bị từ Home Assistant!" : "Đồng bộ thất bại, vui lòng kiểm tra kết nối Home Assistant.";
        });

    // 5. Tool hien thi man hinh dieu khien Dashboard tren man hinh CoreS3
    mcp.AddTool("smarthome.show_dashboard",
        "Hiển thị bảng điều khiển nhà thông minh lên màn hình cảm ứng của thiết bị.",
        PropertyList(),
        [hub](const PropertyList& props) -> ReturnValue {
            Application::GetInstance().Schedule([hub]() {
                hub->ShowDashboard();
            });
            return true;
        });

    // 6. Tool liet ke danh sach tat ca man hinh co the mo tren thiet bi
    mcp.AddTool("ui.list_screens",
        "Liệt kê tất cả các màn hình có thể chuyển đổi trên thiết bị: eyes (màn hình biểu cảm cặp mắt robot AI), main/chat (trợ lý AI chính dạng văn bản), sensors (bảng tổng quan cảm biến), sáu màn hình đơn cho từng cảm biến (temperature, battery, light, motion, network, system), smarthome (điều khiển nhà thông minh) và camera (xem trước camera).",
        PropertyList(),
        [hub](const PropertyList& props) -> ReturnValue {
            return hub->ListScreensJson();
        });

    // 7. Tool chuyen doi man hinh theo yeu cau cua nguoi dung (co do tre cho LLM san sang)
    mcp.AddTool("ui.switch_screen",
        "Chuyển đổi giao diện màn hình trên thiết bị theo yêu cầu.\n"
        "Trong lúc chờ LLM suy nghĩ, màn hình sẽ giữ animation biểu cảm mắt (Thinking), và sẽ tự động chuyển sang màn hình được chọn khi bạn bắt đầu phản hồi (hoặc tối đa sau 2 giây).\n"
        "screen_name: Tên màn hình cần chuyển:\n"
        "  - 'eyes': màn hình biểu cảm cặp mắt robot (Kawaii/Robot Eyes).\n"
        "  - 'main' (hoặc 'chat'): màn hình chat/trợ lý dạng văn bản.\n"
        "  - 'temperature', 'battery', 'light', 'motion', 'network', 'system': màn hình đơn của một cảm biến.\n"
        "  - 'sensors': bảng tổng quan tất cả cảm biến.\n"
        "  - 'smarthome': bảng điều khiển nhà thông minh.\n"
        "  - 'camera': xem trước camera trực tiếp.",
        PropertyList({
            Property("screen_name", kPropertyTypeString)
        }),
        [hub](const PropertyList& props) -> ReturnValue {
            std::string name = props["screen_name"].value<std::string>();
            ESP_LOGI(TAG, "MCP Tool ui.switch_screen: %s (scheduled deferred switch)", name.c_str());
            Application::GetInstance().Schedule([hub, name]() {
                hub->ScheduleScreenSwitch(name, 2000);
            });
            return true;
        });

    // 7b. Tool thiet lap man hinh mac dinh (Default View: Eye Animation hoac Chat View)
    mcp.AddTool("ui.set_default_view",
        "Thiết lập màn hình nền mặc định cho thiết bị theo yêu cầu của người dùng bằng giọng nói.\n"
        "Dùng khi người dùng nói: 'đặt màn hình mặc định là mắt/eye animation', 'đặt màn hình chờ là khuôn mặt', 'đổi màn hình mặc định sang chat/chữ', 'cài đặt màn hình nền'.\n"
        "view_mode: 'eyes' để đặt màn hình biểu cảm mắt làm mặc định; 'chat' để đặt giao diện chat/trợ lý dạng chữ làm mặc định.",
        PropertyList({
            Property("view_mode", kPropertyTypeString)
        }),
        [hub](const PropertyList& props) -> ReturnValue {
            std::string mode_str = props["view_mode"].value<std::string>();
            ESP_LOGI(TAG, "MCP Tool ui.set_default_view: %s", mode_str.c_str());
            bool is_eyes = (mode_str == "eyes" || mode_str == "eye" || mode_str == "mat" || mode_str == "bieu_cam" || mode_str == "kawaii");
            Application::GetInstance().Schedule([hub, is_eyes]() {
                hub->SetDefaultScreenMode(is_eyes ? DefaultScreenMode::Eyes : DefaultScreenMode::Chat);
                hub->ReturnToDefaultScreen();
            });
            return is_eyes ? "Đã cài đặt màn hình mặc định là biểu cảm mắt (Eye Animation View)!"
                           : "Đã cài đặt màn hình mặc định là màn hình hội thoại (Chat View)!";
        });

    // 8. Tool doc toan bo cam bien tren M5Stack CoreS3 (Grounding cho AI)
    mcp.AddTool("sensor.get_all_sensors",
        "Đọc toàn bộ số liệu cảm biến phần cứng của M5Stack CoreS3: Pin, sạc, nhiệt độ bo mạch, ánh sáng môi trường (Lux/Proximity - LTR-553ALS), cảm biến chuyển động & tư thế máy (Gia tốc/Con quay hồi chuyển 6 trục - BMI270), sóng Wi-Fi (RSSI, IP), dung lượng RAM và thời gian hoạt động.",
        PropertyList(),
        [](const PropertyList& props) -> ReturnValue {
            return SensorMonitor::GetInstance().GetAllSensorsJson();
        });

    // 9. Tool doc chuyen sau tung loai cam bien
    mcp.AddTool("sensor.get_sensor_data",
        "Đọc thông số chi tiết của một loại cảm biến cụ thể trên thiết bị và đồng thời hiển thị thẻ thông số nổi bật của cảm biến đó lên màn hình.\n"
        "Dùng tool này khi người dùng hỏi về một cảm biến cụ thể, ví dụ 'nhiệt độ bao nhiêu', 'pin còn bao nhiêu', 'ánh sáng có sáng không'.\n"
        "sensor_type: Loại cảm biến cần đọc ('temperature', 'battery', 'light', 'motion', 'network', 'system').",
        PropertyList({
            Property("sensor_type", kPropertyTypeString)
        }),
        [hub](const PropertyList& props) -> ReturnValue {
            std::string type = props["sensor_type"].value<std::string>();
            ESP_LOGI(TAG, "MCP Tool sensor.get_sensor_data: %s", type.c_str());

            // Lên lịch chuyển sang thẻ cảm biến đó sau tối đa 2s (hoặc khi bắt đầu nói câu trả lời)
            Application::GetInstance().Schedule([hub, type]() {
                hub->ScheduleScreenSwitch(type, 2000);
            });

            return SensorMonitor::GetInstance().GetSensorDataJson(type);
        });

    // 10. Tool phat nhac YouTube / YouTube Music truc tiep tren loa thiet bi
    mcp.AddTool("media.play_youtube",
        "Tìm kiếm và phát bài hát từ YouTube / YouTube Music trực tiếp trên loa thiết bị ở mức âm lượng 80%.\n"
        "BẮT BUỘC gọi công cụ này khi người dùng yêu cầu: 'bật bài [tên bài]', 'nghe bài [tên bài]', 'tìm bài [tên bài]', 'phát nhạc [tên bài]', 'mở nhạc [tên bài]', 'nghe ca khúc [tên bài]'.\n"
        "query: Tên bài hát, ca khúc hoặc kèm ca sĩ (ví dụ: 'Lạc Trôi', 'Cơn mưa ngang qua Sơn Tùng', 'Nơi này có anh', 'Shape of You').",
        PropertyList({
            Property("query", kPropertyTypeString)
        }),
        [hub](const PropertyList& props) -> ReturnValue {
            std::string q = props["query"].value<std::string>();
            ESP_LOGI(TAG, "MCP Tool media.play_youtube: %s", q.c_str());

            // URL encode query
            std::string encoded_q = "";
            char hex_buf[4];
            for (char c : q) {
                if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
                    encoded_q += c;
                } else if (c == ' ') {
                    encoded_q += "+";
                } else {
                    snprintf(hex_buf, sizeof(hex_buf), "%%%02X", (unsigned char)c);
                    encoded_q += hex_buf;
                }
            }

            std::string url = "https://ha.orchable.app/api/audio_gateway/stream?q=" + encoded_q;
            std::string title = "YouTube: " + q;
            bool ok = hub->PlayAudioStream(url, title);
            return ok ? ("Đang tìm kiếm và phát bài hát '" + q + "' từ YouTube Music") : "Không thể phát nhạc từ YouTube Music";
        });

    // 11. Tool phat kenh Radio / Am nhac tieng Viet
    mcp.AddTool("media.play_vietnam_radio",
        "Phát các kênh Radio / Tin tức / Âm nhạc trực tuyến tiếng Việt (VOV) trực tiếp trên loa thiết bị.\n"
        "Gọi công cụ này khi người dùng yêu cầu 'nghe radio', 'bật đài', 'bật VOV', 'nghe VOV giao thông'.\n"
        "station: Tên kênh cần nghe:\n"
        "  - 'vov_giaothong': VOV Giao thông Hà Nội (Tin giao thông, ca nhạc Việt Nam, tin tức)\n"
        "  - 'vov_giaothong_hcm': VOV Giao thông TP.HCM (Ca nhạc, thông tin đô thị)\n"
        "  - 'vov1': VOV1 Thời sự - Chính trị tổng hợp\n"
        "  - 'vov3': VOV3 Âm nhạc & Giải trí",
        PropertyList({
            Property("station", kPropertyTypeString)
        }),
        [hub](const PropertyList& props) -> ReturnValue {
            std::string st = props["station"].value<std::string>();
            ESP_LOGI(TAG, "MCP Tool media.play_vietnam_radio: %s", st.c_str());

            std::string url;
            std::string title;
            if (st == "vov_giaothong" || st == "giaothong" || st == "giao_thong" || st == "hn") {
                url = "https://ha.orchable.app/api/audio_gateway/stream?station=vov_giaothong";
                title = "VOV Giao Thông Hà Nội";
            } else if (st == "vov_giaothong_hcm" || st == "hcm" || st == "sai_gon") {
                url = "https://ha.orchable.app/api/audio_gateway/stream?station=vov_giaothong_hcm";
                title = "VOV Giao Thông TP.HCM";
            } else if (st == "vov1" || st == "thoisu" || st == "thoi_su") {
                url = "https://ha.orchable.app/api/audio_gateway/stream?station=vov1";
                title = "VOV1 - Thời sự";
            } else {
                url = "https://ha.orchable.app/api/audio_gateway/stream?station=vov_giaothong";
                title = "VOV Giao Thông";
            }

            bool ok = hub->PlayAudioStream(url, title);
            return ok ? ("Đang phát " + title) : "Không thể kết nối đến luồng phát thanh";
        });

    // 11. Tool phat am thanh / podcast qua URL truc tiep
    mcp.AddTool("media.play_audio_url",
        "Phát một luồng âm thanh hoặc podcast từ đường dẫn URL (OGG/Opus) trực tiếp ra loa thiết bị.\n"
        "url: Đường dẫn âm thanh http/https\n"
        "title: Tên bài hát hoặc tiêu đề âm thanh",
        PropertyList({
            Property("url", kPropertyTypeString),
            Property("title", kPropertyTypeString)
        }),
        [hub](const PropertyList& props) -> ReturnValue {
            std::string url = props["url"].value<std::string>();
            std::string title = props["title"].value<std::string>();
            ESP_LOGI(TAG, "MCP Tool media.play_audio_url: %s - %s", title.c_str(), url.c_str());
            bool ok = hub->PlayAudioStream(url, title);
            return ok ? ("Bắt đầu phát: " + title) : "Lỗi khi mở luồng âm thanh";
        });

    // 12. Tool dung phat am thanh
    mcp.AddTool("media.stop_audio",
        "Dừng phát âm thanh, radio hoặc podcast đang chạy trên thiết bị hoặc Home Assistant.",
        PropertyList(),
        [hub](const PropertyList& props) -> ReturnValue {
            hub->StopAudioStream();
            return "Đã dừng phát âm thanh.";
        });

    // 13. Tool phat nhac qua Home Assistant (YouTube / Spotify / Media Player)
    mcp.AddTool("media.play_home_assistant",
        "Gửi lệnh phát nhạc, bài hát hoặc video YouTube qua máy chủ Home Assistant (đến Media Player / Loa thông minh trong nhà).\n"
        "entity_id: Id thiết bị media player trên HA (ví dụ: 'media_player.living_room_speaker', 'media_player.music_assistant')\n"
        "media_url: Đường dẫn YouTube, URL nhạc, hoặc ID bài hát cần phát\n"
        "media_type: Loại media ('music', 'audio/mp3', 'video/youtube')",
        PropertyList({
            Property("entity_id", kPropertyTypeString),
            Property("media_url", kPropertyTypeString),
            Property("media_type", kPropertyTypeString)
        }),
        [hub](const PropertyList& props) -> ReturnValue {
            std::string entity_id = props["entity_id"].value<std::string>();
            std::string media_url = props["media_url"].value<std::string>();
            std::string media_type = props["media_type"].value<std::string>();
            ESP_LOGI(TAG, "MCP Tool media.play_home_assistant: %s -> %s", entity_id.c_str(), media_url.c_str());
            bool ok = hub->PlayHomeAssistantMedia(entity_id, media_url, media_type);
            return ok ? ("Đã gửi lệnh phát sang Home Assistant: " + entity_id) : "Lỗi khi gửi lệnh sang Home Assistant";
        });

    ESP_LOGI(TAG, "Smart Home, UI Navigation, Sensor & Media MCP tools successfully registered");
}
