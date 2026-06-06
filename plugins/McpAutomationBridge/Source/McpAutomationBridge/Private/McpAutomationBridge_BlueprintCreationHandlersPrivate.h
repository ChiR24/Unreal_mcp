#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FMcpBridgeWebSocket;
class UBlueprint;
class UFactory;
class UMcpAutomationBridgeSubsystem;

namespace McpBlueprintCreationHandlers {
struct FRequestContext {
  FString RequestId;
  TSharedPtr<FJsonObject> Payload;
  TSharedPtr<FMcpBridgeWebSocket> RequestingSocket;
  FString Name;
  FString SavePath;
  FString ParentClassSpec;
  FString BlueprintTypeSpec;
  FString CreateKey;
};

#if WITH_EDITOR
bool ExecuteBlueprintCreation(UMcpAutomationBridgeSubsystem* Self,
                              const FRequestContext& Context);
UFactory* CreateBlueprintFactory(const FRequestContext& Context);
void ApplyBlueprintProperties(
    UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& Payload);
TSharedPtr<FJsonObject> BuildBlueprintResult(
    UBlueprint* Blueprint, const FString& NormalizedPath);
bool CompleteInflightRequest(
    UMcpAutomationBridgeSubsystem* Self, const FRequestContext& Context,
    bool bSuccess, const FString& Message,
    const TSharedPtr<FJsonObject>& ResultPayload, const FString& ErrorCode);
void CleanupProbeAsset(UBlueprint* ProbeBlueprint);
bool SendProbeResults(
    UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    const TSharedPtr<FJsonObject>& ResultObject, UBlueprint* CreatedBlueprint);
#endif
}
