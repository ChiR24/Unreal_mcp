#pragma once

#include "Containers/Ticker.h"
#include "EditorSubsystem.h"
#include "Templates/SharedPointer.h"
#include "Dom/JsonObject.h"
#include "Logging/LogMacros.h"
#include "HAL/CriticalSection.h"
#include "McpAutomationBridgeSubsystem.generated.h"

UENUM(BlueprintType)
enum class EMcpAutomationBridgeState : uint8
{
    Disconnected,
    Connecting,
    Connected
};

/** Minimal payload wrapper for incoming automation messages. */
USTRUCT(BlueprintType)
struct MCPAUTOMATIONBRIDGE_API FMcpAutomationMessage
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "MCP Automation")
    FString Type;

    UPROPERTY(BlueprintReadOnly, Category = "MCP Automation")
    FString PayloadJson;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMcpAutomationMessageReceived, const FMcpAutomationMessage&, Message);

class FMcpBridgeWebSocket;
DECLARE_LOG_CATEGORY_EXTERN(LogMcpAutomationBridgeSubsystem, Log, All);

UCLASS()
class MCPAUTOMATIONBRIDGE_API UMcpAutomationBridgeSubsystem : public UEditorSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category = "MCP Automation")
    bool IsBridgeActive() const { return bBridgeAvailable; }

    UFUNCTION(BlueprintCallable, Category = "MCP Automation")
    EMcpAutomationBridgeState GetBridgeState() const { return BridgeState; }

    UFUNCTION(BlueprintCallable, Category = "MCP Automation")
    bool SendRawMessage(const FString& Message);

    UPROPERTY(BlueprintAssignable, Category = "MCP Automation")
    FMcpAutomationMessageReceived OnMessageReceived;

    /**
     * Deprecated: Controls whether editor Python fallbacks (execute_editor_python)
     * are accepted and executed by the plugin. Default is false. When disabled
     * the plugin will reject execute_editor_python requests with
     * PYTHON_FALLBACK_DISABLED so callers can fail-fast and prefer native
     * automation actions implemented by the plugin.
     */
    UFUNCTION(BlueprintCallable, Category = "MCP Automation")
    void SetAllowEditorPythonExecution(bool bEnable);

    UFUNCTION(BlueprintCallable, Category = "MCP Automation")
    bool IsEditorPythonExecutionAllowed() const { return bAllowPythonFallbacks; }

    // Public helpers for sending automation responses/errors. These need to be
    // callable from out-of-line helper functions and translation-unit-level
    // handlers that receive a UMcpAutomationBridgeSubsystem* (e.g. static
    // blueprint helper routines). They were previously declared private which
    // prevented those helpers from invoking them via a 'Self' pointer.
    void SendAutomationResponse(TSharedPtr<FMcpBridgeWebSocket> TargetSocket, const FString& RequestId, bool bSuccess, const FString& Message, const TSharedPtr<FJsonObject>& Result = nullptr, const FString& ErrorCode = FString());
    void SendAutomationError(TSharedPtr<FMcpBridgeWebSocket> TargetSocket, const FString& RequestId, const FString& Message, const FString& ErrorCode);

