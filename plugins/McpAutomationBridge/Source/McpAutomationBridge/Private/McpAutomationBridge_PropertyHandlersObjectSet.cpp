#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_PropertyHandlersActorAccess.h"
#include "McpAutomationBridge_PropertyHandlersCdoComponents.h"

#include "Dom/JsonObject.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"
#include "McpPropertyReflection.h"

#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleSetObjectProperty(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString LowerAction = Action.ToLower();
  if (!Action.Equals(TEXT("set_object_property"), ESearchCase::IgnoreCase) &&
      !LowerAction.Contains(TEXT("set_object_property")))
    return false;

  if (!Payload.IsValid())
  {
      SendAutomationError(RequestingSocket, RequestId,
          TEXT("set_object_property payload missing."),
          TEXT("INVALID_PAYLOAD"));
      return true;
  }

  FString ObjectPath;
  Payload->TryGetStringField(TEXT("objectPath"), ObjectPath);
  ObjectPath.TrimStartAndEndInline();

  FString BlueprintPath;
  Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);
  BlueprintPath.TrimStartAndEndInline();

  if (ObjectPath.IsEmpty() && BlueprintPath.IsEmpty())
  {
      SendAutomationError(RequestingSocket, RequestId,
          TEXT("Either objectPath or blueprintPath is required."),
          TEXT("INVALID_OBJECT"));
      return true;
  }

  FString PropertyName;
  Payload->TryGetStringField(TEXT("propertyName"), PropertyName);
  PropertyName.TrimStartAndEndInline();
  if (PropertyName.IsEmpty())
  {
      Payload->TryGetStringField(TEXT("propertyPath"), PropertyName);
      PropertyName.TrimStartAndEndInline();
  }
  if (PropertyName.IsEmpty())
  {
      SendAutomationError(RequestingSocket, RequestId,
          TEXT("propertyName or propertyPath is required."),
          TEXT("INVALID_PROPERTY"));
      return true;
  }

  const TSharedPtr<FJsonValue> ValueField = Payload->TryGetField(TEXT("value"));
  if (!ValueField.IsValid()) {
      SendAutomationError(RequestingSocket, RequestId,
          TEXT("set_object_property payload missing value field."),
          TEXT("INVALID_VALUE"));
      return true;
  }

  UObject* RootObject = nullptr;
  UBlueprint* ResolvedBlueprint = nullptr;

  if (!BlueprintPath.IsEmpty())
  {
      FString NormalizedPath, LoadError;
      ResolvedBlueprint = LoadBlueprintAsset(BlueprintPath, NormalizedPath, LoadError);
      if (!ResolvedBlueprint)
      {
          SendAutomationError(RequestingSocket, RequestId,
              FString::Printf(TEXT("Blueprint not found: %s (%s)"), *BlueprintPath, *LoadError),
              TEXT("BLUEPRINT_NOT_FOUND"));
          return true;
      }

      UClass* GeneratedClass = ResolvedBlueprint->GeneratedClass;
      if (!GeneratedClass)
      {
          SendAutomationError(RequestingSocket, RequestId,
              TEXT("Blueprint has no GeneratedClass (not compiled?)"),
              TEXT("CDO_NOT_FOUND"));
          return true;
      }

      RootObject = GeneratedClass->GetDefaultObject();
      if (!RootObject)
      {
          SendAutomationError(RequestingSocket, RequestId,
              TEXT("Failed to get Class Default Object"),
              TEXT("CDO_NOT_FOUND"));
          return true;
      }

      ObjectPath = RootObject->GetPathName();
  }
  else
  {
      FString ResolvedPath;
      RootObject = McpHandlerUtils::ResolveObjectFromPath(ObjectPath, &ResolvedPath);
      if (!RootObject)
      {
          SendAutomationError(RequestingSocket, RequestId,
              FString::Printf(TEXT("Unable to find object at path %s."), *ObjectPath),
              TEXT("OBJECT_NOT_FOUND"));
          return true;
      }
      if (!ResolvedPath.IsEmpty())
      {
          ObjectPath = ResolvedPath;
      }
  }

  const bool bIsClassDefaultObject = RootObject->HasAnyFlags(RF_ClassDefaultObject);
  if (AActor *Actor = Cast<AActor>(RootObject))
  {
      if (McpPropertyActorAccess::TryHandleSetActorProperty(
              *this, RequestId, PropertyName, Payload, ValueField, Actor,
              bIsClassDefaultObject, RequestingSocket))
      {
          return true;
      }
  }

  FString EffectivePropertyName = PropertyName;
