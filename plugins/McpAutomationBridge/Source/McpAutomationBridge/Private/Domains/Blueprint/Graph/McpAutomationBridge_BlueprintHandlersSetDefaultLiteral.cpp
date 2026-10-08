#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"
#include "Foundation/BridgeHelpers/Assets/McpAutomationBridgeHelpersAssetSaveRegistry.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintAssetLoad.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "Foundation/BridgeHelpers/Properties/McpAutomationBridgeHelpersNestedPropertyPath.h"
#include "Foundation/BridgeHelpers/Properties/McpAutomationBridgeHelpersPropertyApply.h"
#include "Foundation/Reflection/McpPropertyReflection.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/UnrealType.h"

namespace McpBlueprintHandlers {
bool HandleBlueprintSetDefaultLiteral(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  if (ActionMatchesPattern(TEXT("set_default"))) {
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

    // The published schema declares `propertyValue` and, with additionalProperties:false,
    // rejects `value` as undeclared. Reading only `value` therefore made set_default
    // uncallable through the gateway by any input at all. Prefer the contract spelling and
    // keep `value` as the legacy WebSocket fallback.
    TSharedPtr<FJsonValue> ValueField =
        LocalPayload->TryGetField(TEXT("propertyValue"));
    if (!ValueField.IsValid()) {
      ValueField = LocalPayload->TryGetField(TEXT("value"));
    }
    if (!ValueField.IsValid()) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("propertyValue (or legacy 'value') field required"), nullptr,
                             TEXT("INVALID_ARGUMENT"));
      return true;
    }

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

    // A placed instance still holding the old default takes the new one, as the Details panel
    // propagates a class default; a compile that keeps the class layout does not reinstance, so
    // without this every placed actor kept the old value under a success reply.
    void *OldValue = FMemory::Malloc(Property->GetSize(), Property->GetMinAlignment());
    Property->InitializeValue(OldValue);
    Property->CopyCompleteValue(OldValue, Property->ContainerPtrToValuePtr<void>(TargetContainer));
    FString ConversionError;
    const bool bApplied = ApplyJsonValueToProperty(TargetContainer, Property, ValueField, ConversionError);
    int32 InstancesUpdated = 0;
    TArray<UObject *> Instances;
    CDO->GetArchetypeInstances(Instances);
    for (UObject *Instance : bApplied ? Instances : TArray<UObject *>()) {
      void *InstanceContainer = Instance;
      FString Unused;
      if (!IsValid(Instance) || Instance->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) ||
          (PropertyName.Contains(TEXT(".")) &&
           ResolveNestedPropertyPath(Instance, PropertyName, InstanceContainer, Unused) != Property) ||
          !InstanceContainer ||
          !Property->Identical(Property->ContainerPtrToValuePtr<void>(InstanceContainer), OldValue)) {
        continue;
      }
      Instance->Modify();
      Property->CopyCompleteValue(Property->ContainerPtrToValuePtr<void>(InstanceContainer),
                                  Property->ContainerPtrToValuePtr<void>(TargetContainer));
      Instance->PostEditChange();
      ++InstancesUpdated;
    }
    Property->DestroyValue(OldValue);
    FMemory::Free(OldValue);
    if (!bApplied) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             ConversionError, nullptr,
                             TEXT("CONVERSION_FAILED"));
      return true;
    }

    // Capture the value before compilation invalidates the Property pointer
    const TSharedPtr<FJsonValue> CurrentValue =
        McpPropertyReflection::ExportPropertyToJsonValue(TargetContainer, Property);

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    McpSafeCompileBlueprint(Blueprint);
    const bool bSaved = SaveLoadedAssetThrottled(Blueprint);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("propertyName"), PropertyName);
    Result->SetStringField(TEXT("blueprintPath"), LocalNormalized);
    Result->SetNumberField(TEXT("instancesUpdated"), InstancesUpdated);

    if (CurrentValue.IsValid()) {
      Result->SetField(TEXT("value"), CurrentValue);
    }

    // Add verification data for the blueprint asset
    McpHandlerUtils::AddVerification(Result, Blueprint);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Default value set successfully"), Result);
    return true;
  }

  return false;
}
} // namespace McpBlueprintHandlers
