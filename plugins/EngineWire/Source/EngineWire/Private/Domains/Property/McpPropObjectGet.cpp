#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Property/McpPropActorAccess.h"
#include "Domains/Property/McpPropCdoComponents.h"
#include "Domains/Property/McpPropTarget.h"

#include "Dom/JsonObject.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/Reflection/McpPropertyReflection.h"
#include "Safety/McpSafeReflectionTarget.h"
#include "Core/Requests/McpResponseCaptureRegistry.h"

#include "GameFramework/Actor.h"

#include "Components/ActorComponent.h"
#include "Engine/Blueprint.h"

bool UEngineWireSubsystem::HandleGetObjectProperty(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString LowerAction = Action.ToLower();
  if (!Action.Equals(TEXT("get_object_property"), ESearchCase::IgnoreCase) &&
      !LowerAction.Contains(TEXT("get_object_property")))
    return false;

  // objectPaths: the same read on several targets in one call, as set_property writes them (eight platforms' state
  // took eight calls, or a list filtered by a shared name). Each runs through this handler, propertyNames included.
  const TArray<TSharedPtr<FJsonValue>> *Paths = nullptr;
  if (Payload->TryGetArrayField(TEXT("objectPaths"), Paths) && Paths->Num() > 0) {
    TArray<TSharedPtr<FJsonValue>> Targets;
    int32 Read = 0;
    for (const TSharedPtr<FJsonValue> &Entry : *Paths) {
      FString Path;
      if (Entry.IsValid()) Entry->TryGetString(Path);
      Path.TrimStartAndEndInline();
      TSharedPtr<FJsonObject> Row = McpHandlerUtils::CreateResultObject();
      Row->SetStringField(TEXT("objectPath"), Path);
      // An object that is not there says so once, not once per property under propertyNames.
      if (!McpHandlerUtils::ResolveObjectFromPath(Path)) {
        Row->SetStringField(TEXT("error"), McpHandlerUtils::DescribeObjectNotFound(Path));
        Targets.Add(MakeShared<FJsonValueObject>(Row));
        continue;
      }
      TSharedPtr<FJsonObject> One = MakeShared<FJsonObject>();
      One->Values = Payload->Values;
      for (const TCHAR *Field : {TEXT("objectPaths"), TEXT("actorName"), TEXT("name"), TEXT("blueprintPath")}) One->RemoveField(Field);
      One->SetStringField(TEXT("objectPath"), Path);
      const FString ItemId = FString::Printf(TEXT("%s#target%d"), *RequestId, Targets.Num());
      FMcpResponseCaptureRegistry::Get().Begin(ItemId);
      HandleGetObjectProperty(ItemId, Action, One, RequestingSocket);
      const FMcpCapturedResponse Reply = FMcpResponseCaptureRegistry::Get().End(ItemId);
      // propertyName rides along: the receipt redactor judges a value by that sibling name.
      for (const TCHAR *Field : {TEXT("propertyName"), TEXT("value"), TEXT("properties"), TEXT("missingProperties")}) {
        const TSharedPtr<FJsonValue> Value = Reply.Result.IsValid() ? Reply.Result->TryGetField(Field) : nullptr;
        if (Value.IsValid()) Row->SetField(Field, Value);
      }
      if (Reply.bSuccess) ++Read;
      else Row->SetStringField(TEXT("error"), Reply.Message);
      Targets.Add(MakeShared<FJsonValueObject>(Row));
    }
    TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
    Data->SetArrayField(TEXT("targets"), Targets);
    SendAutomationResponse(RequestingSocket, RequestId, Read > 0,
        FString::Printf(TEXT("Read %d of %d objects."), Read, Paths->Num()), Data,
        Read > 0 ? FString() : FString(TEXT("PROPERTY_NOT_FOUND")));
    return true;
  }

  // propertyNames: several reads of one target in one call, each through this handler under a captured id. A name that
  // does not resolve is listed under missingProperties with its reason; the others still answer.
  const TArray<TSharedPtr<FJsonValue>> *Names = nullptr;
  if (Payload->TryGetArrayField(TEXT("propertyNames"), Names) && Names->Num() > 0) {
    TArray<TSharedPtr<FJsonValue>> Rows, Missing;
    for (const TSharedPtr<FJsonValue> &Name : *Names) {
      const FString Wanted = Name.IsValid() ? Name->AsString() : FString();
      TSharedPtr<FJsonObject> One = MakeShared<FJsonObject>();
      One->Values = Payload->Values;
      One->RemoveField(TEXT("propertyNames"));
      One->RemoveField(TEXT("propertyPath"));
      One->SetStringField(TEXT("propertyName"), Wanted);
      const FString ItemId = FString::Printf(TEXT("%s#read%d"), *RequestId, Rows.Num() + Missing.Num());
      FMcpResponseCaptureRegistry::Get().Begin(ItemId);
      HandleGetObjectProperty(ItemId, Action, One, RequestingSocket);
      const FMcpCapturedResponse Reply = FMcpResponseCaptureRegistry::Get().End(ItemId);
      const TSharedPtr<FJsonValue> Value = Reply.Result.IsValid() ? Reply.Result->TryGetField(TEXT("value")) : nullptr;
      FString Resolved;
      if (!Reply.bSuccess || !Value.IsValid() || !Reply.Result->TryGetStringField(TEXT("propertyName"), Resolved)) {
        Missing.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("%s: %s"), *Wanted, *Reply.Message)));
        continue;
      }
      // The resolved name, never the caller's spelling: the receipt redactor judges a value by its sibling name.
      // A dotted path also comes back as asked under propertyPath (name-bearing for the redactor as well): reads
      // of IntroStage.Text and IntroRound.Text otherwise answered as two rows both named "Text".
      TSharedPtr<FJsonObject> Row = McpHandlerUtils::CreateResultObject();
      Row->SetStringField(TEXT("propertyName"), Resolved);
      if (!Wanted.Equals(Resolved, ESearchCase::CaseSensitive)) {
        Row->SetStringField(TEXT("propertyPath"), Wanted);
      }
      Row->SetField(TEXT("value"), Value);
      Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
    Data->SetArrayField(TEXT("properties"), Rows);
    if (Missing.Num() > 0) Data->SetArrayField(TEXT("missingProperties"), Missing);
    SendAutomationResponse(RequestingSocket, RequestId, Rows.Num() > 0,
        FString::Printf(TEXT("Read %d of %d properties."), Rows.Num(), Names->Num()), Data,
        Rows.Num() > 0 ? FString() : FString(TEXT("PROPERTY_NOT_FOUND")));
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

  const bool bIsCDO = RootObject->HasAnyFlags(RF_ClassDefaultObject);
  AActor *Actor = Cast<AActor>(RootObject);
  if (Actor && McpPropertyActorAccess::IsActorTransformProperty(PropertyName)) {
    if (bIsCDO) {
      SendAutomationError(RequestingSocket, RequestId,
          TEXT("Cannot read runtime transform from a Blueprint CDO. Query the SCS template or a spawned instance instead."),
          TEXT("CDO_TRANSFORM"));
      return true;
    }
    const bool bLocation = PropertyName.Equals(TEXT("ActorLocation"), ESearchCase::IgnoreCase);
    const bool bRotation = PropertyName.Equals(TEXT("ActorRotation"), ESearchCase::IgnoreCase);
    TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
    ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
    McpHandlerUtils::AddVerification(ResultPayload, Actor);
    ResultPayload->SetObjectField(TEXT("value"),
        bRotation ? McpHandlerUtils::RotatorToJson(Actor->GetActorRotation())
                  : McpHandlerUtils::VectorToJson(bLocation ? Actor->GetActorLocation() : Actor->GetActorScale3D()));
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           bLocation ? TEXT("Actor location retrieved.")
                           : bRotation ? TEXT("Actor rotation retrieved.") : TEXT("Actor scale retrieved."),
                           ResultPayload, FString());
    return true;
  }

  FString EffectivePropertyName = PropertyName;
  if (ResolvedBlueprint && PropertyName.Contains(TEXT(".")))
  {
      FString ComponentSegment, RemainingPath;
      PropertyName.Split(TEXT("."), &ComponentSegment, &RemainingPath);
      if (UActorComponent* CompTemplate = McpPropertyCdoComponents::FindCdoComponent(ResolvedBlueprint, RootObject, ComponentSegment, false))
      {
          RootObject = CompTemplate;
          EffectivePropertyName = RemainingPath;
      }
  }

  // The guard ran on the resolved root; the Blueprint component-template branch
  // above re-pointed RootObject, so the boundary is re-asserted on the target
  // the property will actually be read from. A template is rooted in a Blueprint
  // that already passed, but the fail-closed boundary must not depend on that
  // non-local invariant.
  if (!McpSafeReflectionTarget::IsAddressable(RootObject)) {
    SendAutomationError(RequestingSocket, RequestId,
                        McpSafeReflectionTarget::DenyMessage(),
                        McpSafeReflectionTarget::DenyCode());
    return true;
  }

  McpHandlerUtils::FPropertyResolveResult PropResult = McpHandlerUtils::ResolveProperty(RootObject, EffectivePropertyName);
  if (!PropResult.IsValid())
  {
      SendAutomationError(RequestingSocket, RequestId, PropResult.Error, TEXT("PROPERTY_NOT_FOUND"));
      return true;
  }

  const TSharedPtr<FJsonValue> CurrentValue =
      McpPropertyReflection::ExportPropertyToJsonValue(PropResult.Container, PropResult.Property);
  if (!CurrentValue.IsValid()) {
    SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Unable to export property %s."), *PropertyName),
        TEXT("PROPERTY_EXPORT_FAILED"));
    return true;
  }

  TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
  // Echo the RESOLVED property's canonical name, never the caller-supplied
  // string: UE property lookup is case-insensitive, and the sibling redactor
  // judges the echoed name with a case-sensitive classifier. Echoing the raw
  // caller string let `capabilitytoken` (lowercase, unsplittable) defeat sibling
  // masking while `CapabilityToken` was masked.
  ResultPayload->SetStringField(TEXT("propertyName"), PropResult.Property->GetName());
  ResultPayload->SetField(TEXT("value"), CurrentValue);

  McpHandlerUtils::AddVerification(ResultPayload, RootObject);

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Property value retrieved."), ResultPayload,
                         FString());
  return true;
}
