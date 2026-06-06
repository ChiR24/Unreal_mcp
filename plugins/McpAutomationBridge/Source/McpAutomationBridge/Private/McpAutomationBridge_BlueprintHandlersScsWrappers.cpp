#include "McpAutomationBridge_BlueprintActionContext.h"
#include "McpAutomationBridgeHelpersJsonFields.h"
#include "McpAutomationBridge_SCSHandlers.h"

namespace McpBlueprintHandlers {
#if WITH_EDITOR
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

  if (ActionMatchesPattern(TEXT("get_blueprint_scs")) ||
      AlphaNumLower.Contains(TEXT("getblueprintscs"))) {
    FString BPPath;
    Payload->TryGetStringField(TEXT("blueprint_path"), BPPath);
    TSharedPtr<FJsonObject> Result = FSCSHandlers::GetBlueprintSCS(BPPath);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId,
                           GetJsonBoolField(Result, TEXT("success")),
                           SafeGetStr(Result, TEXT("message")), Result,
                           SafeGetStr(Result, TEXT("error")));
    return true;
  }

  // add_scs_component: add component to SCS
  if (ActionMatchesPattern(TEXT("add_scs_component")) ||
      AlphaNumLower.Contains(TEXT("addscscomponent"))) {
    FString BPPath;
    Payload->TryGetStringField(TEXT("blueprint_path"), BPPath);
    if (BPPath.IsEmpty()) {
      Payload->TryGetStringField(TEXT("blueprintPath"), BPPath);
    }
    FString CompClass;
    Payload->TryGetStringField(TEXT("component_class"), CompClass);
    if (CompClass.IsEmpty()) {
      Payload->TryGetStringField(TEXT("componentClass"), CompClass);
    }
    FString CompName;
    Payload->TryGetStringField(TEXT("component_name"), CompName);
    if (CompName.IsEmpty()) {
      Payload->TryGetStringField(TEXT("componentName"), CompName);
    }
    FString ParentName;
    Payload->TryGetStringField(TEXT("parent_component"), ParentName);
    if (ParentName.IsEmpty()) {
      Payload->TryGetStringField(TEXT("parentComponent"), ParentName);
    }
    // Feature #1, #2: Extract mesh and material paths for assignment
    FString MeshPath;
    Payload->TryGetStringField(TEXT("mesh_path"), MeshPath);
    if (MeshPath.IsEmpty()) {
      Payload->TryGetStringField(TEXT("meshPath"), MeshPath);
    }
    FString MaterialPath;
    Payload->TryGetStringField(TEXT("material_path"), MaterialPath);
    if (MaterialPath.IsEmpty()) {
      Payload->TryGetStringField(TEXT("materialPath"), MaterialPath);
    }
    TSharedPtr<FJsonObject> Result = FSCSHandlers::AddSCSComponent(
        BPPath, CompClass, CompName, ParentName, MeshPath, MaterialPath);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId,
                           GetJsonBoolField(Result, TEXT("success")),
                           SafeGetStr(Result, TEXT("message")), Result,
                           SafeGetStr(Result, TEXT("error")));
    return true;
  }

  // remove_scs_component: remove component from SCS
  if (ActionMatchesPattern(TEXT("remove_scs_component")) ||
      AlphaNumLower.Contains(TEXT("removescscomponent"))) {
    FString BPPath;
    Payload->TryGetStringField(TEXT("blueprint_path"), BPPath);
    if (BPPath.IsEmpty()) {
      Payload->TryGetStringField(TEXT("blueprintPath"), BPPath);
    }
    FString CompName;
    Payload->TryGetStringField(TEXT("component_name"), CompName);
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

  // reparent_scs_component: reparent component in SCS
  if (ActionMatchesPattern(TEXT("reparent_scs_component")) ||
      AlphaNumLower.Contains(TEXT("reparentscscomponent"))) {
    FString BPPath;
    Payload->TryGetStringField(TEXT("blueprint_path"), BPPath);
    if (BPPath.IsEmpty()) {
      Payload->TryGetStringField(TEXT("blueprintPath"), BPPath);
    }
    FString CompName;
    Payload->TryGetStringField(TEXT("component_name"), CompName);
    if (CompName.IsEmpty()) {
      Payload->TryGetStringField(TEXT("componentName"), CompName);
    }
    FString NewParent;
    Payload->TryGetStringField(TEXT("new_parent"), NewParent);
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

  // set_scs_component_transform: set component transform in SCS
  if (ActionMatchesPattern(TEXT("set_scs_component_transform")) ||
      ActionMatchesPattern(TEXT("set_scs_transform")) ||
      AlphaNumLower.Contains(TEXT("setscscomponenttransform")) ||
      AlphaNumLower.Contains(TEXT("setscstransform"))) {
    FString BPPath;
    Payload->TryGetStringField(TEXT("blueprint_path"), BPPath);
    if (BPPath.IsEmpty()) {
      Payload->TryGetStringField(TEXT("blueprintPath"), BPPath);
    }
    FString CompName;
    Payload->TryGetStringField(TEXT("component_name"), CompName);
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

  // set_scs_component_property: set component property in SCS
  if (ActionMatchesPattern(TEXT("set_scs_component_property")) ||
      ActionMatchesPattern(TEXT("set_scs_property")) ||
      AlphaNumLower.Contains(TEXT("setscscomponentproperty")) ||
      AlphaNumLower.Contains(TEXT("setscsproperty"))) {
    FString BPPath;
    Payload->TryGetStringField(TEXT("blueprint_path"), BPPath);
    if (BPPath.IsEmpty()) {
      Payload->TryGetStringField(TEXT("blueprintPath"), BPPath);
    }
    FString CompName;
    Payload->TryGetStringField(TEXT("component_name"), CompName);
    if (CompName.IsEmpty()) {
      Payload->TryGetStringField(TEXT("componentName"), CompName);
    }
    FString PropName;
    Payload->TryGetStringField(TEXT("property_name"), PropName);
    if (PropName.IsEmpty()) {
      Payload->TryGetStringField(TEXT("propertyName"), PropName);
    }
    const TSharedPtr<FJsonValue> PropVal =
        Payload->TryGetField(TEXT("property_value"));
    const TSharedPtr<FJsonValue> ResolvedPropVal =
        PropVal.IsValid() ? PropVal : Payload->TryGetField(TEXT("value"));
    TSharedPtr<FJsonObject> Result = FSCSHandlers::SetSCSComponentProperty(
        BPPath, CompName, PropName, ResolvedPropVal);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId,
                           GetJsonBoolField(Result, TEXT("success")),
                           SafeGetStr(Result, TEXT("message")), Result,
                           SafeGetStr(Result, TEXT("error")));
    return true;
  }

  // blueprint_set_variable_metadata: apply metadata to the Blueprint variable
  // (editor-only when available)
  return false;
}
#endif
} // namespace McpBlueprintHandlers
