#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_BlueprintCreationHandlers.h"
#include "McpAutomationBridge_BlueprintCreationHandlersPrivate.h"

#include "McpAutomationBridgeSubsystem.h"

#if WITH_EDITOR

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Components/ActorComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Factories/BlueprintFactory.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"

#endif

bool FBlueprintCreationHandlers::HandleBlueprintProbeSubobjectHandle(
    UMcpAutomationBridgeSubsystem *Self, const FString &RequestId,
    const TSharedPtr<FJsonObject> &LocalPayload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  check(Self);
  FString ComponentClass;
  LocalPayload->TryGetStringField(TEXT("componentClass"), ComponentClass);
  if (ComponentClass.IsEmpty()) {
    ComponentClass = TEXT("StaticMeshComponent");
  }

#if WITH_EDITOR
  UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
         TEXT("HandleBlueprintAction: blueprint_probe_subobject_handle start "
              "RequestId=%s componentClass=%s"),
         *RequestId, *ComponentClass);

  TSharedPtr<FJsonObject> ResultObject =
      McpHandlerUtils::CreateResultObject();
  ResultObject->SetStringField(TEXT("componentClass"), ComponentClass);
  ResultObject->SetBoolField(TEXT("success"), false);
  ResultObject->SetBoolField(TEXT("subsystemAvailable"), false);

  const FString ProbeFolder = TEXT("/Game/Temp/MCPProbe");
  const FString ProbeName = FString::Printf(
      TEXT("MCP_Probe_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
  UBlueprint *CreatedBlueprint = nullptr;

  UBlueprintFactory *Factory = NewObject<UBlueprintFactory>();
  FAssetToolsModule &AssetToolsModule =
      FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
  UObject *CreatedAsset = AssetToolsModule.Get().CreateAsset(
      ProbeName, ProbeFolder, UBlueprint::StaticClass(), Factory);
  if (!CreatedAsset) {
    TSharedPtr<FJsonObject> Error = McpHandlerUtils::CreateResultObject();
    Error->SetStringField(TEXT("componentClass"), ComponentClass);
    Error->SetStringField(TEXT("error"),
                          TEXT("Failed to create probe blueprint asset"));
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
           TEXT("blueprint_probe_subobject_handle: asset creation failed"));
    Self->SendAutomationResponse(RequestingSocket, RequestId, false,
                                 TEXT("Failed to create probe blueprint"),
                                 Error, TEXT("PROBE_CREATE_FAILED"));
    return true;
  }

  CreatedBlueprint = Cast<UBlueprint>(CreatedAsset);
  if (!CreatedBlueprint) {
    TSharedPtr<FJsonObject> Error = McpHandlerUtils::CreateResultObject();
    Error->SetStringField(TEXT("componentClass"), ComponentClass);
    Error->SetStringField(TEXT("error"),
                          TEXT("Probe asset was not a Blueprint"));
    UE_LOG(
        LogMcpAutomationBridgeSubsystem, Warning,
        TEXT("blueprint_probe_subobject_handle: created asset not blueprint"));
    Self->SendAutomationResponse(
        RequestingSocket, RequestId, false,
        TEXT("Probe asset created was not a Blueprint"), Error,
        TEXT("PROBE_CREATE_FAILED"));
    McpBlueprintCreationHandlers::CleanupProbeAsset(CreatedBlueprint);
    return true;
  }

  UClass *ProbeComponentClass = ResolveClassByName(ComponentClass);
  if (!ProbeComponentClass ||
      !ProbeComponentClass->IsChildOf(UActorComponent::StaticClass())) {
    TSharedPtr<FJsonObject> Error = McpHandlerUtils::CreateResultObject();
    Error->SetStringField(TEXT("componentClass"), ComponentClass);
    Error->SetStringField(
        TEXT("error"),
        TEXT("Component class could not be resolved to a UActorComponent subclass"));
    Self->SendAutomationResponse(RequestingSocket, RequestId, false,
                                 TEXT("Probe component class invalid"), Error,
                                 TEXT("INVALID_COMPONENT_CLASS"));
    McpBlueprintCreationHandlers::CleanupProbeAsset(CreatedBlueprint);
    return true;
  }

  USimpleConstructionScript *ConstructionScript =
      CreatedBlueprint->SimpleConstructionScript;
  if (!ConstructionScript) {
    ConstructionScript =
        NewObject<USimpleConstructionScript>(CreatedBlueprint);
    CreatedBlueprint->SimpleConstructionScript = ConstructionScript;
  }
  if (!ConstructionScript) {
    TSharedPtr<FJsonObject> Error = McpHandlerUtils::CreateResultObject();
    Error->SetStringField(TEXT("componentClass"), ComponentClass);
    Error->SetStringField(
        TEXT("error"),
        TEXT("Failed to create SimpleConstructionScript for probe Blueprint"));
    Self->SendAutomationResponse(RequestingSocket, RequestId, false,
                                 TEXT("Probe SCS unavailable"), Error,
                                 TEXT("SCS_UNAVAILABLE"));
    McpBlueprintCreationHandlers::CleanupProbeAsset(CreatedBlueprint);
    return true;
  }

  const FName ProbeNodeName(TEXT("ProbeComponent"));
  USCS_Node *ProbeNode =
      ConstructionScript->CreateNode(ProbeComponentClass, ProbeNodeName);
  if (!ProbeNode) {
    TSharedPtr<FJsonObject> Error = McpHandlerUtils::CreateResultObject();
    Error->SetStringField(TEXT("componentClass"), ComponentClass);
    Error->SetStringField(TEXT("error"),
                          TEXT("Failed to create probe SCS node"));
    Self->SendAutomationResponse(
        RequestingSocket, RequestId, false,
        TEXT("Probe component node creation failed"), Error,
        TEXT("PROBE_NODE_CREATE_FAILED"));
    McpBlueprintCreationHandlers::CleanupProbeAsset(CreatedBlueprint);
    return true;
  }

  ConstructionScript->AddNode(ProbeNode);
  FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(
      CreatedBlueprint);
  ResultObject->SetStringField(TEXT("componentNodeName"),
                               ProbeNodeName.ToString());

  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
          TEXT("AssetRegistry"));
  AssetRegistryModule.Get().AssetCreated(CreatedBlueprint);

  return McpBlueprintCreationHandlers::SendProbeResults(
      Self, RequestId, RequestingSocket, ResultObject, CreatedBlueprint);
#else
  Self->SendAutomationResponse(RequestingSocket, RequestId, false,
                               TEXT("Blueprint probe requires editor build."),
                               nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
