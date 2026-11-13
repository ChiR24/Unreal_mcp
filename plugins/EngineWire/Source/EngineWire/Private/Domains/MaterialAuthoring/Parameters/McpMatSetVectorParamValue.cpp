#include "Domains/MaterialAuthoring/Parameters/McpMatParamValue.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonNumbers.h"

namespace McpMaterialAuthoringHandlers
{
bool HandleSetVectorParameterValue(UEngineWireSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction != TEXT("set_vector_parameter_value")) {
    return false;
  }
  // `value` as add_vector_parameter takes it: {r,g,b,a} / {x,y,z,w} or [r,g,b(,a)]. Anything else (missing, an entry
  // that is not a number, a fifth entry) is refused rather than written as white.
  if (!McpIsJsonVectorValue(Payload->TryGetField(TEXT("value")), true)) {
    Bridge->SendAutomationError(Socket, RequestId,
        TEXT("'value' must be an object {r,g,b,a} / {x,y,z,w} or an array [r,g,b(,a)]."), TEXT("INVALID_ARGUMENT"));
    return true;
  }
  const FLinearColor Color = ExtractLinearColorField(Payload, TEXT("value"), FLinearColor::White);

  FParameterValueWriter Writer;
  Writer.Kind = TEXT("Vector");
  Writer.Type = EMaterialParameterType::Vector;
  Writer.SetDefault = [Color](UMaterial* Material, FName Name, TArray<FString>& Available) {
    return SetParameterExpressions<UMaterialExpressionVectorParameter>(Material, Name, Available,
        [Color](UMaterialExpressionVectorParameter& Param) { Param.DefaultValue = Color; });
  };
  Writer.SetOverride = [Color](UMaterialInstanceConstant* Instance, FName Name) {
    Instance->SetVectorParameterValueEditorOnly(Name, Color);
  };
  // Read back what landed, not what was asked for.
  Writer.DescribeValue = [Color](UMaterialInterface* Material, FName Name, const TSharedPtr<FJsonObject>& Result) {
    FLinearColor Stored = Color;
    if (UMaterialInstanceConstant* Instance = Cast<UMaterialInstanceConstant>(Material)) {
      for (const FVectorParameterValue& Entry : Instance->VectorParameterValues) {
        if (Entry.ParameterInfo.Name == Name) { Stored = Entry.ParameterValue; }
      }
    }
    Result->SetObjectField(TEXT("value"), McpHandlerUtils::LinearColorToJson(Stored));
  };
  SetMaterialParameterValue(Bridge, RequestId, Payload, Socket, Writer);
  return true;
}
}
