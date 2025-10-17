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

    // Public helpers for sending automation responses/errors. These need to be
    // callable from out-of-line helper functions and translation-unit-level
    // handlers that receive a UMcpAutomationBridgeSubsystem* (e.g. static
    // blueprint helper routines). They were previously declared private which
    // prevented those helpers from invoking them via a 'Self' pointer.
    void SendAutomationResponse(TSharedPtr<FMcpBridgeWebSocket> TargetSocket, const FString& RequestId, bool bSuccess, const FString& Message, const TSharedPtr<FJsonObject>& Result = nullptr, const FString& ErrorCode = FString());
    void SendAutomationError(TSharedPtr<FMcpBridgeWebSocket> TargetSocket, const FString& RequestId, const FString& Message, const FString& ErrorCode);

private:
    struct FAutomationRequestTelemetry
    {
        FString Action;
        double StartTimeSeconds = 0.0;
    };

    struct FAutomationActionStats
    {
        int32 SuccessCount = 0;
        int32 FailureCount = 0;
        double TotalSuccessDurationSeconds = 0.0;
        double TotalFailureDurationSeconds = 0.0;
        double LastDurationSeconds = 0.0;
        double LastUpdatedSeconds = 0.0;
    };

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

    void RecordAutomationTelemetry(const FString& RequestId, bool bSuccess, const FString& Message, const FString& ErrorCode);
    void EmitAutomationTelemetrySummaryIfNeeded(double NowSeconds);

    // Action handlers (implemented in separate translation units)
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
    // Array manipulation operations
    bool HandleArrayAppend(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleArrayRemove(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleArrayInsert(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleArrayGetElement(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleArraySetElement(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleArrayClear(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    // Map manipulation operations
    bool HandleMapSetValue(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleMapGetValue(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleMapRemoveKey(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleMapHasKey(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleMapGetKeys(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleMapClear(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    // Set manipulation operations
    bool HandleSetAdd(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleSetRemove(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleSetContains(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleSetClear(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleBlueprintAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleSequenceAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    // Effect-related automation actions (Niagara, debug shapes, dynamic lights)
    bool HandleEffectAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    // Asset-related automation actions implemented by the plugin (editor-only operations)
    bool HandleAssetAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    // Asset dependency graph traversal
    bool HandleGetAssetReferences(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleGetAssetDependencies(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    // Actor/editor control actions implemented by the plugin
    bool HandleControlActorAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleControlEditorAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    // Animation and physics related automation actions
    bool HandleAnimationPhysicsAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    // Environment building automation actions (landscape, foliage, etc.)
    bool HandleBuildEnvironmentAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    // Asset workflow handlers
    bool HandleSourceControlCheckout(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleSourceControlSubmit(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleFixupRedirectors(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleBulkRenameAssets(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleBulkDeleteAssets(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleGenerateThumbnail(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    // Landscape, foliage, and Niagara handlers
    bool HandleCreateLandscape(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    // Aggregate landscape editor that dispatches to specific edit ops
    bool HandleEditLandscape(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    // Specific landscape edit operations
    bool HandleModifyHeightmap(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandlePaintLandscapeLayer(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandlePaintFoliage(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleCreateNiagaraSystemNative(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleAddSequencerKeyframe(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleManageSequencerTrack(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleCreateAnimBlueprint(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleCreateMaterialNodes(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    // Niagara system handlers
    bool HandleCreateNiagaraSystem(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleCreateNiagaraEmitter(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleSpawnNiagaraActor(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleModifyNiagaraParameter(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    // Animation blueprint handlers
    bool HandlePlayAnimMontage(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleSetupRagdoll(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    // Material graph handlers
    bool HandleAddMaterialTextureSample(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleAddMaterialExpression(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    // Sequencer track handlers
    bool HandleAddCameraTrack(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleAddAnimationTrack(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleAddTransformTrack(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    // Foliage handlers
    bool HandleAddFoliageType(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleRemoveFoliage(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleGetFoliageInstances(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    // SCS Blueprint authoring handler
    bool HandleSCSAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);

private:
    // Lightweight snapshot cache for automation requests (e.g., create_snapshot)
    TMap<FString, FTransform> CachedActorSnapshots;

    TMap<FString, FAutomationRequestTelemetry> ActiveRequestTelemetry;
    TMap<FString, FAutomationActionStats> AutomationActionTelemetry;
    double TelemetrySummaryIntervalSeconds = 120.0;
    double LastTelemetrySummaryLogSeconds = 0.0;

    /** Guards against reentrant automation request processing */
    bool bProcessingAutomationRequest = false;
};
