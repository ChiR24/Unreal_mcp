#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"
#include "Domains/ControlActor/McpAutomationBridge_ControlActorSupport.h"
#include "Domains/Property/McpAutomationBridge_PropertyHandlersCdoComponents.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintPaths.h"
#include "Foundation/BridgeHelpers/Properties/McpAutomationBridgeHelpersNestedPropertyPath.h"

// Split out of McpAutomationBridge_ControlActorComponentProperties.cpp: teaching
// the read path about blueprintPath pushed that shard past the 250 pure-line
// ceiling, and reading a template is a different job from writing a live actor.
bool UMcpAutomationBridgeSubsystem::HandleControlActorGetComponentProperty(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString ActorName, BlueprintPath, ComponentName, PropertyName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);
  Payload->TryGetStringField(TEXT("componentName"), ComponentName);
  Payload->TryGetStringField(TEXT("propertyName"), PropertyName);
  if (PropertyName.IsEmpty()) {
    Payload->TryGetStringField(TEXT("propertyPath"), PropertyName);
  }

  // `blueprintPath` is declared on this action -- "for CDO/component inspection
  // without spawning" -- and the sibling `details` variant honours it, but this
  // one demanded an actorName, so a component property on a Blueprint with no
  // instance in the level could not be read at all.
  if ((ActorName.IsEmpty() && BlueprintPath.IsEmpty()) || ComponentName.IsEmpty() ||
      PropertyName.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
        TEXT("componentName, propertyName and one of actorName (live actor) or blueprintPath (component template) are required"),
        TEXT("MISSING_PARAM"));
    return true;
  }

  UActorComponent* Component = nullptr;
  if (ActorName.IsEmpty()) {
    FString Normalized, LoadError;
    UBlueprint* Blueprint = LoadBlueprintAsset(BlueprintPath, Normalized, LoadError);
    if (!Blueprint) {
      SendAutomationError(Socket, RequestId,
          FString::Printf(TEXT("Blueprint not found: %s (%s)"), *BlueprintPath, *LoadError),
          TEXT("BLUEPRINT_NOT_FOUND"));
      return true;
    }
    UObject* Cdo = Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetDefaultObject() : nullptr;
    Component = McpPropertyCdoComponents::FindCdoComponent(Blueprint, Cdo, ComponentName, false);
    if (!Component) {
      SendAutomationError(Socket, RequestId,
          FString::Printf(TEXT("Component not found: %s on Blueprint: %s"), *ComponentName, *BlueprintPath),
          TEXT("COMPONENT_NOT_FOUND"));
      return true;
    }
  } else {
    AActor* Actor = FindActorByName(ActorName);
    if (!Actor) {
      SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
      return true;
    }

    // CRITICAL FIX: Use FindComponentByName helper which supports fuzzy matching
    // This handles cases where component names have numeric suffixes (e.g., "StaticMeshComponent0")
    Component = FindComponentByName(Actor, ComponentName);
    if (!Component) {
      SendAutomationError(Socket, RequestId,
          FString::Printf(TEXT("Component not found: %s on actor: %s"), *ComponentName, *ActorName),
          TEXT("COMPONENT_NOT_FOUND"));
      return true;
    }
  }

  // BB-022/023: resolve through the shared nested-path boundary so dotted
  // paths (e.g. BodyInstance.CollisionEnabled) resolve, not just single names.
  void* ContainerPtr = nullptr;
  FString ResolveError;
  FProperty* Property = ResolveNestedPropertyPath(Component, PropertyName, ContainerPtr, ResolveError);
  if (!Property) {
    SendAutomationError(Socket, RequestId,
        FString::Printf(TEXT("Property not found: %s on component: %s"), *PropertyName, *ComponentName),
        TEXT("PROPERTY_NOT_FOUND"));
    return true;
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  if (ActorName.IsEmpty()) {
    Data->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    Data->SetBoolField(TEXT("isBlueprintTemplate"), true);
  } else {
    Data->SetStringField(TEXT("actorName"), ActorName);
  }
  Data->SetStringField(TEXT("componentName"), ComponentName);
  Data->SetStringField(TEXT("propertyName"), PropertyName);
  Data->SetStringField(TEXT("propertyType"), Property->GetClass()->GetName());

  // Read from the resolved container (== Component for single-name paths).
  TSharedPtr<FJsonValue> PropertyValue = ExportPropertyToJsonValue(ContainerPtr, Property);
  if (PropertyValue.IsValid()) {
    Data->SetField(TEXT("value"), PropertyValue);
  } else {
    Data->SetStringField(TEXT("value"), TEXT("<unsupported property type>"));
  }

  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Property retrieved"), Data);
  return true;
#else
  return false;
#endif
}
