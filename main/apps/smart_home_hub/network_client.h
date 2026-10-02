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
    bool SendSwitchCommand(const std::string& entity_id, bool turn_on);
    bool SendLevelCommand(const std::string& entity_id, int level);
    bool TriggerScene(const std::string& scene_name);
    bool FetchDeviceStatus(const std::string& entity_id, SmartDevice& out_device);

private:
    std::string base_url_;
    std::string access_token_;

    bool PostHttpRequest(const std::string& endpoint, const std::string& json_payload);
    std::string GetHttpRequest(const std::string& endpoint);
};

#endif // SMART_HOME_NETWORK_CLIENT_H
