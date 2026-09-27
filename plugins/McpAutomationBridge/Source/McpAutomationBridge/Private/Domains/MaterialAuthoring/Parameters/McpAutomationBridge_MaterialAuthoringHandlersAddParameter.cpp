#include "Domains/MaterialAuthoring/McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

namespace McpMaterialAuthoringHandlers
{
namespace
{
// add_scalar_parameter / add_vector_parameter / add_static_switch_parameter: the same
// named, grouped, placed parameter node; SetDefault fills the class-specific default.
template <typename TParam, typename TSetDefault>
bool AddParameterExpression(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId,
                            const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket,
                            const TCHAR* Label, TSetDefault&& SetDefault)
{
  LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

  FString ParamName, Group;
  if (!Payload->TryGetStringField(TEXT("parameterName"), ParamName) || ParamName.IsEmpty()) {
    Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'parameterName'."), TEXT("INVALID_ARGUMENT"));
    return true;
  }
  Payload->TryGetStringField(TEXT("group"), Group);

  TParam *Param = NewObject<TParam>(HostOuter, TParam::StaticClass(), NAME_None, RF_Transactional);
  Param->ParameterName = FName(*ParamName);
  SetDefault(Param);
  if (!Group.IsEmpty()) {
    Param->Group = FName(*Group);
  }
  Param->MaterialExpressionEditorX = (int32)X;
  Param->MaterialExpressionEditorY = (int32)Y;

  AddExpressionToContainer(Material, Function, Param);
  FINALIZE_HOST();

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("nodeId"), MCP_NODE_ID(Param));
  const FString PlacementWarning = AddMaterialNodePlacementFields(Result, Material, Param);
  Bridge->SendAutomationResponse(Socket, RequestId, true,
      PlacementWarning.IsEmpty()
          ? FString::Printf(TEXT("%s '%s' added."), Label, *ParamName)
          : FString::Printf(TEXT("%s '%s' added. %s"), Label, *ParamName, *PlacementWarning),
      Result);
  return true;
}
}

bool HandleAddParameterNode(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("add_scalar_parameter")) {
    return AddParameterExpression<UMaterialExpressionScalarParameter>(Bridge, RequestId, Payload, Socket,
        TEXT("Scalar parameter"), [&Payload](UMaterialExpressionScalarParameter* Param) {
          Param->DefaultValue = GetJsonNumberField(Payload, TEXT("defaultValue"), 0.0);
        });
  }
  if (SubAction == TEXT("add_vector_parameter")) {
    return AddParameterExpression<UMaterialExpressionVectorParameter>(Bridge, RequestId, Payload, Socket,
        TEXT("Vector parameter"), [&Payload](UMaterialExpressionVectorParameter* Param) {
          // r/g/b/a or x/y/z/w object, or [r,g,b(,a)]; white when absent.
          Param->DefaultValue = ExtractLinearColorField(Payload, TEXT("defaultValue"), FLinearColor::White);
        });
  }
  if (SubAction == TEXT("add_static_switch_parameter")) {
    return AddParameterExpression<UMaterialExpressionStaticSwitchParameter>(Bridge, RequestId, Payload, Socket,
        TEXT("Static switch"), [&Payload](UMaterialExpressionStaticSwitchParameter* Param) {
          Param->DefaultValue = GetJsonBoolField(Payload, TEXT("defaultValue"), false);
        });
  }
  return false;
}
}
