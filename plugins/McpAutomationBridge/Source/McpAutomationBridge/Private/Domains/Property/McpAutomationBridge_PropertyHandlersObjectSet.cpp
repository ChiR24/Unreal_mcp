#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Property/McpAutomationBridge_PropertyHandlersActorAccess.h"
#include "Domains/Property/McpAutomationBridge_PropertyHandlersCdoComponents.h"
#include "Domains/Property/McpAutomationBridge_PropertyHandlersCdoPropagation.h"
#include "Domains/Property/McpAutomationBridge_PropertyHandlersObjectWatch.h"
#include "Domains/Property/McpAutomationBridge_PropertyHandlersTarget.h"

#include "Dom/JsonObject.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/Reflection/McpPropertyReflection.h"
#include "Safety/McpSafeReflectionTarget.h"
#include "Core/Requests/McpResponseCaptureRegistry.h"

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

  // properties: several values on one target in one call ({BoxExtent, CollisionProfileName}). Each runs through
  // this handler under a captured id, so every write keeps its own checks; a watch stays a single-write feature.
  const TSharedPtr<FJsonObject> *Properties = nullptr;
  if (Payload->TryGetObjectField(TEXT("properties"), Properties) && (*Properties)->Values.Num() > 0) {
    TArray<TSharedPtr<FJsonValue>> Rows;
    TArray<FString> Failed;
    TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
    double InstancesUpdated = -1.0;
    for (const TPair<FString, TSharedPtr<FJsonValue>> &Pair : (*Properties)->Values) {
      TSharedPtr<FJsonObject> One = MakeShared<FJsonObject>();
      One->Values = Payload->Values;
      for (const TCHAR *Field : {TEXT("properties"), TEXT("propertyPath"), TEXT("watch")}) One->RemoveField(Field);
      One->SetStringField(TEXT("propertyName"), Pair.Key);
      One->SetField(TEXT("value"), Pair.Value);
      const FString ItemId = FString::Printf(TEXT("%s#prop%d"), *RequestId, Rows.Num());
      FMcpResponseCaptureRegistry::Get().Begin(ItemId);
      HandleSetObjectProperty(ItemId, Action, One, RequestingSocket);
      const FMcpCapturedResponse Reply = FMcpResponseCaptureRegistry::Get().End(ItemId);
      TSharedPtr<FJsonObject> Row = McpHandlerUtils::CreateResultObject();
      Row->SetStringField(TEXT("propertyName"), Pair.Key);
      Row->SetBoolField(TEXT("applied"), Reply.bSuccess);
      if (!Reply.bSuccess) Failed.Add(FString::Printf(TEXT("%s: %s"), *Pair.Key, *Reply.Message));
      // assetPath names the Blueprint or material a class-default or expression write saved: without it a batch on
      // BP_Bug's class defaults recompiled and saved the Blueprint while its receipt listed no change.
      for (const TCHAR *Field : {TEXT("value"), TEXT("actorName"), TEXT("actorPath"), TEXT("packagePath"), TEXT("blueprintCompiled"),
                                 TEXT("assetPath"), TEXT("materialRebuilt")}) {
        const TSharedPtr<FJsonValue> Value = Reply.Result.IsValid() ? Reply.Result->TryGetField(Field) : nullptr;
        if (Value.IsValid()) (FCString::Strcmp(Field, TEXT("value")) == 0 ? Row : Data)->SetField(Field, Value);
      }
      double Updated = 0.0;
      // The same placed copies follow every write of the batch, so the count is the most any one write moved, not a sum.
      if (Reply.Result.IsValid() && Reply.Result->TryGetNumberField(TEXT("instancesUpdated"), Updated)) InstancesUpdated = FMath::Max(InstancesUpdated, Updated);
      Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    if (InstancesUpdated >= 0.0) Data->SetNumberField(TEXT("instancesUpdated"), InstancesUpdated);
    Data->SetArrayField(TEXT("properties"), Rows);
    Data->SetNumberField(TEXT("applied"), Rows.Num() - Failed.Num());
    SendAutomationResponse(RequestingSocket, RequestId, Failed.Num() == 0, Failed.Num() == 0
        ? FString::Printf(TEXT("Set %d properties."), Rows.Num()) : FString::Join(Failed, TEXT("; ")),
        Data, Failed.Num() == 0 ? FString() : FString(TEXT("PROPERTY_BATCH_INCOMPLETE")));
    return true;
  }

  McpPropertyTarget::FPropertyTarget Target;
  if (!McpPropertyTarget::ResolvePropertyTarget(*this, RequestId, Payload, RequestingSocket, Target)) {
    return true;
  }
  UObject* RootObject = Target.RootObject;
  UBlueprint* ResolvedBlueprint = Target.Blueprint;
  FString ObjectPath = Target.ObjectPath;
  const FString& BlueprintPath = Target.BlueprintPath;
  const FString& PropertyName = Target.PropertyName;

  // What a Blueprint compile left behind is no target: a write there changes no default, is never saved, and used to
  // answer success.
  if (McpPropertyTarget::IsSupersededTarget(RootObject)) {
    SendAutomationError(RequestingSocket, RequestId,
        FString::Printf(TEXT("'%s' (class %s) is an object a Blueprint compile left behind: nothing reads it and nothing "
                             "saves it, so a write there changes no default. Address the Blueprint itself with "
                             "blueprintPath (its current default object), or the live actor or asset."),
                        *ObjectPath, *RootObject->GetClass()->GetName()),
        TEXT("STALE_TARGET"));
    return true;
  }

  const TSharedPtr<FJsonValue> ValueField = Payload->TryGetField(TEXT("value"));
  if (!ValueField.IsValid()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("set_object_property payload missing value field."), TEXT("INVALID_VALUE"));
      return true;
  }

  // A watch is resolved before anything is written, so a bad one leaves the target untouched.
  McpPropertyWatch::FWatch Watch;
  bool bWatch = false;
  if (!McpPropertyWatch::ParseWatch(*this, RequestId, Payload, RequestingSocket, RootObject, Watch, bWatch)) return true;

  const bool bIsClassDefaultObject = RootObject->HasAnyFlags(RF_ClassDefaultObject);
  if (AActor *Actor = Cast<AActor>(RootObject))
  {
      if (bWatch && (McpPropertyActorAccess::IsActorTransformProperty(PropertyName) ||
                     PropertyName.Equals(TEXT("bHidden"), ESearchCase::IgnoreCase)))
      {
          SendAutomationError(RequestingSocket, RequestId,
              TEXT("watch samples after a reflected property write; an actor's transform and bHidden are set directly "
                   "(control_actor.get_transform readMode motion samples an actor over time)."),
              TEXT("INVALID_ARGUMENT"));
          return true;
      }
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
    SendAutomationError(RequestingSocket, RequestId, McpSafeReflectionTarget::DenyMessage(), McpSafeReflectionTarget::DenyCode());
    return true;
  }

  // The shared resolver get_property and the component actions use: dotted
  // paths at any depth, and a bare name that lives in one struct member.
  void* TargetContainer = nullptr;
  FString ResolveError, ResolvedPath;
  FProperty* Property = McpResolvePropertyPath(RootObject, EffectivePropertyName, TargetContainer, ResolvedPath, ResolveError);
  if (!Property || !TargetContainer) {
      CreatedOverride.Rollback();
      if (EffectivePropertyName.Contains(TEXT("."))) {
          SendAutomationError(RequestingSocket, RequestId,
              FString::Printf(TEXT("Failed to resolve nested property path '%s': %s"), *PropertyName, *ResolveError),
              TEXT("PROPERTY_NOT_FOUND"));
          return true;
      }
      // Name the object that was ACTUALLY resolved, not just the path the
      // caller sent. When a sub-object path (…:PersistentLevel.Foo) fails to
      // resolve, resolution falls back to an outer object and this error
      // then blamed the property — sending callers to hunt for a property
      // that exists, on an object they never addressed. Showing both makes
      // the real fault (the path didn't resolve to what you meant) visible.
      const FString ResolvedPathName = RootObject->GetPathName();
      const FString Detail =
          ResolvedPathName.Equals(ObjectPath)
              ? FString::Printf(TEXT("Property '%s' not found on object '%s' (%s)."),
                                *PropertyName, *ObjectPath, *ResolveError)
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

  // The placed copies that never overrode this default follow it, as the details panel makes them.
  const TArray<UObject*> Followers = McpPropertyCdoPropagation::CollectFollowers(RootObject, ResolvedPath);
  RootObject->Modify();

  FString ConversionError;
  if (!ApplyJsonValueToProperty(TargetContainer, Property, ValueField, ConversionError))
  {
      CreatedOverride.Rollback();
      SendAutomationError(RequestingSocket, RequestId, ConversionError, TEXT("PROPERTY_CONVERSION_FAILED"));
      return true;
  }
  const int32 FollowersUpdated = McpPropertyCdoPropagation::ApplyToFollowers(Followers, ResolvedPath, ValueField);

  // A Blueprint's default object is replaced by every compile, and a variable the Blueprint declares gets its value
  // back from the text of its default: the write above alone was gone after the compile below, while the reply read
  // the value off the replaced copy and said success.
  const bool bDefaultObject = ResolvedBlueprint && RootObject->HasAnyFlags(RF_ClassDefaultObject);
  const bool bCompare = bDefaultObject && !Property->HasAnyPropertyFlags(CPF_InstancedReference | CPF_ContainsInstancedReference);
  FString WrittenText;
  if (bDefaultObject)
  {
      McpPropertyTarget::KeepDeclaredDefault(ResolvedBlueprint, RootObject, ResolvedPath);
      MCP_PROPERTY_EXPORT_TEXT(Property, WrittenText, Property->ContainerPtrToValuePtr<void>(TargetContainer), nullptr, nullptr, PPF_None);
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
      if (bDefaultObject)
      {
          // Read the write back off the new default object: it is what a spawn gets, what the reply describes and
          // what gets saved. A value the compile did not keep is an error, never a success.
          UObject* Replaced = RootObject;
          const FString PropertyLabel = Property->GetName();
          FString KeptText;
          const bool bFound = McpPropertyTarget::FindOnCurrentDefault(ResolvedBlueprint, ResolvedPath, RootObject, Property, TargetContainer);
          if (bFound)
          {
              MCP_PROPERTY_EXPORT_TEXT(Property, KeptText, Property->ContainerPtrToValuePtr<void>(TargetContainer), nullptr, nullptr, PPF_None);
              if (bWatch && Watch.Object.Get() == Replaced)
              {
                  Watch.Object = RootObject;
              }
          }
          if (!bFound || (bCompare && KeptText != WrittenText))
          {
              SendAutomationError(RequestingSocket, RequestId,
                  FString::Printf(TEXT("The Blueprint compile did not keep the write: '%s' reads '%s' on the Blueprint's default "
                                       "object afterwards, not '%s'."),
                                  *PropertyLabel, bFound ? *KeptText : TEXT("(property gone)"), *WrittenText),
                  TEXT("PROPERTY_SET_FAILED"));
              return true;
          }
      }
      else if (RootObject->HasAnyFlags(RF_DefaultSubObject))
      {
          // A native component of the class defaults was replaced along with them: the reply describes its successor.
          McpPropertyTarget::FindOnCurrentDefault(ResolvedBlueprint, ResolvedPath, RootObject, Property, TargetContainer, RootObject->GetFName());
      }
  }
  McpPropertyActorAccess::RefreshK2NodeTitleCacheIfNeeded(RootObject);
  const bool bMaterialRebuilt = McpPropertyActorAccess::RefreshMaterialHostAfterEdit(RootObject);

  // `saved` used to be hard-coded true while nothing reached disk: the package
  // was only marked dirty, so an InputAction's bTriggerWhenPaused set here was
  // gone after the next editor restart. Persist a real asset package; level and
  // PIE content is saved with its level, and engine content is left alone.
  bool bSaved = false;
  FString SaveSkippedReason;
  // A Blueprint target is saved through its Blueprint: a component a compile moved aside sits in /Engine/Transient.
  UPackage* OwningPackage = (ResolvedBlueprint ? static_cast<UObject*>(ResolvedBlueprint) : RootObject)->GetOutermost();
  if (!bMarkDirty) {
      SaveSkippedReason = TEXT("markDirty was false");
  } else if (OwningPackage->HasAnyPackageFlags(PKG_PlayInEditor) || OwningPackage == GetTransientPackage()) {
      // Checked before ContainsMap: a PIE package holds a map too, and "saved
      // with its level" promised a save that stopping PIE throws away.
      SaveSkippedReason = TEXT("a running-game or transient object has nothing to save; the change lasts until PIE stops");
  } else if (OwningPackage->ContainsMap()) {
      SaveSkippedReason = TEXT("level content is saved with its level");
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
  if (Followers.Num() > 0) ResultPayload->SetNumberField(TEXT("instancesUpdated"), FollowersUpdated);
  // A material expression's write reaches material instances only once its material has rebuilt its parameter
  // lists, so say that happened (a renamed ParameterName was invisible to instances until something else did).
  if (bMaterialRebuilt) {
      ResultPayload->SetBoolField(TEXT("materialRebuilt"), true);
  }
  McpHandlerUtils::AddVerification(ResultPayload, RootObject);

  if (TSharedPtr<FJsonValue> CurrentValue = McpPropertyReflection::ExportPropertyToJsonValue(TargetContainer, Property))
  {
      ResultPayload->SetField(TEXT("value"), CurrentValue);
  }

  if (bWatch) {
      McpPropertyWatch::SendAfterWatch(*this, RequestId, RequestingSocket, ResultPayload, Watch);
      return true;
  }
  SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Property value updated."), ResultPayload);
  return true;
}
