#ifndef SMART_HOME_NETWORK_CLIENT_H
#define SMART_HOME_NETWORK_CLIENT_H

#include <string>
#include <functional>
#include "device_model.h"

class SmartHomeNetworkClient {
public:
    SmartHomeNetworkClient();
    ~SmartHomeNetworkClient() = default;

    void SetHomeAssistantConfig(const std::string& base_url, const std::string& access_token);
    const std::string& GetBaseUrl() const { return base_url_; }
    const std::string& GetAccessToken() const { return access_token_; }
    bool TestConnection(std::string* out_message = nullptr);
    bool SendSwitchCommand(const std::string& entity_id, bool turn_on);
    bool SendLevelCommand(const std::string& entity_id, int level);
    bool SendMediaPlayCommand(const std::string& entity_id, const std::string& media_content_id, const std::string& media_content_type);
    bool SendMediaStopCommand(const std::string& entity_id);
    bool TriggerScene(const std::string& scene_name);
    bool FetchEntitiesFromHomeAssistant(std::vector<SmartDevice>& out_devices);
    bool FetchDeviceStatus(const std::string& entity_id, SmartDevice& out_device);

private:
    std::string base_url_;
    std::string access_token_;

    bool PostHttpRequest(const std::string& endpoint, const std::string& json_payload);
    std::string GetHttpRequest(const std::string& endpoint, int* out_status = nullptr, std::string* out_err_desc = nullptr);
};

#endif // SMART_HOME_NETWORK_CLIENT_H
