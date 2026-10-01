#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FMcpBridgeWebSocket;
class FProperty;
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

// True for an object a Blueprint compile left behind: the REINST_/SKEL_/TRASHCLASS_ copy of a class (or one a newer
// class replaced), or an object of a Blueprint class in the transient package while no game runs. Nothing reads such an
// object and nothing saves it, so a write there changes no default and used to answer success all the same.
bool IsSupersededTarget(const UObject* Object);

// A Blueprint variable keeps its default as text (the variable's DefaultValue) and every compile writes that text back into
// the new default object, so a write to the object alone is gone after the compile. Sets the text of the variable
// PropertyPath starts with (a member of a declared struct changes the whole variable) from DefaultObject; does nothing
// when the Blueprint does not declare that variable.
void KeepDeclaredDefault(UBlueprint* Blueprint, UObject* DefaultObject, const FString& PropertyPath);

// A compile replaces the class and its default object, so the one a write went to is a stale copy afterwards. Finds
// PropertyPath again on the Blueprint's current default object; false, outputs untouched, when it has none or lost the
// property.
bool FindOnCurrentDefault(UBlueprint* Blueprint, const FString& PropertyPath, UObject*& OutObject,
                          FProperty*& OutProperty, void*& OutContainer);

// Reads objectPath|blueprintPath and propertyName|propertyPath and resolves the
// object: the Blueprint's current CDO, or the object at objectPath (recovering its
// Blueprint when that object is a CDO, and the Blueprint's current CDO when it is
// the stale one a compile left). Refuses a target outside McpSafeReflectionTarget
// before any property is touched. False after replying.
bool ResolvePropertyTarget(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId,
                           const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket,
                           FPropertyTarget& Out);
}
