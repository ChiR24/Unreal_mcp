#pragma once

#include <atomic>

#include "Containers/Ticker.h"
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "EditorSubsystem.h"
#include "Engine/DataAsset.h"
#include "HAL/CriticalSection.h"
#include "McpAutomationBridgeLog.h"
#include "McpQueueFairness.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Templates/SharedPointer.h"

class AActor;
class FMcpBridgeWebSocket;
class FMcpNativeTransport;
class UBlueprint;
class USkeleton;
class UMcpAutomationBridgeSubsystem;
enum class EMcpStateKind : uint8;

namespace McpProcessRequestDispatch
{
bool DispatchFallbackAutomationRequest(
    UMcpAutomationBridgeSubsystem* Bridge,
    const FString& RequestId,
    const FString& Action,
    const FString& LowerAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    FString& OutConsumedHandlerLabel);
}

namespace McpAutomationBridge
{
// Returns the automation action currently executing on the bridge (empty when idle). Thread-safe; readable
// off-thread even while a handler blocks the game thread, so external tooling (e.g. a watchdog) can attribute
// a stall to the tool in flight. Publisher lives in Core/Requests/McpAutomationBridge_ProcessRequest.cpp.
MCPAUTOMATIONBRIDGE_API FString GetInFlightAction();
}

#define MCP_DECLARE_ACTION_HANDLER(Name) bool Name(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
#define MCP_DECLARE_PAYLOAD_HANDLER(Name) bool Name(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
#include "McpAutomationBridgeSubsystemActionRoutingDeclarations.h"
#include "McpAutomationBridgeSubsystemActorControlDeclarations.h"
#include "McpAutomationBridgeSubsystemAssetToolingDeclarations.h"
#include "McpAutomationBridgeSubsystemAssetWorkflowDeclarations.h"
#include "McpAutomationBridgeSubsystemAuthoringDeclarations.h"
#include "McpAutomationBridgeSubsystemDomainRoutingDeclarations.h"
#include "McpAutomationBridgeSubsystemEditorControlDeclarations.h"
#include "McpAutomationBridgeSubsystemEnvironmentMediaDeclarations.h"
#include "McpAutomationBridgeSubsystemGraphSystemDeclarations.h"
#include "McpAutomationBridgeSubsystemPropertyCollectionDeclarations.h"
#include "McpAutomationBridgeSubsystemSequenceDeclarations.h"
#include "McpAutomationBridgeSubsystemSkeletonDeclarations.h"

#include "McpAutomationBridgeSubsystem.generated.h"

#ifndef MCP_HAS_CONTROLRIG_FACTORY
  #if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    #define MCP_HAS_CONTROLRIG_FACTORY 1
  #else
    #define MCP_HAS_CONTROLRIG_FACTORY 0
  #endif
#endif

UCLASS(BlueprintType)
class MCPAUTOMATIONBRIDGE_API UMcpGenericDataAsset : public UDataAsset
{
  GENERATED_BODY()

public:
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MCP Data")
  FString ItemName;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MCP Data")
  FString Description;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MCP Data")
  TMap<FString, FString> Properties;
};

UENUM(BlueprintType)
enum class EMcpAutomationBridgeState : uint8
{
  Disconnected,
  Connecting,
  Connected
};

USTRUCT(BlueprintType)
struct MCPAUTOMATIONBRIDGE_API FMcpAutomationMessage
{
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "MCP Automation")
  FString Type;

  UPROPERTY(BlueprintReadOnly, Category = "MCP Automation")
  FString PayloadJson;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FMcpAutomationMessageReceived,
    const FMcpAutomationMessage&,
    Message);

enum class ERequestOrigin : uint8
{
  WebSocket,
  NativeHTTP
};

