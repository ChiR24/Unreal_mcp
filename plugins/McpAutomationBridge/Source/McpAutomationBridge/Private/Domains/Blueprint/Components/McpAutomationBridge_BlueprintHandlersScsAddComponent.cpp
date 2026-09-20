#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"
#include "Domains/SCS/McpAutomationBridge_SCSHandlers.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

namespace McpBlueprintHandlers {
#if WITH_EDITOR
namespace {
FString ScsFieldOrEmpty(const TSharedPtr<FJsonObject> &Object, const TCHAR *Field) {
  FString Value;
  return (Object.IsValid() && Object->TryGetStringField(Field, Value)) ? Value : FString();
}

FString ScsFirstOf(const TSharedPtr<FJsonObject> &Payload, const TCHAR *Snake, const TCHAR *Camel) {
  const FString Value = ScsFieldOrEmpty(Payload, Snake);
  return Value.IsEmpty() ? ScsFieldOrEmpty(Payload, Camel) : Value;
}
}  // namespace

// `add_component` used to be answered here by a second, thinner implementation
// that created the SCS node and stopped: it never read parentComponent, meshPath
// or materialPath, ran no verification, and still reported success - so
// attaching to a named parent silently produced an unparented component. Only
// the `add_scs_component` spelling reached FSCSHandlers::AddSCSComponent, which
// does all of it. One implementation now serves both spellings.
bool HandleScsAddComponent(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  if (!ActionMatchesPattern(TEXT("add_component")) &&
      !ActionMatchesPattern(TEXT("add_scs_component"))) {
    return false;
  }

  const FString BlueprintPath = ResolveBlueprintRequestedPath();
  FString ComponentClass = ScsFirstOf(Payload, TEXT("component_class"), TEXT("componentClass"));
  if (ComponentClass.IsEmpty()) {
    ComponentClass = ScsFieldOrEmpty(Payload, TEXT("componentType"));
  }
  const FString ComponentName = ScsFirstOf(Payload, TEXT("component_name"), TEXT("componentName"));
  // `attachTo` is the spelling the contract publishes and the one the batch
  // operations[] path reads; this single-add path only ever looked for
  // parentComponent, so an `attachTo: "Mesh"` was dropped on the floor, the
  // parent came out empty, and the node landed on the root -- reported as a
  // success naming CollisionCylinder as the parent. Accept all three.
  FString ParentName = ScsFirstOf(Payload, TEXT("parent_component"), TEXT("parentComponent"));
  if (ParentName.IsEmpty()) {
    ParentName = ScsFieldOrEmpty(Payload, TEXT("attachTo"));
  }
  const FString MeshPath = ScsFirstOf(Payload, TEXT("mesh_path"), TEXT("meshPath"));
  const FString MaterialPath = ScsFirstOf(Payload, TEXT("material_path"), TEXT("materialPath"));

  if (ComponentClass.IsEmpty() || ComponentName.IsEmpty()) {
    Bridge.SendAutomationResponse(
        RequestingSocket, RequestId, false,
        TEXT("add_component requires componentClass and componentName"), nullptr,
        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  TSharedPtr<FJsonObject> Result = FSCSHandlers::AddSCSComponent(
      BlueprintPath, ComponentClass, ComponentName, ParentName, MeshPath, MaterialPath);

  // The contract publishes location/rotation/scale on this action, but the add
  // path never read them: a muzzle placed at the barrel tip in the same call
  // silently landed at the component origin, so bullets spawned inside the
  // grip. Apply them here rather than making every caller follow up with a
  // second set_transform they have no way to know they need.
  const bool bHasTransform = Payload.IsValid() &&
      (Payload->HasField(TEXT("location")) || Payload->HasField(TEXT("rotation")) ||
       Payload->HasField(TEXT("scale")));
  if (bHasTransform && GetJsonBoolField(Result, TEXT("success"))) {
    const TSharedPtr<FJsonObject> Moved =
        FSCSHandlers::SetSCSComponentTransform(BlueprintPath, ComponentName, Payload);
    // AddSCSComponent snapshotted the node before the move, so reporting its
    // verification here would answer a placed component with location 0,0,0.
    const TSharedPtr<FJsonObject> *Fresh = nullptr;
    if (!GetJsonBoolField(Moved, TEXT("success"))) {
      // The node exists but sits where it was created; answering success here
      // would be the silent misplacement this path was written to end.
      const FString Code = ScsFieldOrEmpty(Moved, TEXT("errorCode"));
      Result->SetBoolField(TEXT("success"), false);
      Result->SetStringField(
          TEXT("message"),
          FString::Printf(TEXT("Component '%s' was added but its transform was not applied: %s"),
                          *ComponentName, *ScsFieldOrEmpty(Moved, TEXT("error"))));
      Result->SetStringField(TEXT("error"),
                             Code.IsEmpty() ? TEXT("SCS_TRANSFORM_FAILED") : *Code);
    } else if (Moved.IsValid() &&
               Moved->TryGetObjectField(TEXT("scsVerification"), Fresh) && Fresh) {
      Result->SetObjectField(TEXT("scsVerification"), *Fresh);
    }
  }
  // `properties` is published on this action too, and was dropped exactly the
  // way the transform was: a SpringArm added with TargetArmLength 1100 came out
  // at the class default 300 and the call still reported success. Route each
  // one through the same setter the modify path already uses.
  const TSharedPtr<FJsonObject> *PropertiesObj = nullptr;
  if (Payload.IsValid() &&
      Payload->TryGetObjectField(TEXT("properties"), PropertiesObj) &&
      PropertiesObj != nullptr && GetJsonBoolField(Result, TEXT("success"))) {
    TArray<FString> Rejected;
    // Iterate Values directly: UE 5.8 keys the map by UE::FSharedString and 5.7
    // by FString, but *Pair.Key is const TCHAR* on both.
    for (const auto &Pair : (*PropertiesObj)->Values) {
      const FString PropName(*Pair.Key);
      const TSharedPtr<FJsonObject> Applied = FSCSHandlers::SetSCSComponentProperty(
          BlueprintPath, ComponentName, PropName, Pair.Value);
      if (!GetJsonBoolField(Applied, TEXT("success"))) {
        Rejected.Add(PropName);
      }
    }
    if (Rejected.Num() > 0) {
      // The component exists but is not configured the way the caller asked.
      // Reporting success here is the silent default-value bug this path was
      // written to end.
      Result->SetBoolField(TEXT("success"), false);
      Result->SetStringField(
          TEXT("message"),
          FString::Printf(TEXT("Component '%s' was added but these properties did not apply: %s"),
                          *ComponentName, *FString::Join(Rejected, TEXT(", "))));
      Result->SetStringField(TEXT("error"), TEXT("SCS_PROPERTY_FAILED"));
    }
  }
  Bridge.SendAutomationResponse(RequestingSocket, RequestId,
                                GetJsonBoolField(Result, TEXT("success")),
                                ScsFieldOrEmpty(Result, TEXT("message")), Result,
                                ScsFieldOrEmpty(Result, TEXT("error")));
  return true;
}
#endif
}
