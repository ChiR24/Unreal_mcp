#include "Domains/MaterialAuthoring/Parameters/McpAutomationBridge_MaterialAuthoringParameterValue.h"

namespace McpMaterialAuthoringHandlers
{
bool HandleSetScalarParameterValue(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction != TEXT("set_scalar_parameter_value")) {
    return false;
  }
  double Value = 0.0;
  Payload->TryGetNumberField(TEXT("value"), Value);
  FParameterValueWriter Writer;
  Writer.Kind = TEXT("Scalar");
  Writer.Type = EMaterialParameterType::Scalar;
  Writer.SetDefault = [Value](UMaterial* Material, FName Name, TArray<FString>& Available) {
    UMaterialExpressionScalarParameter* Param =
        FindParameterExpression<UMaterialExpressionScalarParameter>(Material, Name, Available);
    if (Param) { Param->DefaultValue = Value; }
    return Param != nullptr;
  };
  Writer.SetOverride = [Value](UMaterialInstanceConstant* Instance, FName Name) {
    Instance->SetScalarParameterValueEditorOnly(Name, Value);
  };
  Writer.DescribeValue = [Value](UMaterialInterface*, FName, const TSharedPtr<FJsonObject>& Result) {
    Result->SetNumberField(TEXT("value"), Value);
  };
  SetMaterialParameterValue(Bridge, RequestId, Payload, Socket, Writer);
  return true;
}
}
