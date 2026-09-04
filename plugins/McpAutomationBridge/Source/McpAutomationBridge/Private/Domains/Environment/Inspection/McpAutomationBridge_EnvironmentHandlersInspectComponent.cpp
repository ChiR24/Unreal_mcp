#include "Domains/Environment/McpAutomationBridge_EnvironmentHandlersShared.h"

#if WITH_EDITOR
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Domains/Property/McpAutomationBridge_PropertyHandlersCdoComponents.h"
#include "Engine/Blueprint.h"

namespace McpEnvironmentHandlers {

namespace {
const TCHAR *McpMobilityName(EComponentMobility::Type Mobility)
{
    switch (Mobility)
    {
    case EComponentMobility::Static:
        return TEXT("Static");
    case EComponentMobility::Stationary:
        return TEXT("Stationary");
    case EComponentMobility::Movable:
        return TEXT("Movable");
    default:
        return TEXT("Unknown");
    }
}
} // namespace

// Component identity, transforms (relative + world), mobility, attachment and
// primitive/mesh facts. The property dump is appended by the caller so
// inspect_object can reuse this without always paying for it.
void McpDescribeComponent(UActorComponent *Component, TSharedPtr<FJsonObject> Resp)
{
    if (!Component || !Resp.IsValid())
    {
        return;
    }
    Resp->SetStringField(TEXT("componentName"), Component->GetName());
    Resp->SetStringField(TEXT("componentClass"), Component->GetClass()->GetName());
    Resp->SetStringField(TEXT("componentClassPath"), Component->GetClass()->GetPathName());
    Resp->SetStringField(TEXT("componentPath"), Component->GetPathName());
    Resp->SetBoolField(TEXT("isActive"), Component->IsActive());
    Resp->SetBoolField(TEXT("isRegistered"), Component->IsRegistered());
    Resp->SetBoolField(TEXT("isTemplate"), Component->IsTemplate());
    if (AActor *Owner = Component->GetOwner())
    {
        Resp->SetStringField(TEXT("ownerActor"), Owner->GetActorLabel());
        Resp->SetStringField(TEXT("ownerPath"), Owner->GetPathName());
    }
    USceneComponent *Scene = Cast<USceneComponent>(Component);
    Resp->SetBoolField(TEXT("isSceneComponent"), Scene != nullptr);
    if (!Scene)
    {
        return;
    }
    Resp->SetObjectField(TEXT("relativeTransform"), McpMakeTransformObject(Scene->GetRelativeTransform()));
    Resp->SetObjectField(TEXT("worldTransform"), McpMakeTransformObject(Scene->GetComponentTransform()));
    Resp->SetStringField(TEXT("mobility"), McpMobilityName(Scene->Mobility));
    Resp->SetBoolField(TEXT("isVisible"), Scene->IsVisible());
    Resp->SetBoolField(TEXT("hiddenInGame"), Scene->bHiddenInGame != 0);
    if (USceneComponent *Parent = Scene->GetAttachParent())
    {
        Resp->SetStringField(TEXT("attachParent"), Parent->GetName());
        Resp->SetStringField(TEXT("attachSocket"), Scene->GetAttachSocketName().ToString());
    }
    Resp->SetNumberField(TEXT("childCount"), Scene->GetNumChildrenComponents());
    Resp->SetObjectField(TEXT("bounds"), McpMakeBoundsObject(Scene->Bounds.GetBox()));
    if (UPrimitiveComponent *Primitive = Cast<UPrimitiveComponent>(Scene))
    {
        Resp->SetStringField(TEXT("collisionEnabled"), StaticEnum<ECollisionEnabled::Type>()->GetNameStringByValue(
            static_cast<int64>(Primitive->GetCollisionEnabled())));
        Resp->SetStringField(TEXT("collisionProfile"), Primitive->GetCollisionProfileName().ToString());
        Resp->SetBoolField(TEXT("castShadow"), Primitive->CastShadow != 0);
    }
    if (UStaticMeshComponent *MeshComponent = Cast<UStaticMeshComponent>(Scene))
    {
        UStaticMesh *Mesh = MeshComponent->GetStaticMesh();
        Resp->SetStringField(TEXT("staticMesh"), Mesh ? Mesh->GetPathName() : TEXT(""));
    }
}

// get_component_details: a world actor (actorName/objectPath/name) or a
// Blueprint (blueprintPath, or an objectPath that resolves to one) plus
// componentName; an "Actor.Component" objectPath resolves the component directly.
bool HandleInspectComponentDetailsAction(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    auto Fail = [&](const FString &Message, const TCHAR *Code) {
        Bridge.SendAutomationError(RequestingSocket, RequestId, Message, Code);
        return true;
    };
    const FString ComponentName = McpGetFirstStringField(Payload, {TEXT("componentName"), TEXT("component"), TEXT("component_name")});
    const FString ActorRef = McpGetFirstStringField(Payload, {TEXT("actorName"), TEXT("objectPath"), TEXT("name"), TEXT("actorPath")});
    FString BlueprintPath = McpGetFirstStringField(Payload, {TEXT("blueprintPath"), TEXT("assetPath")});
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    UActorComponent *Component = nullptr;
    UObject *Target = ActorRef.IsEmpty() ? nullptr : McpHandlerUtils::ResolveObjectFromPath(ActorRef);
    if (UActorComponent *Direct = Cast<UActorComponent>(Target))
    {
        Component = Direct;
    }
    else if (AActor *Actor = Cast<AActor>(Target))
    {
        if (ComponentName.IsEmpty())
        {
            return Fail(TEXT("componentName is required for get_component_details"), TEXT("INVALID_ARGUMENT"));
        }
        Component = McpHandlerUtils::FindActorComponentByName(Actor, ComponentName);
        if (!Component)
        {
            return Fail(FString::Printf(TEXT("Component not found: %s on actor %s"), *ComponentName, *Actor->GetActorLabel()),
                        TEXT("COMPONENT_NOT_FOUND"));
        }
        Resp->SetStringField(TEXT("actorName"), Actor->GetActorLabel());
        Resp->SetStringField(TEXT("actorPath"), Actor->GetPathName());
    }
    else if (UBlueprint *AsBlueprint = Cast<UBlueprint>(Target))
    {
        BlueprintPath = AsBlueprint->GetPathName();
    }
    else if (!ActorRef.IsEmpty() && BlueprintPath.IsEmpty())
    {
        return Fail(FString::Printf(TEXT("Actor not found: %s"), *ActorRef), TEXT("ACTOR_NOT_FOUND"));
    }
    if (!Component && !BlueprintPath.IsEmpty())
    {
        if (ComponentName.IsEmpty())
        {
            return Fail(TEXT("componentName is required for get_component_details"), TEXT("INVALID_ARGUMENT"));
        }
        FString Normalized, LoadError;
        UBlueprint *Blueprint = LoadBlueprintAsset(BlueprintPath, Normalized, LoadError);
        if (!Blueprint)
        {
            return Fail(FString::Printf(TEXT("Blueprint not found: %s (%s)"), *BlueprintPath, *LoadError),
                        TEXT("BLUEPRINT_NOT_FOUND"));
        }
        UObject *Cdo = Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetDefaultObject() : nullptr;
        Component = McpPropertyCdoComponents::FindCdoComponent(Blueprint, Cdo, ComponentName, false);
        if (!Component)
        {
            return Fail(FString::Printf(TEXT("Component not found: %s on Blueprint %s"), *ComponentName, *BlueprintPath),
                        TEXT("COMPONENT_NOT_FOUND"));
        }
        Resp->SetStringField(TEXT("blueprintPath"), Normalized.IsEmpty() ? BlueprintPath : Normalized);
        Resp->SetBoolField(TEXT("isBlueprintTemplate"), true);
    }
    if (!Component)
    {
        return Fail(TEXT("actorName/objectPath (world actor) or blueprintPath, plus componentName, required for get_component_details"),
                    TEXT("INVALID_ARGUMENT"));
    }
    McpDescribeComponent(Component, Resp);
    McpAppendPropertyDump(Component, McpReadStringListField(Payload, TEXT("propertyNames"), TEXT("propertyName")), Resp);
    Resp->SetBoolField(TEXT("success"), true);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                  TEXT("Component details retrieved"), Resp, FString());
    return true;
}

} // namespace McpEnvironmentHandlers
#endif
