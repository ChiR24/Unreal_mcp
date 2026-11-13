#include "Domains/Blueprint/McpBPActionContext.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "Domains/SCS/McpSCSHandlers.h"
#include "Domains/ControlActor/Placement/McpPartPlacement.h"

namespace McpBlueprintHandlers {
void SendScsResult(const FBlueprintActionContext &Context, const TSharedPtr<FJsonObject> &Result) {
  const bool bSuccess = GetJsonBoolField(Result, TEXT("success"));
  const FString Error = GetJsonStringField(Result, TEXT("error"));
  Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, bSuccess,
                                        !bSuccess && !Error.IsEmpty() ? Error : GetJsonStringField(Result, TEXT("message")),
                                        Result, bSuccess ? FString() : GetJsonStringField(Result, TEXT("errorCode")));
}

bool HandleBlueprintScsWrappers(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  TGuardValue<bool> SaveScope(FSCSHandlers::bSaveScsChanges, GetJsonBoolField(Payload, TEXT("save"), true));
  const FString BPPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
  const FString CompName = GetJsonStringField(Payload, TEXT("componentName"));

  // This used to be a THIRD copy of the same six TryGetStringField pairs, and
  // because it sits earlier in the route table than HandleScsAddComponent it is
  // the copy that actually answered every add_scs_component call -- so the
  // `attachTo` alias added to the other copy never ran, and a component asked
  // for `attachTo: "Mesh"` still landed on the collision cylinder while the
  // reply claimed success. Delegate instead of duplicating: ActionMatchesPattern
  // strips separators on both sides, so whenever this matched
  // "add_scs_component" the handler below matches it too.
  if (ActionMatchesPattern(TEXT("add_scs_component"))) {
    if (HandleScsAddComponent(Context)) {
      return true;
    }
  }

  if (ActionMatchesPattern(TEXT("remove_scs_component"))) {
    // componentNames: several removals under one consent, one compile and one save.
    const TArray<TSharedPtr<FJsonValue>> *Names = nullptr;
    if (Payload->TryGetArrayField(TEXT("componentNames"), Names) && Names->Num() > 0) {
      TArray<FString> NameList;
      for (const TSharedPtr<FJsonValue> &Name : *Names) {
        NameList.Add(Name.IsValid() ? Name->AsString() : FString());
      }
      TSharedPtr<FJsonObject> Batch = FSCSHandlers::RemoveSCSComponents(BPPath, NameList);
      const bool bAll = GetJsonBoolField(Batch, TEXT("success"));
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, bAll,
          GetJsonStringField(Batch, bAll ? TEXT("message") : TEXT("error")), Batch,
          bAll ? FString() : GetJsonStringField(Batch, TEXT("errorCode")));
      return true;
    }
    SendScsResult(Context, FSCSHandlers::RemoveSCSComponent(BPPath, CompName));
    return true;
  }

  // A part moved inside another part, or below the actor's capsule, is named in the reply of the three edits below.
  TSharedPtr<FJsonObject> Result;
  if (ActionMatchesPattern(TEXT("reparent_scs_component"))) {
    Result = FSCSHandlers::ReparentSCSComponent(BPPath, CompName, GetJsonStringField(Payload, TEXT("newParent")));
  } else if (ActionMatchesPattern(TEXT("set_scs_transform"))) {
    Result = FSCSHandlers::SetSCSComponentTransform(BPPath, CompName, Payload);
  } else if (ActionMatchesPattern(TEXT("set_scs_property"))) {
    Result = FSCSHandlers::SetSCSComponentProperty(BPPath, CompName, GetJsonStringField(Payload, TEXT("propertyName")),
                                                   Payload->TryGetField(TEXT("propertyValue")));
  } else {
    return false;
  }
  if (GetJsonBoolField(Result, TEXT("success"))) {
    McpPartPlacement::AppendPartWarnings(BPPath, {CompName}, Result);
  }
  SendScsResult(Context, Result);
  return true;
}
} // namespace McpBlueprintHandlers
