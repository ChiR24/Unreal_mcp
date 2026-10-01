#include "Domains/Property/McpAutomationBridge_PropertyHandlersTarget.h"

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Safety/McpSafeReflectionTarget.h"

#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/UnrealType.h"

namespace McpPropertyTarget
{
bool IsSupersededTarget(const UObject* Object)
{
  const UClass* Class = Object->GetClass();
  const FString ClassName = Class->GetName();
  if (Class->HasAnyClassFlags(CLASS_NewerVersionExists) || ClassName.StartsWith(TEXT("REINST_")) ||
      ClassName.StartsWith(TEXT("SKEL_")) || ClassName.StartsWith(TEXT("TRASHCLASS_"))) {
    return true;
  }
  // A running game keeps its objects in the transient package (GameInstance, a widget); outside it, a Blueprint object
  // there is what a compile moved aside.
  return UBlueprint::GetBlueprintFromClass(Class) && Object->GetOutermost() == GetTransientPackage() &&
         !(GEditor && GEditor->PlayWorld);
}

void KeepDeclaredDefault(UBlueprint* Blueprint, UObject* DefaultObject, const FString& PropertyPath)
{
  const int32 Dot = PropertyPath.Find(TEXT("."));
  const FString Head = Dot == INDEX_NONE ? PropertyPath : PropertyPath.Left(Dot);
  const FProperty* Variable = DefaultObject->GetClass()->FindPropertyByName(FName(*Head));
  const int32 Index = Variable ? FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, Variable->GetFName()) : INDEX_NONE;
  if (Index == INDEX_NONE) {
    return;
  }
  FString Text;
  MCP_PROPERTY_EXPORT_TEXT(Variable, Text, Variable->ContainerPtrToValuePtr<void>(DefaultObject), nullptr, nullptr, PPF_None);
  Blueprint->NewVariables[Index].DefaultValue = Text;
}

bool FindOnCurrentDefault(UBlueprint* Blueprint, const FString& PropertyPath, UObject*& OutObject,
                          FProperty*& OutProperty, void*& OutContainer)
{
  UClass* Current = Blueprint->GeneratedClass;
  UObject* Fresh = Current ? Current->GetDefaultObject() : nullptr;
  void* Container = nullptr;
  FString ResolvedPath, Error;
  FProperty* Found = Fresh ? McpResolvePropertyPath(Fresh, PropertyPath, Container, ResolvedPath, Error) : nullptr;
  if (!Found || !Container) {
    return false;
  }
  OutObject = Fresh;
  OutProperty = Found;
  OutContainer = Container;
  return true;
}

bool ResolvePropertyTarget(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId,
                           const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket,
                           FPropertyTarget& Out)
{
  if (!Payload.IsValid()) {
    Bridge.SendAutomationError(Socket, RequestId, TEXT("Property request payload missing."), TEXT("INVALID_PAYLOAD"));
    return false;
  }
  Payload->TryGetStringField(TEXT("objectPath"), Out.ObjectPath);
  Out.ObjectPath.TrimStartAndEndInline();
  Payload->TryGetStringField(TEXT("blueprintPath"), Out.BlueprintPath);
  Out.BlueprintPath.TrimStartAndEndInline();
  if (Out.ObjectPath.IsEmpty() && Out.BlueprintPath.IsEmpty()) {
    Bridge.SendAutomationError(Socket, RequestId,
                               TEXT("A target is required: objectPath, actorName, name or blueprintPath."),
                               TEXT("INVALID_OBJECT"));
    return false;
  }
  Payload->TryGetStringField(TEXT("propertyName"), Out.PropertyName);
  Out.PropertyName.TrimStartAndEndInline();
  if (Out.PropertyName.IsEmpty()) {
    Payload->TryGetStringField(TEXT("propertyPath"), Out.PropertyName);
    Out.PropertyName.TrimStartAndEndInline();
  }
  if (Out.PropertyName.IsEmpty()) {
    Bridge.SendAutomationError(Socket, RequestId, TEXT("propertyName or propertyPath is required."),
                               TEXT("INVALID_PROPERTY"));
    return false;
  }

  if (!Out.BlueprintPath.IsEmpty()) {
    FString NormalizedPath, LoadError;
    Out.Blueprint = LoadBlueprintAsset(Out.BlueprintPath, NormalizedPath, LoadError);
    if (!Out.Blueprint) {
      Bridge.SendAutomationError(Socket, RequestId,
          FString::Printf(TEXT("Blueprint not found: %s (%s)"), *Out.BlueprintPath, *LoadError),
          TEXT("BLUEPRINT_NOT_FOUND"));
      return false;
    }
    UClass* GeneratedClass = Out.Blueprint->GeneratedClass;
    Out.RootObject = GeneratedClass ? GeneratedClass->GetDefaultObject() : nullptr;
    if (!Out.RootObject) {
      Bridge.SendAutomationError(Socket, RequestId,
          GeneratedClass ? TEXT("Failed to get Class Default Object") : TEXT("Blueprint has no GeneratedClass (not compiled?)"),
          TEXT("CDO_NOT_FOUND"));
      return false;
    }
    Out.ObjectPath = Out.RootObject->GetPathName();
  } else {
    FString ResolvedPath;
    Out.RootObject = McpHandlerUtils::ResolveObjectFromPath(Out.ObjectPath, &ResolvedPath);
    if (!Out.RootObject) {
      Bridge.SendAutomationError(Socket, RequestId,
          McpHandlerUtils::DescribeObjectNotFound(Out.ObjectPath), TEXT("OBJECT_NOT_FOUND"));
      return false;
    }
    if (!ResolvedPath.IsEmpty()) {
      Out.ObjectPath = ResolvedPath;
    }
    // A caller naming a Blueprint CDO directly (...Default__BP_Foo_C) lands here
    // rather than in the blueprintPath branch. Recover the owning Blueprint so a
    // write is compiled into the class defaults (otherwise every newly spawned
    // instance kept the stale default) and "Component.Property" resolves.
    if (Out.RootObject->HasAnyFlags(RF_ClassDefaultObject)) {
      Out.Blueprint = UBlueprint::GetBlueprintFromClass(Out.RootObject->GetClass());
      // A compile replaces the class and its default object and leaves the old pair in /Engine/Transient as REINST_<Class>,
      // still found by name: the Blueprint's current default object is the one a caller means.
      UClass* Current = Out.Blueprint ? static_cast<UClass*>(Out.Blueprint->GeneratedClass) : nullptr;
      if (Current && Current != Out.RootObject->GetClass()) {
        Out.RootObject = Current->GetDefaultObject();
        Out.ObjectPath = Out.RootObject->GetPathName();
      }
    }
  }

  // Refuse /Script targets before ANY property is read or written. Reads would
  // let a read-only principal export the plugin's own CapabilityToken (the
  // receipt redactor cannot see it under the generic key `value`); writes would
  // let a write-scoped principal set bRequireCapabilityToken=false on the
  // plugin's settings CDO and persist it through PostEditChange().
  if (!McpSafeReflectionTarget::IsAddressable(Out.RootObject)) {
    Bridge.SendAutomationError(Socket, RequestId, McpSafeReflectionTarget::DenyMessage(),
                               McpSafeReflectionTarget::DenyCode());
    return false;
  }
  return true;
}
}
