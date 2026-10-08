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
      // a Blueprint's class defaults recompiled and saved the Blueprint while its receipt listed no change. Every row
      // saves the same package, so the last row's saved/saveSkippedReason is what reached disk.
      for (const TCHAR *Field : {TEXT("value"), TEXT("actorName"), TEXT("actorPath"), TEXT("packagePath"), TEXT("blueprintCompiled"),
                                 TEXT("assetPath"), TEXT("materialRebuilt"), TEXT("saved"), TEXT("saveSkippedReason")}) {
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
  // An instanced object exports under a new name in every copy, so it never reads back as written.
  const bool bCompare = !Property->HasAnyPropertyFlags(CPF_InstancedReference | CPF_ContainsInstancedReference);
  const FString BeforeText = bCompare ? McpPropertyTarget::ValueText(Property, TargetContainer) : FString();
  RootObject->Modify();

  FString ConversionError;
  if (!McpPropertyTarget::WriteValue(RootObject, EffectivePropertyName, Property, TargetContainer, ValueField, ConversionError))
  {
      CreatedOverride.Rollback();
      SendAutomationError(RequestingSocket, RequestId, ConversionError, TEXT("PROPERTY_CONVERSION_FAILED"));
      return true;
  }
  const int32 FollowersUpdated = McpPropertyCdoPropagation::ApplyToFollowers(Followers, ResolvedPath, ValueField);

  // A Blueprint's default object is replaced by every compile, and a variable the Blueprint declares gets its value
  // back from the text of its default: the write above alone was gone after the compile below, while the reply read
  // the value off the replaced copy and said success. Any other object can put a value back in its own change handling.
  const bool bDefaultObject = ResolvedBlueprint && RootObject->HasAnyFlags(RF_ClassDefaultObject);
  if (bDefaultObject)
  {
      McpPropertyTarget::KeepDeclaredDefault(ResolvedBlueprint, RootObject, ResolvedPath);
  }
  const FString WrittenText = bCompare ? McpPropertyTarget::ValueText(Property, TargetContainer) : FString();

  const bool bMarkDirty = GetJsonBoolField(Payload, TEXT("markDirty"), true);
  if (bMarkDirty)
  {
      RootObject->MarkPackageDirty();
  }

  bool bCompiledBlueprint = false;
  // The details panel names the property it changed; PostEditChange() named none, so an object that works one value
  // out from another there (a Niagara system's warm-up tick count from its warm-up time) kept the old one.
  FPropertyChangedEvent Changed(Property, EPropertyChangeType::ValueSet);
  const int32 Dot = ResolvedPath.Find(TEXT("."));
  if (FProperty* Member = RootObject->GetClass()->FindPropertyByName(FName(*ResolvedPath.Left(Dot == INDEX_NONE ? ResolvedPath.Len() : Dot)))) Changed.MemberProperty = Member;
  RootObject->PostEditChangeProperty(Changed);
  McpRefreshComponentAfterEdit(Cast<UActorComponent>(RootObject));
  McpPropertyTarget::NotifyOwners(RootObject);
  // A value the object's own change handling put back was never applied (a StaticMeshActor's mesh restored a raw
  // CollisionEnabled from its mesh's default collision), and the reply read the old value back while it said success.
  // One it only adjusted (a clamp, a warm-up time snapped to whole ticks) is applied, and the message names it.
  const FString AfterText = bCompare && !bDefaultObject ? McpPropertyTarget::ValueText(Property, TargetContainer) : WrittenText;
  const bool bAdjusted = AfterText != WrittenText;
  if (bAdjusted && AfterText == BeforeText)
  {
      CreatedOverride.Rollback();
      SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("'%s' reads '%s' again after the object's own "
          "change handling ran, not the '%s' written: the object works it out from another of its settings, so change that "
          "one instead."), *Property->GetName(), *AfterText, *WrittenText), TEXT("PROPERTY_SET_FAILED"));
      return true;
  }
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

  FString SaveSkippedReason;
  const bool bSaved = McpPropertyTarget::SaveAfterWrite(RootObject, ResolvedBlueprint, bMarkDirty, SaveSkippedReason);

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
  SendAutomationResponse(RequestingSocket, RequestId, true, bAdjusted ? FString::Printf(TEXT("Property value updated; the "
      "object's own change handling made it '%s'."), *AfterText) : FString(TEXT("Property value updated.")), ResultPayload);
  return true;
}
