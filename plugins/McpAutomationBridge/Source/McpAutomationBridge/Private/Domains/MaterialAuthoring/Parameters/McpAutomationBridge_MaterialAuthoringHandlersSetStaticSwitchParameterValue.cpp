#include "Domains/MaterialAuthoring/Parameters/McpAutomationBridge_MaterialAuthoringParameterValue.h"

namespace McpMaterialAuthoringHandlers
{
bool HandleSetStaticSwitchParameterValue(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction != TEXT("set_static_switch_parameter_value")) {
    return false;
  }
  bool Value = false;
  Payload->TryGetBoolField(TEXT("value"), Value);
  FParameterValueWriter Writer;
  Writer.Kind = TEXT("Static switch");
  Writer.Type = EMaterialParameterType::StaticSwitch;
  Writer.SetDefault = [Value](UMaterial* Material, FName Name, TArray<FString>& Available) {
    UMaterialExpressionStaticBoolParameter* Param =
        FindParameterExpression<UMaterialExpressionStaticBoolParameter>(Material, Name, Available);
    if (Param) { Param->DefaultValue = Value; }
    return Param != nullptr;
  };
  // A switch lives in the static parameter set, which has to be rebuilt.
  Writer.SetOverride = [Value](UMaterialInstanceConstant* Instance, FName Name) {
    FStaticParameterSet StaticParams;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    Instance->GetStaticParameterValues(StaticParams);
#else
    StaticParams = Instance->GetStaticParameters();
#endif
    FStaticSwitchParameter* Switch = StaticParams.StaticSwitchParameters.FindByPredicate(
        [Name](const FStaticSwitchParameter& Entry) { return Entry.ParameterInfo.Name == Name; });
    if (!Switch) {
      Switch = &StaticParams.StaticSwitchParameters.AddDefaulted_GetRef();
      Switch->ParameterInfo.Name = Name;
    }
    Switch->Value = Value;
    Switch->bOverride = true;
    Instance->UpdateStaticPermutation(StaticParams);
  };
  Writer.DescribeValue = [Value](UMaterialInterface*, FName, const TSharedPtr<FJsonObject>& Result) {
    Result->SetBoolField(TEXT("value"), Value);
  };
  SetMaterialParameterValue(Bridge, RequestId, Payload, Socket, Writer);
  return true;
}
}
