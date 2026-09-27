#include "Domains/Property/McpAutomationBridge_PropertyHandlersTarget.h"

#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Safety/McpSafeReflectionTarget.h"

#include "Engine/Blueprint.h"

namespace McpPropertyTarget
{
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
    Bridge.SendAutomationError(Socket, RequestId, TEXT("Either objectPath or blueprintPath is required."),
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
          FString::Printf(TEXT("Unable to find object at path %s."), *Out.ObjectPath), TEXT("OBJECT_NOT_FOUND"));
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