private:
    bool Tick(float DeltaTime);

    void AttemptConnection();
    void HandleConnected(TSharedPtr<FMcpBridgeWebSocket> Socket);
    void HandleClientConnected(TSharedPtr<FMcpBridgeWebSocket> ClientSocket);
    void HandleConnectionError(TSharedPtr<FMcpBridgeWebSocket> Socket, const FString& Error);
    void HandleServerConnectionError(const FString& Error);
    void HandleClosed(TSharedPtr<FMcpBridgeWebSocket> Socket, int32 StatusCode, const FString& Reason, bool bWasClean);
    void HandleMessage(TSharedPtr<FMcpBridgeWebSocket> Socket, const FString& Message);
    void HandleHeartbeat(TSharedPtr<FMcpBridgeWebSocket> Socket);
    void ProcessAutomationRequest(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);

    void StartBridge();
    void StopBridge();

    bool bBridgeAvailable = false;
    EMcpAutomationBridgeState BridgeState = EMcpAutomationBridgeState::Disconnected;
    FTSTicker::FDelegateHandle TickerHandle;
    TArray<TSharedPtr<FMcpBridgeWebSocket>> ActiveSockets;
    TMap<FString, TSharedPtr<FMcpBridgeWebSocket>> PendingRequestsToSockets;
    float TimeUntilReconnect = 0.0f;
    float AutoReconnectDelaySeconds = 5.0f;
    FString CapabilityToken;
    FString EndpointUrl;
    bool bReconnectEnabled = true;

    FString ServerName;
    FString ServerVersion;
    FString ActiveSessionId;
    // Environment override values (optional)
    FString EnvListenPorts;
    FString EnvListenHost;
    bool bEnvListenPortsSet = false;
    float HeartbeatTimeoutSeconds = 0.0f;
    double LastHeartbeatTimestamp = 0.0;
    bool bHeartbeatTrackingEnabled = false;

    // Client port (optional), read from settings
    int32 ClientPort = 0;

    // Track a blueprint currently being modified by this subsystem request
    // so scope-exit handlers can reliably clear busy state without
    // attempting to capture local variables inside macros.
    FString CurrentBusyBlueprintKey;
    bool bCurrentBlueprintBusyMarked = false;
    bool bCurrentBlueprintBusyScheduled = false;

    // Whether an incoming capability token is required
    bool bRequireCapabilityToken = false;

    /** When true, allow any Python script to be executed when Python fallbacks
     * are enabled. This is an additional, explicit opt-in beyond the
     * deprecated bAllowPythonFallbacks flag and restricts ExecPythonCommand
     * to administrators who intentionally enable it. Default: false. */
    bool bAllowAllPythonFallbacks = false;

    /** If non-empty, only Python scripts containing one of these substrings
     * will be permitted when Python fallbacks are enabled. Acts as an
     * audited allowlist for remaining Python fallbacks. */
    TArray<FString> AllowedPythonScriptAllowlist;

    void RecordHeartbeat();
    void ResetHeartbeatTracking();
    void ForceReconnect(const FString& Reason, float ReconnectDelayOverride = -1.0f);
    void SendControlMessage(const TSharedPtr<FJsonObject>& Message);

    // Pending automation request queue (thread-safe). Inbound socket threads
    // will enqueue requests here; the queue is drained sequentially on the
    // game thread to ensure deterministic processing order and avoid
    // reentrancy issues.
    struct FPendingAutomationRequest
    {
        FString RequestId;
        FString Action;
        TSharedPtr<FJsonObject> Payload;
        TSharedPtr<FMcpBridgeWebSocket> RequestingSocket;
    };
    TArray<FPendingAutomationRequest> PendingAutomationRequests;
    FCriticalSection PendingAutomationRequestsMutex;
    bool bPendingRequestsScheduled = false;
    void ProcessPendingAutomationRequests();

    // Action handlers (implemented in separate translation units)
    bool HandleExecuteEditorPython(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    /**
     * Handle lightweight, well-known editor function invocations sent from the
     * server. This action is intended as a native replacement for the
     * execute_editor_python fallback for common scripted templates (spawn,
     * delete, list actors, set viewport camera, asset existence checks, etc.).
     * When the plugin implements a native function we will handle it here and
     * avoid executing arbitrary Python inside the editor.
     */
    bool HandleExecuteEditorFunction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleSetObjectProperty(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleGetObjectProperty(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleBlueprintAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleSequenceAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    // Effect-related automation actions (Niagara, debug shapes, dynamic lights)
    bool HandleEffectAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    // Asset-related automation actions implemented by the plugin (editor-only operations)
    bool HandleAssetAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    // Actor/editor control actions implemented by the plugin
    bool HandleControlActorAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleControlEditorAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);

    // Persist an audit entry for allowed raw Python executions so administrators
    // can review which scripts were executed via the deprecated fallback.
    void AppendAuditLog(const FString& RequestId, uint32 ScriptHash, const FString& RequesterAddr, const FString& ScriptSnippet);

private:
    /** When true, the subsystem will permit executing editor Python via the
     * execute_editor_python handler. This is intended to be a temporary,
     * opt-in compatibility flag for migration and debugging; prefer native
     * plugin handlers and keep this disabled in production. */
    bool bAllowPythonFallbacks = false;
    /** Guards against reentrant automation request processing */
    bool bProcessingAutomationRequest = false;
};