enum class EAutomationQueueRejection : uint8
{
    None, NotAccepting, AlreadyCanceled, QueueFull, SessionQueueFull, GameThreadStalled
};

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

  void BroadcastAutomationEvent(
      const TSharedPtr<FJsonObject>& Event,
      TSharedPtr<FMcpBridgeWebSocket> TargetSocket = nullptr);

  UPROPERTY(BlueprintAssignable, Category = "MCP Automation")
  FMcpAutomationMessageReceived OnMessageReceived;

  void SendAutomationResponse(
      TSharedPtr<FMcpBridgeWebSocket> TargetSocket,
      const FString& RequestId,
      bool bSuccess,
      const FString& Message,
      const TSharedPtr<FJsonObject>& Result = nullptr,
      const FString& ErrorCode = FString(),
      ERequestOrigin Origin = ERequestOrigin::WebSocket);
  void SendAutomationError(
      TSharedPtr<FMcpBridgeWebSocket> TargetSocket,
      const FString& RequestId,
      const FString& Message, const FString& ErrorCode);
  void SendAutomationRejection(
      TSharedPtr<FMcpBridgeWebSocket> TargetSocket,
      const FString& RequestId, EAutomationQueueRejection Reason);
  void SendProgressUpdate(
      const FString& RequestId,
      float Percent = -1.0f,
      const FString& Message = TEXT(""),
      bool bStillWorking = true,
      ERequestOrigin Origin = ERequestOrigin::WebSocket);

  bool ExecuteEditorCommands(const TArray<FString>& Commands, FString& OutErrorMessage);
#if MCP_HAS_CONTROLRIG_FACTORY
  UBlueprint* CreateControlRigBlueprint(
      const FString& AssetName,
      const FString& PackagePath,
      USkeleton* TargetSkeleton,
      FString& OutError);
#endif

  using FAutomationHandler = TFunction<bool(
      const FString&,
      const FString&,
      const TSharedPtr<FJsonObject>&,
      TSharedPtr<FMcpBridgeWebSocket>)>;

  bool RegisterHandler(const FString& Action, FAutomationHandler Handler);
  bool RegisterActionAlias(const FString& AliasAction, const FString& TargetAction);

  struct FRequestErrorCapture
  {
    TArray<FString> ErrorMessages;
    TArray<FString> WarningMessages;
    int32 ErrorCount = 0;
    int32 WarningCount = 0;
    bool bErrorMessagesTruncated = false;
    bool bWarningMessagesTruncated = false;
    std::atomic<bool> bHasErrors{false};
    std::atomic<bool> bHasWarnings{false};
    uint32 CapturingThreadId = 0;
    bool bActive = false;

    void Reset();
  };

  FRequestErrorCapture& GetCurrentErrorCapture();
  void BeginErrorCapture();
  TArray<FString> EndErrorCapture();
  bool HasCapturedErrors() const;
  TArray<FString> GetCapturedErrorMessages() const;

  friend class FMcpRequestErrorDevice;

private:
  FRequestErrorCapture CurrentErrorCapture;
  mutable FCriticalSection ErrorCaptureMutex;
  TSharedPtr<class FMcpRequestErrorDevice> RequestErrorDevice;

