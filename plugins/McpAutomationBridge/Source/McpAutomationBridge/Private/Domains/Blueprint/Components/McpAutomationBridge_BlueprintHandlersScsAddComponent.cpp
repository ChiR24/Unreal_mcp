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
  const FString ParentName = ScsFirstOf(Payload, TEXT("parent_component"), TEXT("parentComponent"));
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
  Bridge.SendAutomationResponse(RequestingSocket, RequestId,
                                GetJsonBoolField(Result, TEXT("success")),
                                ScsFieldOrEmpty(Result, TEXT("message")), Result,
                                ScsFieldOrEmpty(Result, TEXT("error")));
  return true;
}
#endif
}
