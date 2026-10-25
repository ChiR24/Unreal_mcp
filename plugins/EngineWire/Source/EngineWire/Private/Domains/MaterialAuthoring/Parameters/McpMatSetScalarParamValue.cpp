#include "Domains/MaterialAuthoring/Parameters/McpMatParamValue.h"

namespace McpMaterialAuthoringHandlers
{
bool HandleSetScalarParameterValue(UEngineWireSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction != TEXT("set_scalar_parameter_value")) {
    return false;
  }
  // A missing value used to write 0 and report success (set_material_parameter does not require one).
  double Value = 0.0;
  if (!Payload->TryGetNumberField(TEXT("value"), Value)) {
    Bridge->SendAutomationError(Socket, RequestId,
        TEXT("'value' must be a number for a scalar parameter; a colour goes to a vector parameter."), TEXT("INVALID_ARGUMENT"));
    return true;
  }
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
