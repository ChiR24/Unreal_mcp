#include "McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
bool HandleAddScalarParameter(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("add_scalar_parameter")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    FString ParamName, Group;
    double DefaultValue = 0.0;
    if (!Payload->TryGetStringField(TEXT("parameterName"), ParamName) ||
        ParamName.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'parameterName'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    Payload->TryGetNumberField(TEXT("defaultValue"), DefaultValue);
    Payload->TryGetStringField(TEXT("group"), Group);

    UMaterialExpressionScalarParameter *ScalarParam =
        NewObject<UMaterialExpressionScalarParameter>(
            HostOuter, UMaterialExpressionScalarParameter::StaticClass(),
            NAME_None, RF_Transactional);
    ScalarParam->ParameterName = FName(*ParamName);
    ScalarParam->DefaultValue = DefaultValue;
    if (!Group.IsEmpty()) {
      ScalarParam->Group = FName(*Group);
    }
    ScalarParam->MaterialExpressionEditorX = (int32)X;
    ScalarParam->MaterialExpressionEditorY = (int32)Y;

    AddExpressionToContainer(Material, Function, ScalarParam);
    FINALIZE_HOST();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"),
                           MCP_NODE_ID(ScalarParam));
    Bridge->SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Scalar parameter '%s' added."), *ParamName),
        Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // add_vector_parameter
  // --------------------------------------------------------------------------
  return false;
}
}
#endif
