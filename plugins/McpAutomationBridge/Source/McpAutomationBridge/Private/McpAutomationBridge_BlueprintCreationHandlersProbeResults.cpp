#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_BlueprintCreationHandlersPrivate.h"

#if WITH_EDITOR

#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "McpAutomationBridgeSubsystem.h"

#if defined(MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM) &&                               \
    (MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM == 1)
#if defined(__has_include)
#if __has_include("Subsystems/SubobjectDataSubsystem.h")
#include "Subsystems/SubobjectDataSubsystem.h"
#elif __has_include("SubobjectDataSubsystem.h")
#include "SubobjectDataSubsystem.h"
#elif __has_include("SubobjectData/SubobjectDataSubsystem.h")
#include "SubobjectData/SubobjectDataSubsystem.h"
#endif
#else
#include "SubobjectDataSubsystem.h"
#endif
#elif !defined(MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM)
#if defined(__has_include)
#if __has_include("Subsystems/SubobjectDataSubsystem.h")
#include "Subsystems/SubobjectDataSubsystem.h"
#define MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM 1
#elif __has_include("SubobjectDataSubsystem.h")
#include "SubobjectDataSubsystem.h"
#define MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM 1
#elif __has_include("SubobjectData/SubobjectDataSubsystem.h")
#include "SubobjectData/SubobjectDataSubsystem.h"
#define MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM 1
#else
#define MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM 0
#endif
#else
#define MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM 0
#endif
#else
#define MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM 0
#endif

namespace McpBlueprintCreationHandlers {

void CleanupProbeAsset(UBlueprint *ProbeBlueprint) {
  if (!ProbeBlueprint) {
    return;
  }

  const FString AssetPath = ProbeBlueprint->GetPathName();
  if (!UEditorAssetLibrary::DeleteLoadedAsset(ProbeBlueprint)) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
           TEXT("Failed to delete loaded probe asset: %s"), *AssetPath);
  }

  if (!AssetPath.IsEmpty() &&
      UEditorAssetLibrary::DoesAssetExist(AssetPath) &&
      !UEditorAssetLibrary::DeleteAsset(AssetPath)) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
           TEXT("Failed to delete probe asset file: %s"), *AssetPath);
  }
}

bool SendProbeResults(
    UMcpAutomationBridgeSubsystem *Self, const FString &RequestId,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    const TSharedPtr<FJsonObject> &ResultObject,
    UBlueprint *CreatedBlueprint) {
#if MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM
  USubobjectDataSubsystem *Subsystem =
      GEngine ? GEngine->GetEngineSubsystem<USubobjectDataSubsystem>()
              : nullptr;
  if (Subsystem) {
    ResultObject->SetBoolField(TEXT("subsystemAvailable"), true);

    TArray<FSubobjectDataHandle> GatheredHandles;
    Subsystem->K2_GatherSubobjectDataForBlueprint(CreatedBlueprint,
                                                  GatheredHandles);

    TArray<TSharedPtr<FJsonValue>> HandleValues;
    const UScriptStruct *HandleStruct = FSubobjectDataHandle::StaticStruct();
    for (int32 Index = 0; Index < GatheredHandles.Num(); ++Index) {
      const FSubobjectDataHandle &Handle = GatheredHandles[Index];
      const FString Representation =
          HandleStruct
              ? FString::Printf(TEXT("%s@%p"), *HandleStruct->GetName(),
                                (void *)&Handle)
              : FString::Printf(TEXT("<subobject_handle_%d>"), Index);
      HandleValues.Add(
          MakeShared<FJsonValueString>(Representation));
    }
    if (HandleValues.Num() == 0) {
      ResultObject->SetStringField(
          TEXT("error"),
          TEXT("SubobjectDataSubsystem returned no handles for probe Blueprint"));
      CleanupProbeAsset(CreatedBlueprint);
      Self->SendAutomationResponse(RequestingSocket, RequestId, false,
                                   TEXT("Probe produced no component handles"),
                                   ResultObject, TEXT("PROBE_NO_HANDLES"));
      return true;
    }

    ResultObject->SetArrayField(TEXT("gatheredHandles"), HandleValues);
    ResultObject->SetNumberField(TEXT("handleCount"), HandleValues.Num());
    ResultObject->SetBoolField(TEXT("hasHandles"), true);
    ResultObject->SetBoolField(TEXT("success"), true);
    CleanupProbeAsset(CreatedBlueprint);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true,
                                 TEXT("Native probe completed"), ResultObject,
                                 FString());
    return true;
  }
#endif

  ResultObject->SetBoolField(TEXT("subsystemAvailable"), false);
  TArray<TSharedPtr<FJsonValue>> HandleValues;
  if (CreatedBlueprint && CreatedBlueprint->SimpleConstructionScript) {
    const TArray<USCS_Node *> &Nodes =
        CreatedBlueprint->SimpleConstructionScript->GetAllNodes();
    for (USCS_Node *Node : Nodes) {
      if (!Node || !Node->GetVariableName().IsValid()) {
        continue;
      }
      HandleValues.Add(MakeShared<FJsonValueString>(FString::Printf(
          TEXT("scs://%s"), *Node->GetVariableName().ToString())));
    }
  }

  if (HandleValues.Num() == 0) {
    ResultObject->SetStringField(
        TEXT("error"),
        TEXT("No subobject handles or SCS nodes were gathered from probe Blueprint"));
    CleanupProbeAsset(CreatedBlueprint);
    Self->SendAutomationResponse(RequestingSocket, RequestId, false,
                                 TEXT("Probe produced no component handles"),
                                 ResultObject, TEXT("PROBE_NO_HANDLES"));
    return true;
  }

  ResultObject->SetArrayField(TEXT("gatheredHandles"), HandleValues);
  ResultObject->SetNumberField(TEXT("handleCount"), HandleValues.Num());
  ResultObject->SetBoolField(TEXT("hasHandles"), true);
  ResultObject->SetBoolField(TEXT("success"), true);
  CleanupProbeAsset(CreatedBlueprint);
  Self->SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Fallback probe completed"), ResultObject,
                               FString());
  return true;
}

}

#endif