public:
  bool Tick(float DeltaTime);
  EAutomationQueueRejection QueueAutomationRequest(
      const FString& RequestId,
      const FString& Action,
      const TSharedPtr<FJsonObject>& Payload,
      TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
      ERequestOrigin Origin = ERequestOrigin::WebSocket,
      const TMap<EMcpStateKind, int64>& ExpectedRevisions = TMap<EMcpStateKind, int64>(),
      const FString& SessionKey = FString());
  // Cancel API contract: best-effort, advisory bool return. The function is
  // idempotent and only operates on QUEUED requests — a request that is
  // already executing inside ProcessAutomationRequest cannot be interrupted
  // by this API. The bool is true when the call did any meaningful work
  // (removed a queued request, or invoked a registered cancellation
  // callback). It is false when the request id was already gone. Callers
  // should not branch on the return value to decide whether to retry.
  //
  // For long-running handlers that need cooperative cancellation, the
  // request id can be polled via HasPendingRequest() or via a registered
  // callback (RegisterAutomationRequestCancellation) that fires AFTER the
  // in-flight work has finished. The callback is for "your slot is now
  // free, please release external resources" — not for "abort the work."
  // Returning a richer enum (Cancelled / WasAlreadyDone / NotFound) was
  // considered but rejected to keep the public API surface minimal — none
  // of the current callers branch on the return value.
  bool CancelAutomationRequest(const FString& RequestId);
  bool CancelAutomationRequests(const TArray<FString>& RequestIds);
  bool CancelAllAutomationRequests();
  bool RegisterAutomationRequestCancellation(
      const FString& RequestId,
      TFunction<void()> Callback);
  void ClearAutomationRequestCancellation(const FString& RequestId);
  void DiscardCanceledAutomationRequest(const FString& RequestId);

  TSharedPtr<class FMcpConnectionManager> ConnectionManager;
  TSharedPtr<FMcpNativeTransport> NativeTransport;

  FString CurrentBusyBlueprintKey;
  bool bCurrentBlueprintBusyMarked = false;
  bool bCurrentBlueprintBusyScheduled = false;

  struct FPendingAutomationRequest
  {
    FString RequestId;
    FString Action;
    TSharedPtr<FJsonObject> Payload;
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket;
    ERequestOrigin Origin = ERequestOrigin::WebSocket;
    TMap<EMcpStateKind, int64> ExpectedRevisions;
    FString SessionKey;
  };
  TArray<FPendingAutomationRequest> PendingAutomationRequests;
  TSet<FString> CanceledAutomationRequestIds;
  TSet<FString> InFlightAutomationRequestIds;
  TSet<FString> ActiveAutomationRequestIds;
  TMap<FString, TFunction<void()>> AutomationRequestCancellationCallbacks;
  FCriticalSection PendingAutomationRequestsMutex;
  FCriticalSection AutomationRequestExecutionMutex;
  // Queue-time gate only. StopAcceptingAutomationRequests() prevents new
  // requests from entering the queue but does NOT interrupt requests that are
  // already executing inside ProcessAutomationRequest. To wait for in-flight
  // work to drain, callers should follow stop with CancelAllAutomationRequests()
  // and poll ActiveAutomationRequestIds.IsEmpty() under the request mutex.
  bool bAcceptingAutomationRequests = true;
  static constexpr int32 MaxPendingAutomationRequests = 64;
  static constexpr int32 MaxAutomationRequestsPerTick = 16;
  // Task 45 round-robin rotation + single-mutation-lane guard. See McpQueueFairness.h.
  FMcpQueueFairnessState QueueFairness;
  void ProcessPendingAutomationRequests();

  ERequestOrigin CurrentRequestOrigin = ERequestOrigin::WebSocket;
  void RecordAutomationTelemetry(
      const FString& RequestId,
      bool bSuccess,
      const FString& Message,
      const FString& ErrorCode);

  TSharedPtr<FOutputDevice> LogCaptureDevice;

