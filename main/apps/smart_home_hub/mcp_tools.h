#ifndef SMART_HOME_MCP_TOOLS_H
#define SMART_HOME_MCP_TOOLS_H

#include "mcp_server.h"

class SmartHomeHub;

class SmartHomeMcpTools {
public:
    static void RegisterTools(SmartHomeHub* hub);
};

#endif // SMART_HOME_MCP_TOOLS_H
