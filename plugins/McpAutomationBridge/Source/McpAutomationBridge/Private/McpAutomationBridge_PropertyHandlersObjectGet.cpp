#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_PropertyHandlersCdoComponents.h"

#include "Dom/JsonObject.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"
#include "McpPropertyReflection.h"

#include "GameFramework/Actor.h"

#if WITH_EDITOR
#include "Components/ActorComponent.h"
#include "Engine/Blueprint.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleGetObjectProperty(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString LowerAction = Action.ToLower();
  if (!Action.Equals(TEXT("get_object_property"), ESearchCase::IgnoreCase) &&
      !LowerAction.Contains(TEXT("get_object_property")))
    return false;

  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("get_object_property payload missing."),
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
  if (PropertyName.IsEmpty()) {
    SendAutomationError(
        RequestingSocket, RequestId,
        TEXT("get_object_property requires a non-empty propertyName or propertyPath."),
        TEXT("INVALID_PROPERTY"));
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
          SendAutomationError(
              RequestingSocket, RequestId,
              FString::Printf(TEXT("Unable to find object at path %s."), *ObjectPath),
              TEXT("OBJECT_NOT_FOUND"));
          return true;
      }
      if (!ResolvedPath.IsEmpty())
      {
          ObjectPath = ResolvedPath;
      }
  }

  const bool bIsCDO = RootObject->HasAnyFlags(RF_ClassDefaultObject);
  if (AActor *Actor = Cast<AActor>(RootObject)) {
    if (bIsCDO && (PropertyName.Equals(TEXT("ActorLocation"), ESearchCase::IgnoreCase) ||
                   PropertyName.Equals(TEXT("ActorRotation"), ESearchCase::IgnoreCase) ||
                   PropertyName.Equals(TEXT("ActorScale"), ESearchCase::IgnoreCase) ||
                   PropertyName.Equals(TEXT("ActorScale3D"), ESearchCase::IgnoreCase))) {
      SendAutomationError(RequestingSocket, RequestId,
          TEXT("Cannot read runtime transform from a Blueprint CDO. Query the SCS template or a spawned instance instead."),
          TEXT("CDO_TRANSFORM"));
      return true;
    }
    if (PropertyName.Equals(TEXT("ActorLocation"), ESearchCase::IgnoreCase)) {
      FVector Loc = Actor->GetActorLocation();
      TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
      ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
      McpHandlerUtils::AddVerification(ResultPayload, Actor);

      TSharedPtr<FJsonObject> ValObj = McpHandlerUtils::CreateResultObject();
      ValObj->SetNumberField(TEXT("x"), Loc.X);
      ValObj->SetNumberField(TEXT("y"), Loc.Y);
      ValObj->SetNumberField(TEXT("z"), Loc.Z);
      ResultPayload->SetField(TEXT("value"), MakeShared<FJsonValueObject>(ValObj));
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Actor location retrieved."), ResultPayload,
                             FString());
      return true;
    } else if (PropertyName.Equals(TEXT("ActorRotation"), ESearchCase::IgnoreCase)) {
      FRotator Rot = Actor->GetActorRotation();
      TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
      ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
      McpHandlerUtils::AddVerification(ResultPayload, Actor);

      TSharedPtr<FJsonObject> ValObj = McpHandlerUtils::CreateResultObject();
      ValObj->SetNumberField(TEXT("pitch"), Rot.Pitch);
      ValObj->SetNumberField(TEXT("yaw"), Rot.Yaw);
      ValObj->SetNumberField(TEXT("roll"), Rot.Roll);
      ResultPayload->SetField(TEXT("value"), MakeShared<FJsonValueObject>(ValObj));
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Actor rotation retrieved."), ResultPayload,
                             FString());
      return true;
    } else if (PropertyName.Equals(TEXT("ActorScale"), ESearchCase::IgnoreCase) ||
               PropertyName.Equals(TEXT("ActorScale3D"), ESearchCase::IgnoreCase)) {
      FVector Scale = Actor->GetActorScale3D();
      TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
      ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
      McpHandlerUtils::AddVerification(ResultPayload, Actor);

      TSharedPtr<FJsonObject> ValObj = McpHandlerUtils::CreateResultObject();
      ValObj->SetNumberField(TEXT("x"), Scale.X);
      ValObj->SetNumberField(TEXT("y"), Scale.Y);
      ValObj->SetNumberField(TEXT("z"), Scale.Z);
      ResultPayload->SetField(TEXT("value"), MakeShared<FJsonValueObject>(ValObj));
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Actor scale retrieved."), ResultPayload,
                             FString());
      return true;
    }
  }

  FString EffectivePropertyName = PropertyName;
#if WITH_EDITOR
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
#endif

  McpHandlerUtils::FPropertyResolveResult PropResult = McpHandlerUtils::ResolveProperty(RootObject, EffectivePropertyName);
  if (!PropResult.IsValid())
  {
      SendAutomationError(RequestingSocket, RequestId, PropResult.Error, TEXT("PROPERTY_NOT_FOUND"));
      return true;
  }

  const TSharedPtr<FJsonValue> CurrentValue =
      ExportPropertyToJsonValue(PropResult.Container, PropResult.Property);
  if (!CurrentValue.IsValid()) {
    SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Unable to export property %s."), *PropertyName),
        TEXT("PROPERTY_EXPORT_FAILED"));
    return true;
  }

  TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
  ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
  ResultPayload->SetField(TEXT("value"), CurrentValue);

  if (AActor* AsActor = Cast<AActor>(RootObject)) {
    McpHandlerUtils::AddVerification(ResultPayload, AsActor);
  } else {
    McpHandlerUtils::AddVerification(ResultPayload, RootObject);
  }

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Property value retrieved."), ResultPayload,
                         FString());
  return true;
}
