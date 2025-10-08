#include "McpAutomationBridgeSettings.h"

#include "Internationalization/Text.h"

UMcpAutomationBridgeSettings::UMcpAutomationBridgeSettings()
{
    EndpointUrl = TEXT("ws://127.0.0.1:8090/mcp-automation");
    CapabilityToken = TEXT("");
    AutoReconnectDelay = 5.0f;
}

FText UMcpAutomationBridgeSettings::GetSectionText() const
{
    return NSLOCTEXT("McpAutomationBridge", "SettingsSection", "MCP Automation Bridge");
}
