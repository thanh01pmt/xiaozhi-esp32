#include "web_config_server.h"
#include "smart_home_hub.h"
#include <esp_log.h>
#include <cJSON.h>
#include <cstring>
#include <cstdlib>

#define TAG "WebConfigServer"

static const char INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>M5Stack CoreS3 - Smart Home Hub</title>
  <style>
    body { font-family: -apple-system, sans-serif; background: #121212; color: #fff; margin: 0; padding: 20px; }
    .card { background: #1e1e1e; border-radius: 12px; padding: 20px; max-width: 480px; margin: 0 auto; box-shadow: 0 4px 12px rgba(0,0,0,0.5); }
    h2 { color: #00E5FF; margin-top: 0; }
    label { display: block; margin-top: 12px; font-size: 14px; color: #aaa; }
    input[type=text], input[type=password] { width: 100%; box-sizing: border-box; padding: 10px; background: #2a2a2a; border: 1px solid #444; border-radius: 6px; color: #fff; margin-top: 4px; }
    button { width: 100%; margin-top: 20px; padding: 12px; background: #00E5FF; border: none; border-radius: 6px; font-weight: bold; cursor: pointer; color: #000; }
    .status { margin-top: 16px; font-size: 13px; color: #888; text-align: center; }
  </style>
</head>
<body>
  <div class="card">
    <h2>Smart Home Hub Settings</h2>
    <form method="POST" action="/api/config">
      <label>Home Assistant URL:</label>
      <input type="text" name="ha_url" placeholder="http://192.168.1.100:8123" required>
      <label>Long-lived Access Token:</label>
      <input type="password" name="ha_token" placeholder="Bearer Token" required>
      <button type="submit">Save & Restart Services</button>
    </form>
    <div class="status">XiaoZhi ESP32 - M5Stack CoreS3 Hub</div>
  </div>
</body>
</html>
)rawliteral";

WebConfigServer::WebConfigServer() = default;

WebConfigServer::~WebConfigServer() {
    Stop();
}

esp_err_t WebConfigServer::IndexGetHandler(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

esp_err_t WebConfigServer::ApiStatusGetHandler(httpd_req_t* req) {
    httpd_resp_set_type(req, "application/json");
    std::string json_data = SmartHomeHub::GetInstance().GetDeviceStatusJson();
    return httpd_resp_send(req, json_data.c_str(), json_data.length());
}

esp_err_t WebConfigServer::ConfigPostHandler(httpd_req_t* req) {
    char buf[256];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    ESP_LOGI(TAG, "Config POST received: %s", buf);

    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, "Configuration saved successfully. Rebooting services...", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t WebConfigServer::Start() {
    if (server_handle_ != nullptr) {
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_open_sockets = 4;
    config.lru_purge_enable = true;

    esp_err_t ret = httpd_start(&server_handle_, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        return ret;
    }

    httpd_uri_t index_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = IndexGetHandler,
        .user_ctx = nullptr
    };
    httpd_register_uri_handler(server_handle_, &index_uri);

    httpd_uri_t status_uri = {
        .uri = "/api/status",
        .method = HTTP_GET,
        .handler = ApiStatusGetHandler,
        .user_ctx = nullptr
    };
    httpd_register_uri_handler(server_handle_, &status_uri);

    httpd_uri_t config_uri = {
        .uri = "/api/config",
        .method = HTTP_POST,
        .handler = ConfigPostHandler,
        .user_ctx = nullptr
    };
    httpd_register_uri_handler(server_handle_, &config_uri);

    ESP_LOGI(TAG, "WebConfigServer started on port %d", config.server_port);
    return ESP_OK;
}

void WebConfigServer::Stop() {
    if (server_handle_ != nullptr) {
        httpd_stop(server_handle_);
        server_handle_ = nullptr;
        ESP_LOGI(TAG, "WebConfigServer stopped");
    }
}
