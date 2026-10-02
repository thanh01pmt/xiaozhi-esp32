#include "mcp_tools.h"
#include "smart_home_hub.h"
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

    ESP_LOGI(TAG, "Smart Home MCP tools successfully registered");
}
