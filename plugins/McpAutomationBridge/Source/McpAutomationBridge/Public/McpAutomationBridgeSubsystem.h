#pragma once

#include <atomic>

#include "Containers/Ticker.h"
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "EditorSubsystem.h"
#include "Engine/DataAsset.h"
#include "HAL/CriticalSection.h"
#include "McpAutomationBridgeLog.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Templates/SharedPointer.h"

class AActor;
class FMcpBridgeWebSocket;
class FMcpNativeTransport;
class UBlueprint;
class USkeleton;
class UMcpAutomationBridgeSubsystem;

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
      const FString& Message,
      const FString& ErrorCode);
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

  void RegisterHandler(const FString& Action, FAutomationHandler Handler);

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

    void Reset()
    {
      ErrorMessages.Empty();
      WarningMessages.Empty();
      ErrorCount = 0;
      WarningCount = 0;
      bErrorMessagesTruncated = false;
      bWarningMessagesTruncated = false;
      bHasErrors = false;
      bHasWarnings = false;
      CapturingThreadId = 0;
      bActive = false;
    }
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
  void QueueAutomationRequest(
      const FString& RequestId,
      const FString& Action,
      const TSharedPtr<FJsonObject>& Payload,
      TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
      ERequestOrigin Origin = ERequestOrigin::WebSocket);

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
  };
  TArray<FPendingAutomationRequest> PendingAutomationRequests;
  FCriticalSection PendingAutomationRequestsMutex;
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
  void InitializeHandlers();
  void StartNativeTransport();
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

  void ProcessAutomationRequest(
      const FString& RequestId,
      const FString& Action,
      const TSharedPtr<FJsonObject>& Payload,
      TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
      ERequestOrigin Origin = ERequestOrigin::WebSocket);

  friend struct FMcpLevelHandlerAccess;
  friend struct FMcpEditorFunctionHandlerAccess;
  friend class FMcpNativeTransport;
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
