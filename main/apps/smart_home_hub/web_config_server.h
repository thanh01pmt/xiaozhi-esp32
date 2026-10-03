#ifndef WEB_CONFIG_SERVER_H
#define WEB_CONFIG_SERVER_H

#include <esp_err.h>
#include <esp_http_server.h>
#include <string>

class WebConfigServer {
public:
    WebConfigServer();
    ~WebConfigServer();

    esp_err_t Start();
    void Stop();
    bool IsRunning() const { return server_handle_ != nullptr; }

private:
    httpd_handle_t server_handle_ = nullptr;

    static esp_err_t IndexGetHandler(httpd_req_t* req);
    static esp_err_t ConfigGetHandler(httpd_req_t* req);
    static esp_err_t ConfigPostHandler(httpd_req_t* req);
    static esp_err_t SyncPostHandler(httpd_req_t* req);
    static esp_err_t ApiStatusGetHandler(httpd_req_t* req);
};

#endif // WEB_CONFIG_SERVER_H
