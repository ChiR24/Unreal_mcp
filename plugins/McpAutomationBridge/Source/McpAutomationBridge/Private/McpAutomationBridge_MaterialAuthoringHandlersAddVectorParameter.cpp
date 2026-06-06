#include "McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
bool HandleAddVectorParameter(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("add_vector_parameter")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    FString ParamName, Group;
    if (!Payload->TryGetStringField(TEXT("parameterName"), ParamName) ||
        ParamName.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'parameterName'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    Payload->TryGetStringField(TEXT("group"), Group);

    UMaterialExpressionVectorParameter *VecParam =
        NewObject<UMaterialExpressionVectorParameter>(
            HostOuter, UMaterialExpressionVectorParameter::StaticClass(),
            NAME_None, RF_Transactional);
    VecParam->ParameterName = FName(*ParamName);
    if (!Group.IsEmpty()) {
      VecParam->Group = FName(*Group);
    }

    // Parse default value
    const TSharedPtr<FJsonObject> *DefaultObj;
    if (Payload->TryGetObjectField(TEXT("defaultValue"), DefaultObj)) {
      double R = 1.0, G = 1.0, B = 1.0, A = 1.0;
      (*DefaultObj)->TryGetNumberField(TEXT("r"), R);
      (*DefaultObj)->TryGetNumberField(TEXT("g"), G);
      (*DefaultObj)->TryGetNumberField(TEXT("b"), B);
      (*DefaultObj)->TryGetNumberField(TEXT("a"), A);
      VecParam->DefaultValue = FLinearColor(R, G, B, A);
    }

    VecParam->MaterialExpressionEditorX = (int32)X;
    VecParam->MaterialExpressionEditorY = (int32)Y;

    AddExpressionToContainer(Material, Function, VecParam);
    FINALIZE_HOST();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"),
                           MCP_NODE_ID(VecParam));
    Bridge->SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Vector parameter '%s' added."), *ParamName),
        Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // add_static_switch_parameter
  // --------------------------------------------------------------------------
  return false;
}
}
#endif
