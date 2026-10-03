#include "network_client.h"
#include "board.h"
#include <esp_log.h>
#include <cJSON.h>

#define TAG "SH_NetClient"

SmartHomeNetworkClient::SmartHomeNetworkClient() = default;

void SmartHomeNetworkClient::SetHomeAssistantConfig(const std::string& base_url, const std::string& access_token) {
    base_url_ = base_url;
    access_token_ = access_token;
    ESP_LOGI(TAG, "Configured Home Assistant URL: %s", base_url_.c_str());
}

bool SmartHomeNetworkClient::PostHttpRequest(const std::string& endpoint, const std::string& json_payload) {
    if (base_url_.empty()) {
        ESP_LOGW(TAG, "Base URL not configured, simulating local execution");
        return true;
    }

    std::string full_url = base_url_ + endpoint;
    auto network = Board::GetInstance().GetNetwork();
    if (!network) {
        ESP_LOGE(TAG, "Network not available");
        return false;
    }

    auto http = network->CreateHttp();
    if (!http) {
        ESP_LOGE(TAG, "Failed to create Http instance");
        return false;
    }

    http->SetTimeout(8000);
    http->SetHeader("Content-Type", "application/json");
    if (!access_token_.empty()) {
        std::string auth = "Bearer " + access_token_;
        http->SetHeader("Authorization", auth);
    }
    http->SetContent(std::string(json_payload));

    auto open_result = http->Open("POST", full_url);
    if (!open_result) {
        ESP_LOGE(TAG, "HTTP POST %s Open failed: %s", full_url.c_str(), open_result.error().ToString().c_str());
        return false;
    }

    auto status_res = http->GetStatusCode();
    if (!status_res) {
        ESP_LOGE(TAG, "HTTP POST %s GetStatusCode failed: %s", full_url.c_str(), status_res.error().ToString().c_str());
        http->Close();
        return false;
    }

    int status = *status_res;
    ESP_LOGI(TAG, "POST %s, status=%d", endpoint.c_str(), status);
    http->Close();
    return (status >= 200 && status < 300);
}

std::string SmartHomeNetworkClient::GetHttpRequest(const std::string& endpoint, int* out_status, std::string* out_err_desc) {
    if (out_status != nullptr) {
        *out_status = -1;
    }
    if (out_err_desc != nullptr) {
        out_err_desc->clear();
    }
    if (base_url_.empty()) {
        ESP_LOGW(TAG, "Base URL not configured");
        if (out_err_desc != nullptr) *out_err_desc = "Base URL rỗng";
        return "";
    }

    std::string full_url = base_url_ + endpoint;
    auto network = Board::GetInstance().GetNetwork();
    if (!network) {
        ESP_LOGE(TAG, "Network not available");
        if (out_err_desc != nullptr) *out_err_desc = "Không có kết nối mạng";
        return "";
    }

    auto http = network->CreateHttp();
    if (!http) {
        ESP_LOGE(TAG, "Failed to create Http instance");
        if (out_err_desc != nullptr) *out_err_desc = "Không thể tạo Http client";
        return "";
    }

    http->SetTimeout(8000);
    if (!access_token_.empty()) {
        std::string auth = "Bearer " + access_token_;
        http->SetHeader("Authorization", auth);
    }

    auto open_result = http->Open("GET", full_url);
    if (!open_result) {
        std::string err_str = open_result.error().ToString();
        ESP_LOGE(TAG, "HTTP GET %s Open failed: %s", full_url.c_str(), err_str.c_str());
        if (out_err_desc != nullptr) {
            *out_err_desc = err_str;
        }
        return "";
    }

    auto status_res = http->GetStatusCode();
    if (!status_res) {
        std::string err_str = status_res.error().ToString();
        ESP_LOGE(TAG, "HTTP GET %s GetStatusCode failed: %s", full_url.c_str(), err_str.c_str());
        if (out_err_desc != nullptr) {
            *out_err_desc = err_str;
        }
        http->Close();
        return "";
    }

    int status = *status_res;
    if (out_status != nullptr) {
        *out_status = status;
    }
    ESP_LOGI(TAG, "GET %s, status=%d", endpoint.c_str(), status);

    std::string response = http->ReadAll();
    http->Close();
    return response;
}

bool SmartHomeNetworkClient::TestConnection(std::string* out_message) {
    if (base_url_.empty()) {
        if (out_message) *out_message = "Chưa cấu hình URL Home Assistant";
        return false;
    }
    std::string err_desc;
    int status = -1;
    std::string resp = GetHttpRequest("/api/", &status, &err_desc);
    if (status == 200) {
        if (out_message) *out_message = "Kết nối Home Assistant thành công! (HTTP 200 OK)";
        return true;
    } else if (status == 401) {
        if (out_message) *out_message = "Lỗi xác thực: Access Token không hợp lệ hoặc đã hết hạn (HTTP 401 Unauthorized)";
        return false;
    } else if (status == -1) {
        if (out_message) *out_message = "Không thể kết nối đến máy chủ Home Assistant: " + err_desc;
        return false;
    } else {
        if (out_message) *out_message = "Máy chủ phản hồi với mã lỗi HTTP " + std::to_string(status);
        return false;
    }
}

