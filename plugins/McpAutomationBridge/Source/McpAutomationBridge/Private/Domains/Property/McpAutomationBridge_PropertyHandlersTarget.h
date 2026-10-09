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
// PropertyPath again on the Blueprint's current default object, or on its default subobject SubobjectName (a native
// component of the class defaults, which the compile replaced too); false, outputs untouched, when it has none or lost
// the property.
bool FindOnCurrentDefault(UBlueprint* Blueprint, const FString& PropertyPath, UObject*& OutObject,
                          FProperty*& OutProperty, void*& OutContainer, FName SubobjectName = NAME_None);

// Property's value in Container as text, the form a write is compared by once the object's own change handling or a
// Blueprint compile ran.
FString ValueText(FProperty* Property, void* Container);

// Writes Value through the component's own setter when the key has one (a StaticMeshComponent's StaticMesh, collision,
// SimulatePhysics, Mobility), as control_actor edit_component does, else straight into Container. The component is the
// root, or the one a placed actor's path names first ("StaticMeshComponent0.StaticMesh"): a raw StaticMesh write
// tripped the engine's KnownStaticMesh check, and a raw CollisionEnabled was put back by the mesh's default collision.
bool WriteValue(UObject* Root, const FString& Path, FProperty* Property, void* Container,
                const TSharedPtr<FJsonValue>& Value, FString& OutError);

// Tells the owner of an instanced subobject (a water body's wave generator) that the property holding it changed, as
// the details panel does, up the chain of owners: the waves were not regenerated when only the generator heard it.
void NotifyOwners(UObject* Edited);

// Saves the package a write landed in when it is a project asset (a Blueprint target through its Blueprint: a
// component a compile moved aside sits in /Engine/Transient). Otherwise false with OutSkipReason saying why nothing
// was saved: markDirty off, running-game or transient content, level content (saved with its level), engine content,
// or a save that failed.
bool SaveAfterWrite(UObject* Target, UBlueprint* Blueprint, bool bMarkDirty, FString& OutSkipReason);

// Reads objectPath|blueprintPath and propertyName|propertyPath and resolves the
// object: the Blueprint's current CDO, or the object at objectPath (recovering its
// Blueprint when that object is a CDO, and the Blueprint's current CDO when it is
// the stale one a compile left, or the Blueprint asset itself when the property is
// not one of the asset's own). Refuses a target outside McpSafeReflectionTarget
// before any property is touched. False after replying.
bool ResolvePropertyTarget(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId,
                           const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket,
                           FPropertyTarget& Out);

// One write of a batch: set_object_property run again on One under the captured id ItemId.
using FSetBatchWrite = TFunctionRef<void(const FString& ItemId, const TSharedPtr<FJsonObject>& One)>;

struct FSetBatchReply
{
  bool bSuccess = true;
  FString Message;
  FString ErrorCode;
  TSharedPtr<FJsonObject> Data;
};

// set_object_property's batches: properties (several values on one target, rows under properties) and objectPaths (the
// same write, or the same properties, on several targets, rows under targets). Every write runs through RunOne, so it
// keeps each check a single call makes; a watch stays a single-write feature. A write that did not apply fails the call
// with PROPERTY_BATCH_INCOMPLETE naming it. False, Out untouched, when Payload asks for no batch.
bool RunSetBatch(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FSetBatchWrite RunOne, FSetBatchReply& Out);
}
