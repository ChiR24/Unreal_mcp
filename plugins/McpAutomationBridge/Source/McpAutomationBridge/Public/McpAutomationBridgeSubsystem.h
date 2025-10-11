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
    void SendAutomationResponse(TSharedPtr<FMcpBridgeWebSocket> TargetSocket, const FString& RequestId, bool bSuccess, const FString& Message, const TSharedPtr<FJsonObject>& Result = nullptr, const FString& ErrorCode = FString());
    void SendAutomationError(TSharedPtr<FMcpBridgeWebSocket> TargetSocket, const FString& RequestId, const FString& Message, const FString& ErrorCode);

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

private:
    /** Guards against reentrant automation request processing */
    bool bProcessingAutomationRequest = false;
};