bool SmartHomeNetworkClient::SendSwitchCommand(const std::string& entity_id, bool turn_on) {
    std::string domain = "homeassistant";
    if (entity_id.find("light.") == 0) {
        domain = "light";
    } else if (entity_id.find("switch.") == 0) {
        domain = "switch";
    }

    std::string service = turn_on ? "turn_on" : "turn_off";
    std::string endpoint = "/api/services/" + domain + "/" + service;
    std::string payload = "{\"entity_id\":\"" + entity_id + "\"}";
    return PostHttpRequest(endpoint, payload);
}

bool SmartHomeNetworkClient::SendLevelCommand(const std::string& entity_id, int level) {
    if (entity_id.find("climate.") == 0) {
        std::string endpoint = "/api/services/climate/set_temperature";
        std::string payload = "{\"entity_id\":\"" + entity_id + "\",\"temperature\":" + std::to_string(level) + "}";
        return PostHttpRequest(endpoint, payload);
    }

    std::string endpoint = "/api/services/light/turn_on";
    int brightness = (level * 255) / 100;
    std::string payload = "{\"entity_id\":\"" + entity_id + "\",\"brightness\":" + std::to_string(brightness) + "}";
    return PostHttpRequest(endpoint, payload);
}

bool SmartHomeNetworkClient::TriggerScene(const std::string& scene_name) {
    std::string endpoint = "/api/services/scene/turn_on";
    std::string payload = "{\"entity_id\":\"scene." + scene_name + "\"}";
    return PostHttpRequest(endpoint, payload);
}

bool SmartHomeNetworkClient::FetchEntitiesFromHomeAssistant(std::vector<SmartDevice>& out_devices) {
    if (base_url_.empty()) {
        ESP_LOGW(TAG, "FetchEntities: Base URL not configured");
        return false;
    }

    int status = -1;
    std::string err_desc;
    std::string resp = GetHttpRequest("/api/states", &status, &err_desc);
    if (status != 200 || resp.empty()) {
        ESP_LOGE(TAG, "FetchEntities failed, status: %d, err: %s", status, err_desc.c_str());
        return false;
    }

    cJSON* root = cJSON_Parse(resp.c_str());
    if (!root || !cJSON_IsArray(root)) {
        ESP_LOGE(TAG, "FetchEntities: failed to parse JSON response");
        if (root) cJSON_Delete(root);
        return false;
    }

    out_devices.clear();
    int size = cJSON_GetArraySize(root);
    for (int i = 0; i < size; ++i) {
        cJSON* item = cJSON_GetArrayItem(root, i);
        if (!cJSON_IsObject(item)) continue;

        cJSON* entity_id_obj = cJSON_GetObjectItem(item, "entity_id");
        if (!entity_id_obj || !cJSON_IsString(entity_id_obj)) continue;
        std::string entity_id = entity_id_obj->valuestring;

        // Filter domains of interest: switch, light, climate, fan
        DeviceType dev_type;
        if (entity_id.rfind("switch.", 0) == 0) {
            dev_type = DeviceType::Switch;
        } else if (entity_id.rfind("light.", 0) == 0) {
            dev_type = DeviceType::Light;
        } else if (entity_id.rfind("climate.", 0) == 0) {
            dev_type = DeviceType::Climate;
        } else if (entity_id.rfind("fan.", 0) == 0) {
            dev_type = DeviceType::Switch;
        } else {
            continue; // Skip sensors, automation, zone, etc.
        }

        // Get friendly_name
        std::string friendly_name = entity_id;
        cJSON* attrs = cJSON_GetObjectItem(item, "attributes");
        if (attrs && cJSON_IsObject(attrs)) {
            cJSON* fn = cJSON_GetObjectItem(attrs, "friendly_name");
            if (fn && cJSON_IsString(fn)) {
                friendly_name = fn->valuestring;
            }
        }

        // Get state
        bool state = false;
        cJSON* state_obj = cJSON_GetObjectItem(item, "state");
        if (state_obj && cJSON_IsString(state_obj)) {
            std::string st = state_obj->valuestring;
            state = (st == "on" || st == "open" || st == "active" || st == "cool" || st == "heat");
        }

        SmartDevice dev;
        dev.id = entity_id;
        dev.name = friendly_name;
        dev.room = "Home Assistant";
        dev.type = dev_type;
        dev.state = state;
        dev.level = state ? 100 : 0;
        out_devices.push_back(dev);
        ESP_LOGI(TAG, "Discovered HA entity: %s ('%s') [%s]",
                 entity_id.c_str(), friendly_name.c_str(), state ? "ON" : "OFF");
    }

    cJSON_Delete(root);
    ESP_LOGI(TAG, "FetchEntities: found %u controllable devices", (unsigned)out_devices.size());
    return true;
}
