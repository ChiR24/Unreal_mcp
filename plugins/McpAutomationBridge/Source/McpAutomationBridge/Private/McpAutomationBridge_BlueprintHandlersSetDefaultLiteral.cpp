#include "McpAutomationBridge_BlueprintActionContext.h"
#include "McpAutomationBridgeHelpersAssetSaveRegistry.h"
#include "McpAutomationBridgeHelpersBlueprintAssetLoad.h"
#include "McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "McpAutomationBridgeHelpersNestedPropertyPath.h"
#include "McpAutomationBridgeHelpersPropertyApply.h"
#include "McpAutomationBridgeHelpersPropertyExport.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/UnrealType.h"
#endif

namespace McpBlueprintHandlers {
#if WITH_EDITOR
bool HandleBlueprintSetDefaultLiteral(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  if (ActionMatchesPattern(TEXT("blueprint_set_default")) ||
      ActionMatchesPattern(TEXT("set_default")) ||
      AlphaNumLower.Contains(TEXT("blueprintsetdefault")) ||
      AlphaNumLower.Contains(TEXT("setdefault"))) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
           TEXT("Entered blueprint_set_default handler: RequestId=%s"),
           *RequestId);
    FString Path = ResolveBlueprintRequestedPath();
    if (Path.IsEmpty()) {
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("blueprint_set_default requires a blueprint path."), nullptr,
          TEXT("INVALID_BLUEPRINT_PATH"));
      return true;
    }

    FString PropertyName;
    LocalPayload->TryGetStringField(TEXT("propertyName"), PropertyName);
    if (PropertyName.TrimStartAndEnd().IsEmpty()) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("propertyName required"), nullptr,
                             TEXT("INVALID_ARGUMENT"));
      return true;
    }

    const TSharedPtr<FJsonValue> ValueField =
        LocalPayload->TryGetField(TEXT("value"));
    if (!ValueField.IsValid()) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("value field required"), nullptr,
                             TEXT("INVALID_ARGUMENT"));
      return true;
    }

#if WITH_EDITOR
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
           TEXT("HandleBlueprintAction: blueprint_set_default start "
                "RequestId=%s Path=%s Prop=%s"),
           *RequestId, *Path, *PropertyName);

    FString LocalNormalized;
    FString LocalLoadError;
    UBlueprint *Blueprint =
        LoadBlueprintAsset(Path, LocalNormalized, LocalLoadError);
    if (!Blueprint) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             LocalLoadError.IsEmpty()
                                 ? TEXT("Failed to load blueprint")
                                 : LocalLoadError,
                             nullptr, TEXT("BLUEPRINT_NOT_FOUND"));
      return true;
    }

    if (!Blueprint->GeneratedClass) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Blueprint has no generated class"), nullptr,
                             TEXT("INVALID_BLUEPRINT"));
      return true;
    }

    UObject *CDO = Blueprint->GeneratedClass->GetDefaultObject();
    if (!CDO) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Could not get CDO"), nullptr,
                             TEXT("INVALID_BLUEPRINT"));
      return true;
    }

    void *TargetContainer = nullptr;
    FProperty *Property = nullptr;
    FString ResolveError;

    if (PropertyName.Contains(TEXT("."))) {
      Property = ResolveNestedPropertyPath(CDO, PropertyName, TargetContainer,
                                           ResolveError);
    } else {
      TargetContainer = CDO;
      Property = CDO->GetClass()->FindPropertyByName(*PropertyName);
      if (!Property) {
        ResolveError =
            FString::Printf(TEXT("Property '%s' not found"), *PropertyName);
      }
    }

    if (!Property || !TargetContainer) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             ResolveError.IsEmpty() ? TEXT("Property not found")
                                                    : ResolveError,
                             nullptr, TEXT("PROPERTY_NOT_FOUND"));
      return true;
    }

    Blueprint->Modify();
    CDO->Modify();

    FString ConversionError;
    if (!ApplyJsonValueToProperty(TargetContainer, Property, ValueField,
                                  ConversionError)) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             ConversionError, nullptr,
                             TEXT("CONVERSION_FAILED"));
      return true;
    }

    // Capture the value before compilation invalidates the Property pointer
    const TSharedPtr<FJsonValue> CurrentValue =
        ExportPropertyToJsonValue(TargetContainer, Property);

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    McpSafeCompileBlueprint(Blueprint);
    const bool bSaved = SaveLoadedAssetThrottled(Blueprint);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("propertyName"), PropertyName);
    Result->SetStringField(TEXT("blueprintPath"), LocalNormalized);

    if (CurrentValue.IsValid()) {
      Result->SetField(TEXT("value"), CurrentValue);
    }

    // Add verification data for the blueprint asset
    McpHandlerUtils::AddVerification(Result, Blueprint);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Default value set successfully"), Result);
    return true;
#else
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("blueprint_set_default requires editor build"),
                           nullptr, TEXT("NOT_AVAILABLE"));
    return true;
#endif
  }

  return false;
}
#endif
} // namespace McpBlueprintHandlers
