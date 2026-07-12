#pragma once

#include "CoreMinimal.h"
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

    /** SECURITY WARNING: When enabled, allows the WebSocket and native MCP
     * transports to bind to non-loopback addresses. This exposes editor
     * automation to the local network. Default: false.
     */
    UPROPERTY(config, EditAnywhere, Category = "Security")
    bool bAllowNonLoopback;

    /** Deprecated compatibility field. Network media URLs are always disabled
     * because the media backend cannot pin redirect destinations. */
    UPROPERTY(config, EditAnywhere, Category = "Security|Media")
    bool bAllowLoopbackMediaUrls;

    /** Deprecated compatibility field. This prefix is ignored. */
    UPROPERTY(config, EditAnywhere, Category = "Security|Media",
        meta = (EditCondition = "bAllowLoopbackMediaUrls"))
    FString AllowedLoopbackMediaUrlPrefix;

    /** Maximum output width or height accepted by Movie Render Queue. */
    UPROPERTY(config, EditAnywhere, Category = "Security|Movie Render Queue",
        meta = (ClampMin = "1", ClampMax = "16384"))
    int32 MaxMovieRenderResolutionDimension;

    /** Maximum per-session MCP requests per minute (HTTP/SSE only). Counts
     *  every request method, not just tool calls. 0 disables the cap. */
    UPROPERTY(config, EditAnywhere, Category = "Security|Native MCP Rate Limit",
        meta = (ClampMin = "0"))
    int32 MaxClientRequestsPerMinute = 600;

    /** Maximum per-session MCP tool calls per minute (HTTP/SSE only). 0
     *  disables the cap. */
    UPROPERTY(config, EditAnywhere, Category = "Security|Native MCP Rate Limit",
        meta = (ClampMin = "0"))
    int32 MaxClientToolCallsPerMinute = 120;

    /** Maximum width multiplied by height accepted by Movie Render Queue. */
    UPROPERTY(config, EditAnywhere, Category = "Security|Movie Render Queue",
        meta = (ClampMin = "1"))
    int64 MaxMovieRenderPixelCount;

    /** Maximum effective output frames accepted per MRQ job, including handles. */
    UPROPERTY(config, EditAnywhere, Category = "Security|Movie Render Queue",
        meta = (ClampMin = "1"))
    int32 MaxMovieRenderFrameCount;

    /** Maximum effective output frame rate accepted by Movie Render Queue. */
    UPROPERTY(config, EditAnywhere, Category = "Security|Movie Render Queue",
        meta = (ClampMin = "1", ClampMax = "240"))
    int32 MaxMovieRenderEffectiveFrameRate = 240;

    /** Maximum handle frames accepted on either side of an MRQ shot. */
    UPROPERTY(config, EditAnywhere, Category = "Security|Movie Render Queue",
        meta = (ClampMin = "0"))
    int32 MaxMovieRenderHandleFrameCount;

    /** Maximum spatial or temporal anti-aliasing sample count. */
    UPROPERTY(config, EditAnywhere, Category = "Security|Movie Render Queue",
        meta = (ClampMin = "1"))
    int32 MaxMovieRenderSampleCount;

    /** Maximum spatial x temporal anti-aliasing sample product. */
    UPROPERTY(config, EditAnywhere, Category = "Security|Movie Render Queue",
        meta = (ClampMin = "1"))
    int32 MaxMovieRenderCombinedSampleCount;

    /** Maximum console variables accepted in one MRQ configuration request. */
    UPROPERTY(config, EditAnywhere, Category = "Security|Movie Render Queue",
        meta = (ClampMin = "0"))
    int32 MaxMovieRenderConsoleVariables;

    /** Exact MRQ console-variable names that automation may set. */
    UPROPERTY(config, EditAnywhere, Category = "Security|Movie Render Queue")
    TArray<FString> MovieRenderConsoleVariableAllowlist;

    /** Maximum absolute value accepted for an allowlisted MRQ console variable. */
    UPROPERTY(config, EditAnywhere, Category = "Security|Movie Render Queue",
        meta = (ClampMin = "0.0"))
    float MaxMovieRenderConsoleVariableMagnitude;

    /** Executor classes that callers may select explicitly. */
    UPROPERTY(config, EditAnywhere, Category = "Security|Movie Render Queue")
    TArray<FString> MovieRenderExecutorClassAllowlist;

    /** Burn-in widget classes that callers may select explicitly. */
    UPROPERTY(config, EditAnywhere, Category = "Security|Movie Render Queue")
    TArray<FString> MovieRenderBurnInClassAllowlist;

    UPROPERTY(config, EditAnywhere, Category = "Security|Movie Render Queue",
        meta = (ClampMin = "1"))
    int32 MaxMovieRenderEnabledJobs;

    /** Maximum total jobs retained in the Movie Render Queue. */
    UPROPERTY(config, EditAnywhere, Category = "Security|Movie Render Queue",
        meta = (ClampMin = "1"))
    int32 MaxMovieRenderQueueJobs;

    UPROPERTY(config, EditAnywhere, Category = "Security|Movie Render Queue",
        meta = (ClampMin = "1"))
    int64 MaxMovieRenderAggregateWork;

    /** Maximum frame-number zero padding accepted in output settings. */
    UPROPERTY(config, EditAnywhere, Category = "Security|Movie Render Queue",
        meta = (ClampMin = "1"))
    int32 MaxMovieRenderZeroPadFrameNumbers;

    UPROPERTY(config, EditAnywhere, Category = "Security|Movie Render Queue",
        meta = (ClampMin = "1"))
    int32 MaxMovieRenderOutputScanFiles;

    UPROPERTY(config, EditAnywhere, Category = "Security|Take Recorder")
    TArray<FString> TakeRecorderSourceClassAllowlist;

    /** Maximum number of actor or class entries accepted by one Take Recorder request. */
    UPROPERTY(config, EditAnywhere, Category = "Security|Take Recorder",
        meta = (ClampMin = "1"))
    int32 MaxTakeRecorderSourceItems;

    /** Maximum length accepted for Take Recorder actor names and class paths. */
    UPROPERTY(config, EditAnywhere, Category = "Security|Take Recorder",
        meta = (ClampMin = "1"))
    int32 MaxTakeRecorderStringLength;

    /** Maximum server-side MRQ wait deadline in milliseconds. */
    UPROPERTY(config, EditAnywhere, Category = "Security|Movie Render Queue",
        meta = (ClampMin = "1"))
    int32 MaxMovieRenderTimeoutMs;

    /** Maximum time to wait for a timed-out executor to settle after cancellation. */
    UPROPERTY(config, EditAnywhere, Category = "Security|Movie Render Queue",
        meta = (ClampMin = "1", ClampMax = "30000"))
    int32 MaxMovieRenderCancellationWaitMs;

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

    // ── Native MCP Streamable HTTP ──────────────────────────────────────

    /** Enable native MCP Streamable HTTP endpoint (POST /mcp).
     * AI clients (Claude Code, Cursor, etc.) connect directly without the TypeScript bridge.
     * Requires editor restart after changing. */
    UPROPERTY(config, EditAnywhere, Category = "Native MCP",
        meta = (DisplayName = "Enable Native MCP Server"))
    bool bEnableNativeMCP = false;

    /** Port for the native MCP HTTP server. Must differ from WebSocket ports. */
    UPROPERTY(config, EditAnywhere, Category = "Native MCP",
        meta = (DisplayName = "Native MCP Port", EditCondition = "bEnableNativeMCP",
                ClampMin = "1024", ClampMax = "65535"))
    int32 NativeMCPPort = 3000;

    /** Load all 23 canonical tools on startup. */
    UPROPERTY(config, EditAnywhere, Category = "Native MCP",
        meta = (DisplayName = "Load All Tools on Start", EditCondition = "bEnableNativeMCP"))
    bool bLoadAllToolsOnStart = true;

    /** When true, tools/list advertises only the 'unreal' gateway; the canonical 23 tools stay registered and reachable through it. Default: true. */
    UPROPERTY(config, EditAnywhere, Category = "Native MCP",
        meta = (DisplayName = "Enable Native Gateway Mode", EditCondition = "bEnableNativeMCP"))
    bool bEnableNativeGateway = true;

    /** Additional instructions sent to AI clients in the MCP initialize response.
     * Use this to describe your project, conventions, or constraints.
     * Appended after the default server instructions from server-info.json. */
    UPROPERTY(config, EditAnywhere, Category = "Native MCP",
        meta = (DisplayName = "Server Instructions", EditCondition = "bEnableNativeMCP",
                MultiLine = "true"))
    FString NativeMCPInstructions;

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