#if WITH_EDITOR
  UInheritableComponentHandler* CreatedInheritedOverrideHandler = nullptr;
  FComponentKey CreatedInheritedOverrideKey;
  bool bFoundCdoComponent = false;
  auto RemoveCreatedInheritedOverride = [&]() {
      if (CreatedInheritedOverrideHandler && CreatedInheritedOverrideKey.IsValid())
      {
          CreatedInheritedOverrideHandler->RemoveOverridenComponentTemplate(CreatedInheritedOverrideKey);
          CreatedInheritedOverrideHandler = nullptr;
          CreatedInheritedOverrideKey = FComponentKey();
      }
  };

  if (ResolvedBlueprint && PropertyName.Contains(TEXT(".")))
  {
      FString ComponentSegment, RemainingPath;
      PropertyName.Split(TEXT("."), &ComponentSegment, &RemainingPath);
      if (UActorComponent* CompTemplate = McpPropertyCdoComponents::FindCdoComponent(
          ResolvedBlueprint, RootObject, ComponentSegment, true,
          &CreatedInheritedOverrideHandler, &CreatedInheritedOverrideKey,
          &bFoundCdoComponent))
      {
          RootObject = CompTemplate;
          EffectivePropertyName = RemainingPath;
          ObjectPath = CompTemplate->GetPathName();
      }
      else if (bFoundCdoComponent)
      {
          SendAutomationError(RequestingSocket, RequestId,
              FString::Printf(TEXT("Unable to create inherited component override for '%s' on Blueprint '%s'."), *ComponentSegment, *BlueprintPath),
              TEXT("COMPONENT_OVERRIDE_FAILED"));
          return true;
      }
  }
#endif

  void* TargetContainer = nullptr;
  FProperty* Property = nullptr;
  if (EffectivePropertyName.Contains(TEXT("."))) {
      FString ResolveError;
      Property = ResolveNestedPropertyPath(RootObject, EffectivePropertyName, TargetContainer, ResolveError);
      if (!Property || !TargetContainer) {
#if WITH_EDITOR
          RemoveCreatedInheritedOverride();
#endif
          SendAutomationError(RequestingSocket, RequestId,
              FString::Printf(TEXT("Failed to resolve nested property path '%s': %s"), *PropertyName, *ResolveError),
              TEXT("PROPERTY_NOT_FOUND"));
          return true;
      }
  }
  else
  {
      TargetContainer = RootObject;
      Property = RootObject->GetClass()->FindPropertyByName(*EffectivePropertyName);
      if (!Property) {
#if WITH_EDITOR
          RemoveCreatedInheritedOverride();
#endif
          SendAutomationError(RequestingSocket, RequestId,
              FString::Printf(TEXT("Property '%s' not found on object '%s'."), *PropertyName, *ObjectPath),
              TEXT("PROPERTY_NOT_FOUND"));
          return true;
      }
  }

#if WITH_EDITOR
  RootObject->Modify();
#endif

  FString ConversionError;
  if (!ApplyJsonValueToProperty(TargetContainer, Property, ValueField, ConversionError))
  {
#if WITH_EDITOR
      RemoveCreatedInheritedOverride();
#endif
      SendAutomationError(RequestingSocket, RequestId, ConversionError, TEXT("PROPERTY_CONVERSION_FAILED"));
      return true;
  }

  const bool bMarkDirty = McpHandlerUtils::GetOptionalBool(Payload, TEXT("markDirty"), true);
  if (bMarkDirty)
  {
      RootObject->MarkPackageDirty();
  }

#if WITH_EDITOR
  RootObject->PostEditChange();
  if (ResolvedBlueprint)
  {
      FBlueprintEditorUtils::MarkBlueprintAsModified(ResolvedBlueprint);
  }
  McpPropertyActorAccess::RefreshK2NodeTitleCacheIfNeeded(RootObject);
#endif

  TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
  ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
  ResultPayload->SetBoolField(TEXT("saved"), true);
  McpPropertyActorAccess::AddObjectVerification(ResultPayload, RootObject);

  if (TSharedPtr<FJsonValue> CurrentValue = ExportPropertyToJsonValue(TargetContainer, Property))
  {
      ResultPayload->SetField(TEXT("value"), CurrentValue);
  }

  SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Property value updated."), ResultPayload);
  return true;
}
