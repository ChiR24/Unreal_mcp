#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"
#include "Domains/SCS/McpAutomationBridge_SCSHandlers.h"

namespace McpBlueprintHandlers {
bool HandleBlueprintScsWrappers(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  auto SafeGetStr = [](const TSharedPtr<FJsonObject> &Object,
                       const FString &FieldName) {
    FString Value;
    if (Object.IsValid() && Object->TryGetStringField(FieldName, Value)) {
      return Value;
    }
    return FString();
  };

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
    FString BPPath;
    if (BPPath.IsEmpty()) {
      Payload->TryGetStringField(TEXT("blueprintPath"), BPPath);
    }
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
          SafeGetStr(Batch, bAll ? TEXT("message") : TEXT("error")), Batch,
          bAll ? FString() : SafeGetStr(Batch, TEXT("errorCode")));
      return true;
    }
    FString CompName;
    if (CompName.IsEmpty()) {
      Payload->TryGetStringField(TEXT("componentName"), CompName);
    }
    TSharedPtr<FJsonObject> Result =
        FSCSHandlers::RemoveSCSComponent(BPPath, CompName);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId,
                           GetJsonBoolField(Result, TEXT("success")),
                           SafeGetStr(Result, TEXT("message")), Result,
                           SafeGetStr(Result, TEXT("error")));
    return true;
  }

  if (ActionMatchesPattern(TEXT("reparent_scs_component"))) {
    FString BPPath;
    if (BPPath.IsEmpty()) {
      Payload->TryGetStringField(TEXT("blueprintPath"), BPPath);
    }
    FString CompName;
    if (CompName.IsEmpty()) {
      Payload->TryGetStringField(TEXT("componentName"), CompName);
    }
    FString NewParent;
    if (NewParent.IsEmpty()) {
      Payload->TryGetStringField(TEXT("newParent"), NewParent);
    }
    TSharedPtr<FJsonObject> Result =
        FSCSHandlers::ReparentSCSComponent(BPPath, CompName, NewParent);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId,
                           GetJsonBoolField(Result, TEXT("success")),
                           SafeGetStr(Result, TEXT("message")), Result,
                           SafeGetStr(Result, TEXT("error")));
    return true;
  }

  if (ActionMatchesPattern(TEXT("set_scs_transform"))) {
    FString BPPath;
    if (BPPath.IsEmpty()) {
      Payload->TryGetStringField(TEXT("blueprintPath"), BPPath);
    }
    FString CompName;
    if (CompName.IsEmpty()) {
      Payload->TryGetStringField(TEXT("componentName"), CompName);
    }
    TSharedPtr<FJsonObject> Result =
        FSCSHandlers::SetSCSComponentTransform(BPPath, CompName, Payload);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId,
                           GetJsonBoolField(Result, TEXT("success")),
                           SafeGetStr(Result, TEXT("message")), Result,
                           SafeGetStr(Result, TEXT("error")));
    return true;
  }

  if (ActionMatchesPattern(TEXT("set_scs_property"))) {
    FString BPPath;
    if (BPPath.IsEmpty()) {
      Payload->TryGetStringField(TEXT("blueprintPath"), BPPath);
    }
    FString CompName;
    if (CompName.IsEmpty()) {
      Payload->TryGetStringField(TEXT("componentName"), CompName);
    }
    FString PropName;
    if (PropName.IsEmpty()) {
      Payload->TryGetStringField(TEXT("propertyName"), PropName);
    }
    // The published capability schema declares `propertyValue` and, with
    // additionalProperties:false, rejects both spellings this handler used to read. That
    // left set_scs_property uncallable through the gateway by ANY input: the contract
    // spelling never reached the handler, and the handler spellings never passed
    // validation. Prefer the contract, keep the legacy WebSocket spellings as fallbacks.
    TSharedPtr<FJsonValue> ResolvedPropVal =
        Payload->TryGetField(TEXT("propertyValue"));
    if (!ResolvedPropVal.IsValid()) {
      ResolvedPropVal = Payload->TryGetField(TEXT("property_value"));
    }
    if (!ResolvedPropVal.IsValid()) {
      ResolvedPropVal = Payload->TryGetField(TEXT("value"));
    }
    TSharedPtr<FJsonObject> Result = FSCSHandlers::SetSCSComponentProperty(
        BPPath, CompName, PropName, ResolvedPropVal);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId,
                           GetJsonBoolField(Result, TEXT("success")),
                           SafeGetStr(Result, TEXT("message")), Result,
                           SafeGetStr(Result, TEXT("error")));
    return true;
  }

  return false;
}
} // namespace McpBlueprintHandlers
