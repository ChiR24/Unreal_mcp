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
// Null, with OutError, when Context names a parent class that resolves to nothing.
UFactory* CreateBlueprintFactory(const FRequestContext& Context, FString& OutError);
// Sets the payload `properties` on the class default object; every name lands in one of the lists.
void ApplyBlueprintProperties(
    UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& Payload,
    TArray<FString>& OutApplied, TArray<FString>& OutFailed);
TSharedPtr<FJsonObject> BuildBlueprintResult(
    UBlueprint* Blueprint, const FString& NormalizedPath);
}
