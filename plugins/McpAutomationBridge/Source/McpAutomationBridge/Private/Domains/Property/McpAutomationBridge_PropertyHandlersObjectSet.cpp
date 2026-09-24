#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Property/McpAutomationBridge_PropertyHandlersActorAccess.h"
#include "Domains/Property/McpAutomationBridge_PropertyHandlersCdoComponents.h"

#include "Dom/JsonObject.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/Reflection/McpPropertyReflection.h"
#include "Safety/McpSafeReflectionTarget.h"

#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
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

      // A caller naming a Blueprint CDO directly (…Default__BP_Foo_C) lands here
      // rather than in the blueprintPath branch, so the Blueprint was never
      // marked modified or recompiled: the value sat on the CDO while every
      // newly SPAWNED instance kept the stale class default, and the handler
      // still answered saved:true. Recover the owning Blueprint so the write
      // below is compiled into the class defaults.
      if (RootObject && RootObject->HasAnyFlags(RF_ClassDefaultObject))
      {
          ResolvedBlueprint = UBlueprint::GetBlueprintFromClass(RootObject->GetClass());
      }
  }

  // Refuse /Script targets before ANY property is written. set_property is a
  // `write` capability with no consent requirement, so without this a
  // write-scoped principal could set bRequireCapabilityToken=false on the
  // plugin's own settings CDO — which PostEditChange() below would then persist
  // to DefaultGame.ini — and reconnect as an unauthenticated loopback Admin.
  if (!McpSafeReflectionTarget::IsAddressable(RootObject)) {
    SendAutomationError(RequestingSocket, RequestId,
                        McpSafeReflectionTarget::DenyMessage(),
                        McpSafeReflectionTarget::DenyCode());
    return true;
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

  // The guard ran on the resolved root; the Blueprint component-template branch
  // above re-pointed RootObject, so the boundary is re-asserted on the target
  // that Modify()/ApplyJsonValueToProperty()/PostEditChange() will actually hit.
  // A template is rooted in a Blueprint that already passed, but the fail-closed
  // boundary must not depend on that non-local invariant.
  if (!McpSafeReflectionTarget::IsAddressable(RootObject)) {
    SendAutomationError(RequestingSocket, RequestId,
                        McpSafeReflectionTarget::DenyMessage(),
                        McpSafeReflectionTarget::DenyCode());
    return true;
  }

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
          // Name the object that was ACTUALLY resolved, not just the path the
          // caller sent. When a sub-object path (…:PersistentLevel.Foo) fails to
          // resolve, resolution falls back to an outer object and this error
          // then blamed the property — sending callers to hunt for a property
          // that exists, on an object they never addressed. Showing both makes
          // the real fault (the path didn't resolve to what you meant) visible.
          const FString ResolvedPathName = RootObject->GetPathName();
          const FString Detail =
              ResolvedPathName.Equals(ObjectPath)
                  ? FString::Printf(TEXT("Property '%s' not found on object '%s'."),
                                    *PropertyName, *ObjectPath)
                  : FString::Printf(
                        TEXT("Property '%s' not found. Requested object '%s' did not "
                             "resolve; the request was applied to '%s' (class '%s') "
                             "instead. For a sub-object such as an actor in a level, "
                             "address it with actorName rather than objectPath."),
                        *PropertyName, *ObjectPath, *ResolvedPathName,
                        *RootObject->GetClass()->GetName());
          SendAutomationError(RequestingSocket, RequestId, Detail,
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

  bool bCompiledBlueprint = false;
#if WITH_EDITOR
  RootObject->PostEditChange();
  McpRefreshComponentAfterEdit(Cast<UActorComponent>(RootObject));
  if (ResolvedBlueprint)
  {
      FBlueprintEditorUtils::MarkBlueprintAsModified(ResolvedBlueprint);
      // Marking alone leaves the previously generated class defaults in place,
      // so a value written to the CDO does not reach instances spawned from the
      // class until something else recompiles. Compile here so the write means
      // what the caller asked for.
      FKismetEditorUtilities::CompileBlueprint(ResolvedBlueprint);
      bCompiledBlueprint = true;
  }
  McpPropertyActorAccess::RefreshK2NodeTitleCacheIfNeeded(RootObject);
#endif

  // `saved` used to be hard-coded true while nothing reached disk: the package
  // was only marked dirty, so an InputAction's bTriggerWhenPaused set here was
  // gone after the next editor restart. Persist a real asset package; level and
  // PIE content is saved with its level, and engine content is left alone.
  bool bSaved = false;
  FString SaveSkippedReason;
#if WITH_EDITOR
  UPackage* OwningPackage = RootObject->GetOutermost();
  if (!bMarkDirty) {
      SaveSkippedReason = TEXT("markDirty was false");
  } else if (OwningPackage->ContainsMap() || OwningPackage->HasAnyPackageFlags(PKG_PlayInEditor)) {
      SaveSkippedReason = TEXT("level content is saved with its level");
  } else if (OwningPackage->GetName().StartsWith(TEXT("/Engine/"))) {
      SaveSkippedReason = TEXT("engine content is not saved");
  } else {
      bSaved = McpSafeAssetSave(OwningPackage);
      if (!bSaved) {
          SaveSkippedReason = TEXT("the package could not be saved; the change is only in memory");
      }
  }
#endif

  TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
  // Echo the RESOLVED property's canonical name, never the caller-supplied
  // string: UE property lookup is case-insensitive, and the sibling redactor
  // judges the echoed name with a case-sensitive classifier.
  ResultPayload->SetStringField(TEXT("propertyName"), Property->GetName());
  ResultPayload->SetBoolField(TEXT("saved"), bSaved);
  if (!SaveSkippedReason.IsEmpty()) {
      ResultPayload->SetStringField(TEXT("saveSkippedReason"), SaveSkippedReason);
  }
  // A Blueprint write only reaches future instances once the class is rebuilt,
  // so say whether that happened rather than leaving the caller to assume it.
  ResultPayload->SetBoolField(TEXT("blueprintCompiled"), bCompiledBlueprint);
  McpPropertyActorAccess::AddObjectVerification(ResultPayload, RootObject);

  if (TSharedPtr<FJsonValue> CurrentValue = ExportPropertyToJsonValue(TargetContainer, Property))
  {
      ResultPayload->SetField(TEXT("value"), CurrentValue);
  }

  SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Property value updated."), ResultPayload);
  return true;
}
