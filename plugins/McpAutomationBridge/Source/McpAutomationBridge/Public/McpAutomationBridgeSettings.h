#pragma once

#include "Engine/DeveloperSettings.h"
#include "McpAutomationBridgeSettings.generated.h"

// Store these settings in the project's config (DefaultGame.ini) and expose
// them in Project Settings -> Plugins. Use defaultconfig so the values are
// written to the project's default INI file when persisted.

UENUM()
enum class EMcpLogVerbosity : uint8
{
    NoLogging     UMETA(DisplayName = "No Logging"),
    Fatal         UMETA(DisplayName = "Fatal"),
    Error         UMETA(DisplayName = "Error"),
    Warning       UMETA(DisplayName = "Warning"),
    Display       UMETA(DisplayName = "Display"),
    Log           UMETA(DisplayName = "Log"),
    Verbose       UMETA(DisplayName = "Verbose"),
    VeryVerbose   UMETA(DisplayName = "VeryVerbose")
};

UCLASS(config=Game, defaultconfig, meta = (DisplayName = "MCP Automation Bridge"))
class MCPAUTOMATIONBRIDGE_API UMcpAutomationBridgeSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UMcpAutomationBridgeSettings();

    /** If true, the plugin will always start a listening WebSocket server on startup and accept inbound MCP connections. */
    UPROPERTY(config, EditAnywhere, Category = "Connection")
    bool bAlwaysListen;

    /** Host to bind the listening sockets. Default: 127.0.0.1 (loopback).
     * To bind to LAN addresses (e.g., 0.0.0.0 or 192.168.x.x), enable bAllowNonLoopback in Security settings.
     */
    UPROPERTY(config, EditAnywhere, Category = "Connection")
    FString ListenHost;

    /** Comma-separated list of ports to listen on. Example: "8090,8091" */
    UPROPERTY(config, EditAnywhere, Category = "Connection")
    FString ListenPorts;

    UPROPERTY(config, EditAnywhere, Category = "Connection")
    FString EndpointUrl;

    UPROPERTY(config, EditAnywhere, Category = "Security")
    FString CapabilityToken;

    UPROPERTY(config, EditAnywhere, Category = "Connection", meta = (ClampMin = "0.0"))
    float AutoReconnectDelay;

    /** Port the plugin expects the MCP server to use when the tool connects back as a client (optional). */
    UPROPERTY(config, EditAnywhere, Category = "Connection")
    int32 ClientPort;

    /** When true, require a capability token for incoming connections (enforces matching token). */
    UPROPERTY(config, EditAnywhere, Category = "Security")
    bool bRequireCapabilityToken;

    /** SECURITY WARNING: When enabled, allows binding to non-loopback addresses (e.g., 0.0.0.0, 192.168.x.x).
     * This exposes the automation bridge to your local network. Only enable if you need LAN access
     * and understand the security implications. Default: false (loopback-only).
     */
    UPROPERTY(config, EditAnywhere, Category = "Security")
    bool bAllowNonLoopback;

    /** Enable TLS for the automation bridge WebSocket server. */
    UPROPERTY(config, EditAnywhere, Category = "Security")
    bool bEnableTls;

    /** PEM certificate path used for TLS (server). */
    UPROPERTY(config, EditAnywhere, Category = "Security")
    FString TlsCertificatePath;

    /** PEM private key path used for TLS (server). */
    UPROPERTY(config, EditAnywhere, Category = "Security")
    FString TlsPrivateKeyPath;

    /** Max inbound WebSocket messages per minute before disconnect (0 = disabled). */
    UPROPERTY(config, EditAnywhere, Category = "Security", meta = (ClampMin = "0"))
    int32 MaxMessagesPerMinute;

    /** Max inbound automation_request messages per minute before disconnect (0 = disabled). */
    UPROPERTY(config, EditAnywhere, Category = "Security", meta = (ClampMin = "0"))
    int32 MaxAutomationRequestsPerMinute;

    /** Optional runtime log verbosity override exposed via Project Settings. */

    UPROPERTY(config, EditAnywhere, Category = "Debug")
    EMcpLogVerbosity LogVerbosity;

    /** When true, apply the selected LogVerbosity to this plugin's log category at runtime. */
    UPROPERTY(config, EditAnywhere, Category = "Debug")
    bool bApplyLogVerbosityToAll;

    /** When true, emit extra per-socket telemetry for control/response delivery
     * attempts. This is intended for short-term debugging of intermittent
     * delivery failures and is off by default to avoid log spam. When enabled
     * the subsystem will raise aggregated delivery summaries to Log level and
     * include per-socket details for inspection.
     */
    UPROPERTY(config, EditAnywhere, Category = "Debug")
    bool bEnableSocketTelemetry;

    /** When true, the plugin will open multiple listen sockets provided by ListenPorts. */
    UPROPERTY(config, EditAnywhere, Category = "Connection")
    bool bMultiListen;

    // Heartbeat settings
    /** Heartbeat interval to advertise to connected clients (milliseconds). If <= 0, server default will be used. */
    UPROPERTY(config, EditAnywhere, Category = "Heartbeat")
    int32 HeartbeatIntervalMs;

    /** How many seconds without a heartbeat before a connection is considered timed out. If <= 0, heartbeat timeout checking is disabled. */
    UPROPERTY(config, EditAnywhere, Category = "Heartbeat", meta = (ClampMin = "0.0"))
    float HeartbeatTimeoutSeconds;

    // Server socket tuning
    /** Backlog parameter passed to listen() when creating the listening socket. If <= 0, engine default will be used. */
    UPROPERTY(config, EditAnywhere, Category = "Connection")
    int32 ListenBacklog;

    /** How long (seconds) the server socket thread should sleep when no incoming connection; small values reduce CPU but increase latency. If <= 0, engine default will be used. */
    UPROPERTY(config, EditAnywhere, Category = "Connection", meta = (ClampMin = "0.0"))
    float AcceptSleepSeconds;

    /** Frequency, in seconds, for the subsystem ticker. If <= 0, engine default will be used. */
    UPROPERTY(config, EditAnywhere, Category = "Debug", meta = (ClampMin = "0.0"))
    float TickerIntervalSeconds;

    virtual FName GetCategoryName() const override { return FName(TEXT("Plugins")); }
    virtual FText GetSectionText() const override;

#if WITH_EDITOR
    // Persist changed properties immediately when edited in Project Settings
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override
    {
        Super::PostEditChangeProperty(PropertyChangedEvent);
        SaveConfig();
    }
#endif
};
