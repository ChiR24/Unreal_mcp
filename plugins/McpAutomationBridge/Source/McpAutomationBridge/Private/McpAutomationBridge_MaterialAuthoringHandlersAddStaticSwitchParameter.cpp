#include "McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
bool HandleAddStaticSwitchParameter(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("add_static_switch_parameter")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    FString ParamName, Group;
    bool DefaultValue = false;
    if (!Payload->TryGetStringField(TEXT("parameterName"), ParamName) ||
        ParamName.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'parameterName'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    Payload->TryGetBoolField(TEXT("defaultValue"), DefaultValue);
    Payload->TryGetStringField(TEXT("group"), Group);

    UMaterialExpressionStaticSwitchParameter *SwitchParam =
        NewObject<UMaterialExpressionStaticSwitchParameter>(
            HostOuter, UMaterialExpressionStaticSwitchParameter::StaticClass(),
            NAME_None, RF_Transactional);
    SwitchParam->ParameterName = FName(*ParamName);
    SwitchParam->DefaultValue = DefaultValue;
    if (!Group.IsEmpty()) {
      SwitchParam->Group = FName(*Group);
    }
    SwitchParam->MaterialExpressionEditorX = (int32)X;
    SwitchParam->MaterialExpressionEditorY = (int32)Y;

    AddExpressionToContainer(Material, Function, SwitchParam);
    FINALIZE_HOST();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"),
                           MCP_NODE_ID(SwitchParam));
    Bridge->SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Static switch '%s' added."), *ParamName), Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // add_math_node
  // --------------------------------------------------------------------------
  return false;
}
}
#endif
