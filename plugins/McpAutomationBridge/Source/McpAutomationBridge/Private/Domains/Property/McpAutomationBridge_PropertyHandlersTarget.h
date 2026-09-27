#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FMcpBridgeWebSocket;
class UBlueprint;
class UMcpAutomationBridgeSubsystem;

namespace McpPropertyTarget
{
// What get_object_property / set_object_property operate on.
struct FPropertyTarget
{
  FString ObjectPath;     // the resolved object's path
  FString BlueprintPath;  // as the caller gave it
  FString PropertyName;   // propertyName, else propertyPath
  UObject* RootObject = nullptr;
  UBlueprint* Blueprint = nullptr;  // blueprintPath's, or the owner of a CDO objectPath
};

// Reads objectPath|blueprintPath and propertyName|propertyPath and resolves the
// object: the Blueprint's CDO, or the object at objectPath (recovering its
// Blueprint when that object is a CDO). Refuses a target outside
// McpSafeReflectionTarget before any property is touched. False after replying.
bool ResolvePropertyTarget(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId,
                           const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket,
                           FPropertyTarget& Out);
}
