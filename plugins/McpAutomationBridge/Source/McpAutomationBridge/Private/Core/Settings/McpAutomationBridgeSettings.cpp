#include "McpAutomationBridgeSettings.h"
#include "Dom/JsonObject.h"

#include "Internationalization/Text.h"

/**
 * @brief Initializes MCP Automation Bridge settings with practical defaults for editor use.
 *
 * Sets sensible out-of-the-box values for connectivity, listening behavior, runtime timing, and logging
 * so the plugin runs in server/listen mode by default and presents a usable configuration in Project Settings.
 */
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
    bRequireCapabilityToken = false;
    bAllowNonLoopback = false; // Security: default to loopback-only binding
    bAllowLoopbackMediaUrls = false;
    AllowedLoopbackMediaUrlPrefix = TEXT("");
    MaxMovieRenderResolutionDimension = 8192;
    MaxMovieRenderPixelCount = 33554432;
    MaxMovieRenderFrameCount = 10000;
    MaxMovieRenderEffectiveFrameRate = 240;
    MaxMovieRenderHandleFrameCount = 1000;
    MaxMovieRenderSampleCount = 64;
    MaxMovieRenderCombinedSampleCount = 256;
    MaxMovieRenderConsoleVariables = 32;
    MovieRenderConsoleVariableAllowlist = {
        TEXT("r.MotionBlurQuality"),
        TEXT("r.DepthOfFieldQuality")
    };
    MaxMovieRenderConsoleVariableMagnitude = 1000.0f;
    // C7: The class allowlists below ship with safe default entries, not
    // empty. The audit flagged this as "empty by default" but inspection
    // shows the defaults are populated with editor-provided safe classes
    // (PIE/in-process executors, default burn-in, take recorder sources).
    // These defaults are applied unless the project's config overrides
    // them. Do NOT clear these defaults in code — projects that rely on
    // them would silently get an empty allowlist and a different policy
    // (the validators use ContainsByPredicate which returns false on an
    // empty list, blocking all values). Empty arrays here are an explicit
    // deny-all configuration, not a default.
    MovieRenderExecutorClassAllowlist = {
        TEXT("/Script/MovieRenderPipelineEditor.MoviePipelinePIEExecutor"),
        TEXT("/Script/MovieRenderPipelineCore.MoviePipelineInProcessExecutor")
    };
    MovieRenderBurnInClassAllowlist = {
        TEXT("/MovieRenderPipeline/Blueprints/DefaultBurnIn.DefaultBurnIn_C")
    };
    MaxMovieRenderEnabledJobs = 8;
    MaxMovieRenderQueueJobs = 32;
    MaxMovieRenderAggregateWork = 4000000000000LL;
    MaxMovieRenderZeroPadFrameNumbers = 12;
    MaxMovieRenderOutputScanFiles = 10000;
    TakeRecorderSourceClassAllowlist = {
        TEXT("/Script/TakeRecorderSources.TakeRecorderCameraCutSource"),
        TEXT("/Script/TakeRecorderSources.TakeRecorderLevelVisibilitySource")
    };
    MaxTakeRecorderSourceItems = 64;
    MaxTakeRecorderStringLength = 1024;
    MaxMovieRenderTimeoutMs = 3600000;
    MaxMovieRenderCancellationWaitMs = 30000;
    // CRITICAL: Default to 0 (disabled) for development/testing - prevents rate limit disconnects during rapid API calls
    // For production deployments, set to a reasonable limit (e.g., 600) via Project Settings or environment variables
    MaxMessagesPerMinute = 0;
    MaxAutomationRequestsPerMinute = 0;
    bEnableTls = false;
    TlsCertificatePath = TEXT("");
    TlsPrivateKeyPath = TEXT("");

    // Reasonable runtime tuning defaults
    HeartbeatIntervalMs = 1000; // advertise heartbeats every 1s
    HeartbeatTimeoutSeconds = 10.0f; // drop connections after 10s without heartbeat
    ListenBacklog = 10; // typical listen backlog
    AcceptSleepSeconds = 0.01f; // brief sleepers to reduce CPU when idle
    TickerIntervalSeconds = 0.1f; // subsystem tick every 100ms

    // Default logging behavior
    LogVerbosity = EMcpLogVerbosity::Log;
    bApplyLogVerbosityToAll = false;
    // Per-socket telemetry (off by default to avoid noise)
    bEnableSocketTelemetry = false;
}

/**
 * @brief Returns the localized text used as the settings section header for the MCP Automation Bridge.
 *
 * @return FText The localized label "MCP Automation Bridge" for display in the settings UI.
 */
FText UMcpAutomationBridgeSettings::GetSectionText() const
{
    return NSLOCTEXT("McpAutomationBridge", "SettingsSection", "MCP Automation Bridge");
}