private:
  FTSTicker::FDelegateHandle TickHandle;
  TMap<FString, FAutomationHandler> AutomationHandlers;
  TSet<FString> AutomationAliasActions;
  TMap<FString, FString> PendingAutomationActionAliases;
  void InitializeHandlers();
  void LoadConfiguredHandlerAliases();
  void StartAcceptingAutomationRequests();
  void StopAcceptingAutomationRequests();
  bool RegisterActionAliasInternal(
      const FString& AliasAction,
      const FString& TargetAction,
      bool bAllowPendingTarget);
  void TryActivatePendingActionAliases(const FString& TargetAction);
  void StartNativeTransport();
  void ReconcileLogCaptureDevice();
  void RegisterCoreAndAssetHandlers();
  void RegisterEnvironmentMediaHandlers();
  void RegisterSystemAndEditorHandlers();
  void RegisterAssetRoutingHandlers();
  void RegisterBlueprintAndDomainHandlers();
  void RegisterAudioAnimationHandlers();
  void RegisterWorldAndMiscHandlers();

  MCP_SUBSYSTEM_PROPERTY_COLLECTION_DECLARATIONS
  MCP_SUBSYSTEM_ACTION_ROUTING_DECLARATIONS
  MCP_SUBSYSTEM_ENVIRONMENT_MEDIA_DECLARATIONS
  MCP_SUBSYSTEM_ASSET_TOOLING_DECLARATIONS
  MCP_SUBSYSTEM_AUTHORING_DECLARATIONS
  MCP_SUBSYSTEM_GRAPH_SYSTEM_DECLARATIONS
  MCP_SUBSYSTEM_SKELETON_DECLARATIONS
  MCP_SUBSYSTEM_DOMAIN_ROUTING_DECLARATIONS
  MCP_SUBSYSTEM_SEQUENCE_DECLARATIONS
  MCP_SUBSYSTEM_ACTOR_CONTROL_DECLARATIONS
  MCP_SUBSYSTEM_EDITOR_CONTROL_DECLARATIONS
  MCP_SUBSYSTEM_ASSET_WORKFLOW_DECLARATIONS

  TMap<FString, FTransform> CachedActorSnapshots;
  bool bProcessingAutomationRequest = false;

  // ExpectedRevisions and SessionKey are carried so the deferral and reentrancy
  // re-queues below can hand them back to QueueAutomationRequest. Without them
  // a re-queued request silently reverted to the parameter defaults: the
  // live-state pins vanished (so the stale-state precondition gate was skipped
  // entirely on the second drain) and the request fell into the anonymous
  // session lane, which is also exempt from the per-session admission cap.
  void ProcessAutomationRequest(
      const FString& RequestId,
      const FString& Action,
      const TSharedPtr<FJsonObject>& Payload,
      TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
      ERequestOrigin Origin = ERequestOrigin::WebSocket,
      const TMap<EMcpStateKind, int64>& ExpectedRevisions = TMap<EMcpStateKind, int64>(),
      const FString& SessionKey = FString());

  friend struct FMcpLevelHandlerAccess;
  friend struct FMcpEditorFunctionHandlerAccess; friend struct FMcpUiHandlerAccess;
  friend class FMcpNativeTransport;
  friend class FMcpCustomHandlerAliasDispatchTest;
  friend class FMcpAutomationShutdownCancellationTest;
  friend bool McpProcessRequestDispatch::DispatchFallbackAutomationRequest(
      UMcpAutomationBridgeSubsystem* Bridge,
      const FString& RequestId,
      const FString& Action,
      const FString& LowerAction,
      const TSharedPtr<FJsonObject>& Payload,
      TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
      FString& OutConsumedHandlerLabel);
};

#undef MCP_SUBSYSTEM_ASSET_WORKFLOW_DECLARATIONS
#undef MCP_SUBSYSTEM_EDITOR_CONTROL_DECLARATIONS
#undef MCP_SUBSYSTEM_ACTOR_CONTROL_DECLARATIONS
#undef MCP_SUBSYSTEM_SEQUENCE_DECLARATIONS
#undef MCP_SUBSYSTEM_DOMAIN_ROUTING_DECLARATIONS
#undef MCP_SUBSYSTEM_SKELETON_DECLARATIONS
#undef MCP_SUBSYSTEM_GRAPH_SYSTEM_DECLARATIONS
#undef MCP_SUBSYSTEM_AUTHORING_DECLARATIONS
#undef MCP_SUBSYSTEM_ASSET_TOOLING_DECLARATIONS
#undef MCP_SUBSYSTEM_ENVIRONMENT_MEDIA_DECLARATIONS
#undef MCP_SUBSYSTEM_ACTION_ROUTING_DECLARATIONS
#undef MCP_SUBSYSTEM_PROPERTY_COLLECTION_DECLARATIONS
#undef MCP_DECLARE_PAYLOAD_HANDLER
#undef MCP_DECLARE_ACTION_HANDLER
