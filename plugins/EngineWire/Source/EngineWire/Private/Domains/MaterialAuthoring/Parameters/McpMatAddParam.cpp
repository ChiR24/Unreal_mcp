#include "Domains/MaterialAuthoring/McpMatPrivate.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonNumbers.h"

namespace McpMaterialAuthoringHandlers
{
namespace
{
// add_scalar_parameter / add_vector_parameter / add_static_switch_parameter: the same
// named, grouped, placed parameter node; SetDefault fills the class-specific default.
template <typename TParam, typename TSetDefault>
bool AddParameterExpression(UEngineWireSubsystem* Bridge, const FString& RequestId,
                            const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket,
                            const TCHAR* Label, TSetDefault&& SetDefault)
{
  LOAD_EDITABLE_MATERIAL_OR_FUNCTION_OR_RETURN();

  // The add_material_node family documents `name` as "Parameter name, when the node type is a
  // parameter", so a build_material_graph step {edit: add_scalar_parameter, name: ...} failed with
  // "Missing 'parameterName'" although it followed the contract. Either spelling names it.
  FString Group;
  const FString ParamName = McpGetFirstStringField(Payload, {TEXT("parameterName"), TEXT("name")});
  if (ParamName.IsEmpty()) {
    Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'parameterName' (or 'name')."), TEXT("INVALID_ARGUMENT"));
    return true;
  }
  Payload->TryGetStringField(TEXT("group"), Group);

  TParam *Param = NewObject<TParam>(HostOuter, TParam::StaticClass(), NAME_None, RF_Transactional);
  Param->ParameterName = FName(*ParamName);
  SetDefault(Param);
  if (!Group.IsEmpty()) {
    Param->Group = FName(*Group);
  }
  FString PlacementWarning;
  TSharedPtr<FJsonObject> Result = PlaceMaterialExpression(RequestId, Material, Function, Param, X, Y, &PlacementWarning);
  Bridge->SendAutomationResponse(Socket, RequestId, true,
      PlacementWarning.IsEmpty()
          ? FString::Printf(TEXT("%s '%s' added."), Label, *ParamName)
          : FString::Printf(TEXT("%s '%s' added. %s"), Label, *ParamName, *PlacementWarning),
      Result);
  return true;
}

// defaultValue, when sent, as a JSON Type: an array or an object read as 0 or false behind a success where a batch step
// reaches the adder past the schema the gateway holds a direct call to.
bool DefaultValueIsOrReply(UEngineWireSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                           TSharedPtr<FMcpBridgeWebSocket> Socket, EJson Type, const TCHAR* Kind)
{
  const TSharedPtr<FJsonValue> Value = Payload->TryGetField(TEXT("defaultValue"));
  if (!Value.IsValid() || Value->Type == Type) {
    return true;
  }
  Bridge->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("defaultValue takes %s; no node was added."), Kind),
                              TEXT("INVALID_ARGUMENT"));
  return false;
}
}

bool HandleAddParameterNode(UEngineWireSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("add_scalar_parameter")) {
    if (!DefaultValueIsOrReply(Bridge, RequestId, Payload, Socket, EJson::Number, TEXT("a number"))) {
      return true;
    }
    return AddParameterExpression<UMaterialExpressionScalarParameter>(Bridge, RequestId, Payload, Socket,
        TEXT("Scalar parameter"), [&Payload](UMaterialExpressionScalarParameter* Param) {
          Param->DefaultValue = GetJsonNumberField(Payload, TEXT("defaultValue"), 0.0);
        });
  }
  if (SubAction == TEXT("add_vector_parameter")) {
    // [1, 0.5] made it white and ["a", "b", "c"] black, each answered as added: nothing is added for those.
    if (Payload->HasField(TEXT("defaultValue")) && !McpIsJsonVectorValue(Payload->TryGetField(TEXT("defaultValue")), true)) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("defaultValue for a vector parameter takes [r, g, b], [r, g, b, a] "
          "or {r, g, b, a}; any other value is refused and no node was added."), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    return AddParameterExpression<UMaterialExpressionVectorParameter>(Bridge, RequestId, Payload, Socket,
        TEXT("Vector parameter"), [&Payload](UMaterialExpressionVectorParameter* Param) {
          // r/g/b/a or x/y/z/w object, or [r,g,b(,a)]; white when absent.
          Param->DefaultValue = ExtractLinearColorField(Payload, TEXT("defaultValue"), FLinearColor::White);
        });
  }
  if (SubAction == TEXT("add_static_switch_parameter")) {
    if (!DefaultValueIsOrReply(Bridge, RequestId, Payload, Socket, EJson::Boolean, TEXT("true or false"))) {
      return true;
    }
    return AddParameterExpression<UMaterialExpressionStaticSwitchParameter>(Bridge, RequestId, Payload, Socket,
        TEXT("Static switch"), [&Payload](UMaterialExpressionStaticSwitchParameter* Param) {
          Param->DefaultValue = GetJsonBoolField(Payload, TEXT("defaultValue"), false);
        });
  }
  return false;
}
}
