#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Property/McpAutomationBridge_PropertyHandlersActorAccess.h"
#include "Domains/Property/McpAutomationBridge_PropertyHandlersCdoComponents.h"
#include "Domains/Property/McpAutomationBridge_PropertyHandlersTarget.h"

#include "Dom/JsonObject.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/Reflection/McpPropertyReflection.h"
#include "Safety/McpSafeReflectionTarget.h"

#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"

#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"

bool UMcpAutomationBridgeSubsystem::HandleSetObjectProperty(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString LowerAction = Action.ToLower();
  if (!Action.Equals(TEXT("set_object_property"), ESearchCase::IgnoreCase) &&
      !LowerAction.Contains(TEXT("set_object_property")))
    return false;

  McpPropertyTarget::FPropertyTarget Target;
  if (!McpPropertyTarget::ResolvePropertyTarget(*this, RequestId, Payload, RequestingSocket, Target)) {
    return true;
  }
  UObject* RootObject = Target.RootObject;
  UBlueprint* ResolvedBlueprint = Target.Blueprint;
  FString ObjectPath = Target.ObjectPath;
  const FString& BlueprintPath = Target.BlueprintPath;
  const FString& PropertyName = Target.PropertyName;

  const TSharedPtr<FJsonValue> ValueField = Payload->TryGetField(TEXT("value"));
  if (!ValueField.IsValid()) {
      SendAutomationError(RequestingSocket, RequestId,
          TEXT("set_object_property payload missing value field."),
          TEXT("INVALID_VALUE"));
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
  McpPropertyCdoComponents::FCreatedInheritedOverride CreatedOverride;
  bool bFoundCdoComponent = false;

  if (ResolvedBlueprint && PropertyName.Contains(TEXT(".")))
  {
      FString ComponentSegment, RemainingPath;
      PropertyName.Split(TEXT("."), &ComponentSegment, &RemainingPath);
      if (UActorComponent* CompTemplate = McpPropertyCdoComponents::FindCdoComponent(
          ResolvedBlueprint, RootObject, ComponentSegment, true,
          &CreatedOverride.Handler, &CreatedOverride.Key,
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
          CreatedOverride.Rollback();
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
          CreatedOverride.Rollback();
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

  RootObject->Modify();

  FString ConversionError;
  if (!ApplyJsonValueToProperty(TargetContainer, Property, ValueField, ConversionError))
  {
      CreatedOverride.Rollback();
      SendAutomationError(RequestingSocket, RequestId, ConversionError, TEXT("PROPERTY_CONVERSION_FAILED"));
      return true;
  }

  const bool bMarkDirty = GetJsonBoolField(Payload, TEXT("markDirty"), true);
  if (bMarkDirty)
  {
      RootObject->MarkPackageDirty();
  }

  bool bCompiledBlueprint = false;
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

  // `saved` used to be hard-coded true while nothing reached disk: the package
  // was only marked dirty, so an InputAction's bTriggerWhenPaused set here was
  // gone after the next editor restart. Persist a real asset package; level and
  // PIE content is saved with its level, and engine content is left alone.
  bool bSaved = false;
  FString SaveSkippedReason;
  UPackage* OwningPackage = RootObject->GetOutermost();
  if (!bMarkDirty) {
      SaveSkippedReason = TEXT("markDirty was false");
  } else if (OwningPackage->ContainsMap() || OwningPackage->HasAnyPackageFlags(PKG_PlayInEditor)) {
      SaveSkippedReason = TEXT("level content is saved with its level");
  } else if (OwningPackage == GetTransientPackage()) {
      SaveSkippedReason = TEXT("a running-game or transient object has nothing to save; the change lasts until PIE stops");
  } else if (OwningPackage->GetName().StartsWith(TEXT("/Engine/"))) {
      SaveSkippedReason = TEXT("engine content is not saved");
  } else {
      bSaved = McpSafeAssetSave(OwningPackage);
      if (!bSaved) {
          SaveSkippedReason = TEXT("the package could not be saved; the change is only in memory");
      }
  }

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
  McpHandlerUtils::AddVerification(ResultPayload, RootObject);

  if (TSharedPtr<FJsonValue> CurrentValue = McpPropertyReflection::ExportPropertyToJsonValue(TargetContainer, Property))
  {
      ResultPayload->SetField(TEXT("value"), CurrentValue);
  }

  SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Property value updated."), ResultPayload);
  return true;
}
