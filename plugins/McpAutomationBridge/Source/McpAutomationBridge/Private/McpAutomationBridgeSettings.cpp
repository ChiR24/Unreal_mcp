#include "McpAutomationBridgeSettings.h"

#include "Internationalization/Text.h"

UMcpAutomationBridgeSettings::UMcpAutomationBridgeSettings()
{
    // Provide practical defaults so the Project Settings UI shows a usable out-of-the-box configuration.
    // By default the plugin will run in server/listen mode so the MCP server (node process)
    // can connect to it (tests expect the plugin to accept inbound MCP connections).
    EndpointUrl = TEXT("");
    CapabilityToken = TEXT("");
    AutoReconnectDelay = 5.0f; // Seconds between automatic reconnect attempts when disabled/failed
    bAlwaysListen = true; // Start a listening server by default in the Editor
    ListenHost = TEXT("127.0.0.1");
    ListenPorts = TEXT("8090,8091");
    bMultiListen = true;

    // Reasonable runtime tuning defaults
    HeartbeatIntervalMs = 1000; // advertise heartbeats every 1s
    HeartbeatTimeoutSeconds = 10.0f; // drop connections after 10s without heartbeat
    ListenBacklog = 10; // typical listen backlog
    AcceptSleepSeconds = 0.01f; // brief sleepers to reduce CPU when idle
    TickerIntervalSeconds = 0.1f; // subsystem tick every 100ms

    // Default logging behavior
    LogVerbosity = EMcpLogVerbosity::Log;
    bApplyLogVerbosityToAll = false;
}

FText UMcpAutomationBridgeSettings::GetSectionText() const
{
    return NSLOCTEXT("McpAutomationBridge", "SettingsSection", "MCP Automation Bridge");
}
