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
    bool IsBridgeActive() const;

    UFUNCTION(BlueprintCallable, Category = "MCP Automation")
    EMcpAutomationBridgeState GetBridgeState() const;

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

    bool ExecuteEditorCommands(const TArray<FString>& Commands, FString& OutErrorMessage);
    #if MCP_HAS_CONTROLRIG_FACTORY
    UBlueprint* CreateControlRigBlueprint(const FString& AssetName, const FString& PackagePath, USkeleton* TargetSkeleton, FString& OutError);
    #endif

    // Automation Handler Delegate
    using FAutomationHandler = TFunction<bool(const FString&, const FString&, const TSharedPtr<FJsonObject>&, TSharedPtr<FMcpBridgeWebSocket>)>;

    /**
     * Registers a handler for a specific automation action.
     * This allows for O(1) dispatch of automation requests and runtime extensibility.
     */
    void RegisterHandler(const FString& Action, FAutomationHandler Handler);

private:
    // Telemetry structs moved to McpConnectionManager

    bool Tick(float DeltaTime);

    // Connection Manager
    TSharedPtr<class FMcpConnectionManager> ConnectionManager;

    // Track a blueprint currently being modified by this subsystem request
    // so scope-exit handlers can reliably clear busy state without
    // attempting to capture local variables inside macros.
    FString CurrentBusyBlueprintKey;
    bool bCurrentBlueprintBusyMarked = false;
    bool bCurrentBlueprintBusyScheduled = false;

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

    // Active Log Device
    TSharedPtr<FOutputDevice> LogCaptureDevice;

    // Action handlers (implemented in separate translation units)
    TMap<FString, FAutomationHandler> AutomationHandlers;
    void InitializeHandlers();

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
    // Helper function to create Niagara effects with default systems
    bool CreateNiagaraEffect(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, const FString& EffectName, const FString& DefaultSystemPath);
    // Asset-related automation actions implemented by the plugin (editor-only operations)
    bool HandleAssetAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    // Asset dependency graph traversal
    bool HandleGetAssetReferences(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleGetAssetDependencies(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    // Actor/editor control actions implemented by the plugin
    bool HandleControlActorAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleControlEditorAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    // Level and lighting helpers (top-level actions)
    bool HandleLevelAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleUiAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    // Animation and physics related automation actions
    bool HandleAnimationPhysicsAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    // Audio related automation actions
    bool HandleAudioAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    // Lighting related automation actions
    bool HandleLightingAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    // Performance related automation actions
    bool HandlePerformanceAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    // Environment building automation actions (landscape, foliage, etc.)
    bool HandleBuildEnvironmentAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleControlEnvironmentAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandlePaintFoliage(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleGetFoliageInstances(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleRemoveFoliage(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    // Asset workflow handlers
    bool HandleSourceControlCheckout(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleSourceControlSubmit(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleFixupRedirectors(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleBulkRenameAssets(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleBulkDeleteAssets(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleGenerateThumbnail(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    // Landscape, foliage, and Niagara handlers
    bool HandleCreateLandscape(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleCreateProceduralTerrain(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleCreateProceduralFoliage(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleCreateLandscapeGrassType(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleGenerateLODs(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleBakeLightmap(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    // Aggregate landscape editor that dispatches to specific edit ops
    bool HandleEditLandscape(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    // Specific landscape edit operations
    bool HandleModifyHeightmap(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandlePaintLandscapeLayer(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleSculptLandscape(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleSetLandscapeMaterial(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
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
    bool HandleAddFoliageInstances(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    // SCS Blueprint authoring handler
    bool HandleSCSAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);

    // Additional consolidated tool handlers
    bool HandleSystemControlAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleConsoleCommandAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleInspectAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);

    // 1. Editor Authoring & Graph Editing
    bool HandleBlueprintGraphAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleNiagaraGraphAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleMaterialGraphAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleBehaviorTreeAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleWorldPartitionAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleRenderAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleListBlueprints(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);


    // 2. Execution & Build / Test Pipeline
    bool HandlePipelineAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleTestAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);

    // 3. Observability, Logs, Debugging & History
    bool HandleLogAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleDebugAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleAssetQueryAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    bool HandleInsightsAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);

    // 4. Input, UI, Hotkeys & Dialogs
    bool HandleUiAutomationAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);

private:
    // Sequence helpers
    FString ResolveSequencePath(const TSharedPtr<FJsonObject>& Payload);
    TSharedPtr<FJsonObject> EnsureSequenceEntry(const FString& SeqPath);

    // Individual sequence action handlers
    bool HandleSequenceCreate(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleSequenceSetDisplayRate(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleSequenceSetProperties(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleSequenceOpen(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleSequenceAddCamera(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleSequencePlay(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleSequenceAddActor(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleSequenceAddActors(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleSequenceAddSpawnable(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleSequenceRemoveActors(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleSequenceGetBindings(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleSequenceGetProperties(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleSequenceSetPlaybackSpeed(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleSequencePause(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleSequenceStop(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleSequenceList(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleSequenceDuplicate(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleSequenceRename(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleSequenceDelete(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleSequenceGetMetadata(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleSequenceAddKeyframe(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);

    // Control handlers
    AActor* FindActorByName(const FString& Target);
    
    // Control Actor Subhandlers
    bool HandleControlActorSpawn(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleControlActorSpawnBlueprint(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleControlActorDelete(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleControlActorApplyForce(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleControlActorSetTransform(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleControlActorGetTransform(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleControlActorSetVisibility(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleControlActorAddComponent(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleControlActorSetComponentProperties(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleControlActorGetComponents(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleControlActorDuplicate(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleControlActorAttach(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleControlActorDetach(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleControlActorFindByTag(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleControlActorAddTag(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleControlActorRemoveTag(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleControlActorFindByName(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleControlActorDeleteByTag(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleControlActorSetBlueprintVariables(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleControlActorCreateSnapshot(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleControlActorList(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleControlActorGet(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleControlActorRestoreSnapshot(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleControlActorExport(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleControlActorGetBoundingBox(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleControlActorGetMetadata(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);

    // Control Editor Subhandlers
    bool HandleControlEditorPlay(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleControlEditorStop(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleControlEditorFocusActor(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleControlEditorSetCamera(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleControlEditorSetViewMode(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleControlEditorOpenAsset(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);

    // Asset handlers
    bool HandleImportAsset(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleCreateMaterial(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleCreateMaterialInstance(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleCreateNiagaraSystemAsset(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleDuplicateAsset(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleRenameAsset(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleMoveAsset(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleDeleteAssets(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleListAssets(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleCreateFolder(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleGetDependencies(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleGetAssetGraph(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleCreateThumbnail(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleSetTags(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleSetMetadata(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleGetMetadata(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleGenerateReport(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleValidateAsset(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleAddMaterialParameter(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleListMaterialInstances(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleResetInstanceParameters(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleDoesAssetExist(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
    bool HandleGetMaterialStats(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);

    // Lightweight snapshot cache for automation requests (e.g., create_snapshot)
    TMap<FString, FTransform> CachedActorSnapshots;

    /** Guards against reentrant automation request processing */
    bool bProcessingAutomationRequest = false;

    void ProcessAutomationRequest(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
};
