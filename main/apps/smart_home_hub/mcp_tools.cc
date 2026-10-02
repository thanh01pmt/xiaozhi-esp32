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

    // 4. Tool lay trang thai thiet bi
    mcp.AddTool("smarthome.get_device_status",
        "Lấy danh sách và trạng thái hiện tại của các thiết bị trong nhà thông minh.",
        PropertyList(),
        [hub](const PropertyList& props) -> ReturnValue {
            return hub->GetDeviceStatusJson();
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
        "Liệt kê tất cả các màn hình có thể chuyển đổi trên thiết bị (main: Trợ lý AI chính, sensors: Thông số cảm biến & phần cứng, smarthome: Điều khiển nhà thông minh, camera: Chụp ảnh hoặc quan sát camera).",
        PropertyList(),
        [hub](const PropertyList& props) -> ReturnValue {
            return hub->ListScreensJson();
        });

    // 7. Tool chuyen doi man hinh theo yeu cau cua nguoi dung
    mcp.AddTool("ui.switch_screen",
        "Chuyển đổi giao diện màn hình trên thiết bị theo yêu cầu. Dùng 'main' để quay lại màn hình trợ lý chính.\n"
        "screen_name: Tên màn hình cần chuyển ('main', 'sensors', 'smarthome', hoặc 'camera').",
        PropertyList({
            Property("screen_name", kPropertyTypeString)
        }),
        [hub](const PropertyList& props) -> ReturnValue {
            std::string name = props["screen_name"].value<std::string>();
            ESP_LOGI(TAG, "MCP Tool ui.switch_screen: %s", name.c_str());
            // Validate up front so an unknown name is reported instead of a silent no-op.
            static const char* kScreens[] = {"main", "sensors", "smarthome", "camera"};
            bool known = false;
            for (const char* id : kScreens) {
                if (name == id) {
                    known = true;
                    break;
                }
            }
            if (!known) {
                ESP_LOGW(TAG, "ui.switch_screen: unknown screen '%s'", name.c_str());
                return std::string("Unknown screen '") + name +
                       "'. Valid screens: main, sensors, smarthome, camera.";
            }
            Application::GetInstance().Schedule([hub, name]() {
                hub->SwitchScreen(name);
            });
            return true;
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
        "sensor_type: Loại cảm biến cần đọc ('battery', 'temperature', 'light', 'motion', 'network', 'system').",
        PropertyList({
            Property("sensor_type", kPropertyTypeString)
        }),
        [hub](const PropertyList& props) -> ReturnValue {
            std::string type = props["sensor_type"].value<std::string>();
            ESP_LOGI(TAG, "MCP Tool sensor.get_sensor_data: %s", type.c_str());

            // Tu dong bat the man hinh rieng cua cam bien do
            Application::GetInstance().Schedule([hub, type]() {
                hub->ShowSensorCard(type);
            });

            return SensorMonitor::GetInstance().GetSensorDataJson(type);
        });

    ESP_LOGI(TAG, "Smart Home, UI Navigation & Sensor MCP tools successfully registered");
}
