#include "network_client.h"
#include <esp_log.h>
#include <esp_http_client.h>
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
    esp_http_client_config_t config = {};
    config.url = full_url.c_str();
    config.method = HTTP_METHOD_POST;
    config.timeout_ms = 3000;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        return false;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    if (!access_token_.empty()) {
        std::string auth = "Bearer " + access_token_;
        esp_http_client_set_header(client, "Authorization", auth.c_str());
    }

    esp_http_client_set_post_field(client, json_payload.c_str(), json_payload.length());
    esp_err_t err = esp_http_client_perform(client);
    bool success = false;
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        success = (status >= 200 && status < 300);
        ESP_LOGI(TAG, "POST %s, status=%d", endpoint.c_str(), status);
    } else {
        ESP_LOGE(TAG, "HTTP POST failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return success;
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
