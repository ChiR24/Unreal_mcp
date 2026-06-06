#include "McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
bool HandleSetStaticSwitchParameterValue(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("set_static_switch_parameter_value")) {
    FString AssetPath, ParamName;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("parameterName"), ParamName) || ParamName.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'parameterName'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    bool Value = false;
    Payload->TryGetBoolField(TEXT("value"), Value);

    FString ValidatedPath = SanitizeProjectRelativePath(AssetPath);
    if (ValidatedPath.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid path '%s'"), *AssetPath), TEXT("INVALID_PATH"));
      return true;
    }
    AssetPath = ValidatedPath;

    UMaterialInstanceConstant *Instance = LoadObject<UMaterialInstanceConstant>(nullptr, *AssetPath);
    if (!Instance) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Could not load material instance."), TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    // Set the static switch parameter
    FStaticParameterSet StaticParams;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    Instance->GetStaticParameterValues(StaticParams);
#else
    StaticParams = Instance->GetStaticParameters();
#endif
    bool bFound = false;
    for (auto &Switch : StaticParams.StaticSwitchParameters) {
      if (Switch.ParameterInfo.Name == FName(*ParamName)) {
        Switch.Value = Value;
        Switch.bOverride = true;
        bFound = true;
        break;
      }
    }
    if (!bFound) {
      // Add new entry
      FStaticSwitchParameter NewSwitch;
      NewSwitch.ParameterInfo.Name = FName(*ParamName);
      NewSwitch.Value = Value;
      NewSwitch.bOverride = true;
      StaticParams.StaticSwitchParameters.Add(NewSwitch);
    }
    Instance->UpdateStaticPermutation(StaticParams);
    Instance->PostEditChange();
    Instance->MarkPackageDirty();

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      SaveMaterialInstanceAsset(Instance);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, Instance);
    Result->SetStringField(TEXT("parameterName"), ParamName);
    Result->SetBoolField(TEXT("value"), Value);
    Bridge->SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("Static switch '%s' set to %s."), *ParamName, Value ? TEXT("true") : TEXT("false")),
                           Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // delete_node — batch removal with auto-disconnect
  // --------------------------------------------------------------------------
  return false;
}
}
#endif
