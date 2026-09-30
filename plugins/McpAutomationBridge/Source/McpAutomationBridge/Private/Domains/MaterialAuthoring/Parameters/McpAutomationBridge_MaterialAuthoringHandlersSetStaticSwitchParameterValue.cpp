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
    // UE 5.1 keeps the switch list in FStaticParameterSet::EditorOnly; 5.0 and 5.2+ hold it directly.
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 1
    TArray<FStaticSwitchParameter>& Switches = StaticParams.EditorOnly.StaticSwitchParameters;
#else
    TArray<FStaticSwitchParameter>& Switches = StaticParams.StaticSwitchParameters;
#endif
    FStaticSwitchParameter* Switch = Switches.FindByPredicate(
        [Name](const FStaticSwitchParameter& Entry) { return Entry.ParameterInfo.Name == Name; });
    if (!Switch) {
      Switch = &Switches.AddDefaulted_GetRef();
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
