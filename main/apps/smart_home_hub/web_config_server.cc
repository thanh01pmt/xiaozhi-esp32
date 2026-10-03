#include "web_config_server.h"
#include "smart_home_hub.h"
#include <esp_log.h>
#include <cJSON.h>
#include <cstring>
#include <cstdlib>

#define TAG "WebConfigServer"

static std::string UrlDecode(const std::string& str) {
    std::string ret;
    char ch;
    int i, ii;
    for (i = 0; i < (int)str.length(); i++) {
        if (str[i] == '%') {
            if (i + 2 < (int)str.length()) {
                sscanf(str.substr(i + 1, 2).c_str(), "%x", &ii);
                ch = static_cast<char>(ii);
                ret += ch;
                i = i + 2;
            }
        } else if (str[i] == '+') {
            ret += ' ';
        } else {
            ret += str[i];
        }
    }
    return ret;
}

static void ParseFormData(const std::string& body, std::string& ha_url, std::string& ha_token) {
    size_t pos = 0;
    while (pos < body.length()) {
        size_t eq = body.find('=', pos);
        if (eq == std::string::npos) break;
        std::string key = body.substr(pos, eq - pos);
        size_t amp = body.find('&', eq + 1);
        std::string val;
        if (amp == std::string::npos) {
            val = body.substr(eq + 1);
            pos = body.length();
        } else {
            val = body.substr(eq + 1, amp - eq - 1);
            pos = amp + 1;
        }

        if (key == "ha_url") {
            ha_url = UrlDecode(val);
        } else if (key == "ha_token") {
            ha_token = UrlDecode(val);
        }
    }
}

