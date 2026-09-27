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

bool ExecuteBlueprintCreation(UMcpAutomationBridgeSubsystem* Self,
                              const FRequestContext& Context);
UFactory* CreateBlueprintFactory(const FRequestContext& Context);
void ApplyBlueprintProperties(
    UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& Payload);
TSharedPtr<FJsonObject> BuildBlueprintResult(
    UBlueprint* Blueprint, const FString& NormalizedPath);
}
