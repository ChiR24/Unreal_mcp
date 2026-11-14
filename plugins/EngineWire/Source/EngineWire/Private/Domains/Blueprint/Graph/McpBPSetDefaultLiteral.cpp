#include "Domains/Blueprint/McpBPActionContext.h"
#include "Foundation/BridgeHelpers/Assets/McpAssetSaveRegistry.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPAssetLoad.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPCompileHelpers.h"
#include "Foundation/BridgeHelpers/Properties/McpNestedPropPath.h"
#include "Foundation/BridgeHelpers/Properties/McpPropApply.h"
#include "Foundation/Reflection/McpPropertyReflection.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/UnrealType.h"

namespace McpBlueprintHandlers {
bool HandleBlueprintSetDefaultLiteral(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  if (ActionMatchesPattern(TEXT("set_default"))) {
    FString Path = RequireBlueprintPath(Context, TEXT("blueprint_set_default"));
    if (Path.IsEmpty()) {
      return true;
    }

    FString PropertyName;
    LocalPayload->TryGetStringField(TEXT("propertyName"), PropertyName);
    if (PropertyName.TrimStartAndEnd().IsEmpty()) {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
                             TEXT("propertyName required"),
                             TEXT("INVALID_ARGUMENT"));
      return true;
    }

    TSharedPtr<FJsonValue> ValueField =
        LocalPayload->TryGetField(TEXT("propertyValue"));
    if (!ValueField.IsValid()) {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
                             TEXT("propertyValue field required"),
                             TEXT("INVALID_ARGUMENT"));
      return true;
    }

    UE_LOG(LogEngineWireSubsystem, Log,
           TEXT("HandleBlueprintAction: blueprint_set_default start "
                "RequestId=%s Path=%s Prop=%s"),
           *RequestId, *Path, *PropertyName);

    FString LocalNormalized;
    FString LocalLoadError;
    UBlueprint *Blueprint =
        LoadBlueprintAsset(Path, LocalNormalized, LocalLoadError);
    if (!Blueprint) {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
                             LocalLoadError.IsEmpty()
                                 ? TEXT("Failed to load blueprint")
                                 : LocalLoadError,
                             TEXT("BLUEPRINT_NOT_FOUND"));
      return true;
    }

    if (!Blueprint->GeneratedClass) {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
                             TEXT("Blueprint has no generated class"),
                             TEXT("INVALID_BLUEPRINT"));
      return true;
    }

    UObject *CDO = Blueprint->GeneratedClass->GetDefaultObject();
    if (!CDO) {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
                             TEXT("Could not get CDO"),
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
      Bridge.SendAutomationError(RequestingSocket, RequestId,
                             ResolveError.IsEmpty() ? TEXT("Property not found")
                                                    : ResolveError,
                             TEXT("PROPERTY_NOT_FOUND"));
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
      // Preview copies (the Blueprint editor's viewport, placement checks) take the value too, but only
      // actors placed in a level count: a class with no placed copies answered 2.
      const UWorld *World = Instance->GetWorld();
      if (World && (World->WorldType == EWorldType::Editor || World->WorldType == EWorldType::PIE ||
                    World->WorldType == EWorldType::Game)) {
        ++InstancesUpdated;
      }
    }
    Property->DestroyValue(OldValue);
    FMemory::Free(OldValue);
    if (!bApplied) {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
                             ConversionError,
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