static const char INDEX_HTML_TEMPLATE[] = R"rawliteral(<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>M5Stack CoreS3 - Smart Home Hub</title>
  <style>
    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; background: #121212; color: #fff; margin: 0; padding: 20px; }
    .card { background: #1e1e1e; border-radius: 12px; padding: 24px; max-width: 520px; margin: 0 auto; box-shadow: 0 4px 20px rgba(0,0,0,0.6); border: 1px solid #333; }
    h2 { color: #00E5FF; margin-top: 0; font-size: 22px; }
    label { display: block; margin-top: 16px; font-size: 13px; font-weight: 600; color: #bbb; text-transform: uppercase; letter-spacing: 0.5px; }
    input[type=text], input[type=password] { width: 100%; box-sizing: border-box; padding: 12px; background: #2a2a2a; border: 1px solid #444; border-radius: 8px; color: #fff; margin-top: 6px; font-size: 14px; }
    input:focus { border-color: #00E5FF; outline: none; }
    button { width: 100%; margin-top: 24px; padding: 14px; background: #00E5FF; border: none; border-radius: 8px; font-weight: bold; font-size: 15px; cursor: pointer; color: #000; transition: background 0.2s; }
    button:hover { background: #33ebff; }
    .status-badge { margin-top: 20px; padding: 12px; border-radius: 8px; font-size: 13px; line-height: 1.5; }
    .status-badge.ok { background: rgba(0, 229, 255, 0.15); border: 1px solid #00E5FF; color: #00E5FF; }
    .status-badge.warn { background: rgba(255, 179, 0, 0.15); border: 1px solid #ffb300; color: #ffb300; }
    .status-badge.err { background: rgba(255, 82, 82, 0.15); border: 1px solid #ff5252; color: #ff5252; }
    .info { margin-top: 20px; font-size: 12px; color: #777; text-align: center; }
  </style>
</head>
<body>
  <div class="card">
    <h2>Smart Home Hub Settings</h2>
    <form method="POST" action="/api/config">
      <label>Home Assistant URL:</label>
      <input type="text" name="ha_url" value="%HA_URL%" placeholder="http://192.168.1.100:8123 hoặc https://ha.orchable.app" required>
      <label>Long-Lived Access Token:</label>
      <input type="password" name="ha_token" value="%HA_TOKEN%" placeholder="Bearer Token từ Home Assistant Profile" required>
      <button type="submit">Lưu & Kiểm tra Kết nối HA</button>
    </form>
    %STATUS_BADGE%
    <div class="info">XiaoZhi ESP32 - M5Stack CoreS3 Smart Hub</div>
  </div>
</body>
</html>)rawliteral";

WebConfigServer::WebConfigServer() = default;

WebConfigServer::~WebConfigServer() {
    Stop();
}

esp_err_t WebConfigServer::IndexGetHandler(httpd_req_t* req) {
    auto& hub = SmartHomeHub::GetInstance();
    std::string current_url = hub.GetNetworkClient().GetBaseUrl();
    std::string current_token = hub.GetNetworkClient().GetAccessToken();

    std::string html = INDEX_HTML_TEMPLATE;
    
    // Replace URL placeholder
    size_t pos = html.find("%HA_URL%");
    if (pos != std::string::npos) {
        html.replace(pos, 8, current_url);
    }
    
    // Replace Token placeholder
    pos = html.find("%HA_TOKEN%");
    if (pos != std::string::npos) {
        html.replace(pos, 10, current_token);
    }

    // Status badge
    std::string status_badge;
    if (current_url.empty()) {
        status_badge = "<div class=\"status-badge warn\">⚠️ Chưa cấu hình kết nối Home Assistant.</div>";
    } else {
        status_badge = "<div class=\"status-badge ok\">✅ Đang liên kết với: <b>" + current_url + "</b></div>";
    }
    pos = html.find("%STATUS_BADGE%");
    if (pos != std::string::npos) {
        html.replace(pos, 14, status_badge);
    }

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, html.c_str(), html.length());
}

esp_err_t WebConfigServer::ConfigGetHandler(httpd_req_t* req) {
    httpd_resp_set_type(req, "application/json");
    auto& hub = SmartHomeHub::GetInstance();
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "ha_url", hub.GetNetworkClient().GetBaseUrl().c_str());
    cJSON_AddBoolToObject(root, "has_token", !hub.GetNetworkClient().GetAccessToken().empty());
    
    char* printed = cJSON_PrintUnformatted(root);
    std::string res = printed ? printed : "{}";
    if (printed) free(printed);
    cJSON_Delete(root);
    return httpd_resp_send(req, res.c_str(), res.length());
}

esp_err_t WebConfigServer::ApiStatusGetHandler(httpd_req_t* req) {
    httpd_resp_set_type(req, "application/json");
    std::string json_data = SmartHomeHub::GetInstance().GetDeviceStatusJson();
    return httpd_resp_send(req, json_data.c_str(), json_data.length());
}

esp_err_t WebConfigServer::ConfigPostHandler(httpd_req_t* req) {
    size_t total_len = req->content_len;
    if (total_len == 0 || total_len > 2048) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid content length");
        return ESP_FAIL;
    }

    std::string body;
    body.resize(total_len);
    size_t received = 0;
    while (received < total_len) {
        int ret = httpd_req_recv(req, &body[received], total_len - received);
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            ESP_LOGE(TAG, "Failed to receive POST body");
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Receive failed");
            return ESP_FAIL;
        }
        received += ret;
    }

    std::string ha_url, ha_token;
    ParseFormData(body, ha_url, ha_token);

    // Trim trailing slash on URL
    while (!ha_url.empty() && ha_url.back() == '/') {
        ha_url.pop_back();
    }

    ESP_LOGI(TAG, "Parsed HA URL: %s, token length: %u", ha_url.c_str(), (unsigned)ha_token.length());

    auto& hub = SmartHomeHub::GetInstance();
    hub.SaveHomeAssistantConfig(ha_url, ha_token);

    // Test connection
    std::string test_msg;
    bool ok = hub.GetNetworkClient().TestConnection(&test_msg);

    std::string resp_html = "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
        "<title>Kết quả cấu hình</title><style>"
        "body { font-family: -apple-system, sans-serif; background: #121212; color: #fff; padding: 20px; text-align: center; }"
        ".card { background: #1e1e1e; border-radius: 12px; padding: 24px; max-width: 480px; margin: 30px auto; border: 1px solid #333; }"
        "h2 { color: " + std::string(ok ? "#00E5FF" : "#ff5252") + "; }"
        ".msg { margin: 20px 0; font-size: 15px; color: #ddd; }"
        "a { display: inline-block; padding: 10px 20px; background: #00E5FF; color: #000; text-decoration: none; border-radius: 6px; font-weight: bold; margin-top: 15px; }"
        "</style></head><body><div class=\"card\">"
        "<h2>" + std::string(ok ? "Lưu Thành Công & Đã Kết Nối!" : "Đã Lưu Nhưng Lỗi Kết Nối") + "</h2>"
        "<div class=\"msg\">" + test_msg + "</div>"
        "<div>URL: <b>" + ha_url + "</b></div>"
        "<a href=\"/\">Quay lại Trang Chủ</a></div></body></html>";

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, resp_html.c_str(), resp_html.length());
    return ESP_OK;
}

esp_err_t WebConfigServer::SyncPostHandler(httpd_req_t* req) {
    auto& hub = SmartHomeHub::GetInstance();
    bool ok = hub.SyncDevicesFromHomeAssistant();

    std::string json = "{\"success\":" + std::string(ok ? "true" : "false") +
                       ",\"message\":\"" + std::string(ok ? "Đã đồng bộ thiết bị thành công!" : "Lỗi khi đồng bộ thiết bị") + "\"}";
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json.c_str(), json.length());
}

esp_err_t WebConfigServer::Start() {
    if (server_handle_ != nullptr) {
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.stack_size = 10240;
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

    httpd_uri_t get_config_uri = {
        .uri = "/api/config",
        .method = HTTP_GET,
        .handler = ConfigGetHandler,
        .user_ctx = nullptr
    };
    httpd_register_uri_handler(server_handle_, &get_config_uri);

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

    httpd_uri_t sync_uri = {
        .uri = "/api/sync",
        .method = HTTP_POST,
        .handler = SyncPostHandler,
        .user_ctx = nullptr
    };
    httpd_register_uri_handler(server_handle_, &sync_uri);

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
