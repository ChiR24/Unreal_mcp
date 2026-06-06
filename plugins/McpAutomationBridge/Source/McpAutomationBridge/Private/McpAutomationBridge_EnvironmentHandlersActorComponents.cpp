#include "McpAutomationBridge_EnvironmentHandlersShared.h"

#if WITH_EDITOR
namespace McpEnvironmentHandlers {

UWorld *McpGetEditorWorld()
{
    return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
}
AActor *McpFindActorByNameOrClass(UClass *ActorClass, const FString &ActorName)
{
    UWorld *World = McpGetEditorWorld();
    if (!World)
    {
        return nullptr;
    }

    AActor *FirstClassMatch = nullptr;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor *Actor = *It;
        if (!Actor || (ActorClass && !Actor->IsA(ActorClass)))
        {
            continue;
        }

        if (!FirstClassMatch)
        {
            FirstClassMatch = Actor;
        }

        if (!ActorName.IsEmpty() &&
            (Actor->GetActorLabel().Equals(ActorName, ESearchCase::IgnoreCase) ||
             Actor->GetName().Equals(ActorName, ESearchCase::IgnoreCase)))
        {
            return Actor;
        }
    }

    return ActorName.IsEmpty() ? FirstClassMatch : nullptr;
}
AActor *McpFindOrSpawnActor(UClass *ActorClass, const FString &ActorName, const FVector &Location,
                                   const FRotator &Rotation)
{
    if (!ActorClass)
    {
        return nullptr;
    }

    if (AActor *Existing = McpFindActorByNameOrClass(ActorClass, ActorName))
    {
        return Existing;
    }

    const FString Label = ActorName.IsEmpty() ? ActorClass->GetName() : ActorName;
    return SpawnActorInActiveWorld<AActor>(ActorClass, Location, Rotation, Label);
}
UActorComponent *McpFindComponentByClass(AActor *Actor, UClass *ComponentClass)
{
    if (!Actor || !ComponentClass)
    {
        return nullptr;
    }

    TInlineComponentArray<UActorComponent *> Components;
    Actor->GetComponents(Components);
    for (UActorComponent *Component : Components)
    {
        if (Component && Component->IsA(ComponentClass))
        {
            return Component;
        }
    }
    return nullptr;
}
UActorComponent *McpFindOrAddComponent(AActor *Actor, UClass *ComponentClass, const FString &ComponentName)
{
    if (!Actor || !ComponentClass || !ComponentClass->IsChildOf(UActorComponent::StaticClass()))
    {
        return nullptr;
    }

    if (UActorComponent *Existing = McpFindComponentByClass(Actor, ComponentClass))
    {
        return Existing;
    }

    UActorComponent *Component = NewObject<UActorComponent>(Actor, ComponentClass,
        FName(*(ComponentName.IsEmpty() ? ComponentClass->GetName() : ComponentName)), RF_Transactional);
    if (!Component)
    {
        return nullptr;
    }

    Actor->Modify();
    Actor->AddInstanceComponent(Component);
    if (USceneComponent *SceneComp = Cast<USceneComponent>(Component))
    {
        if (USceneComponent *Root = Actor->GetRootComponent())
        {
            SceneComp->SetupAttachment(Root);
        }
        else
        {
            Actor->SetRootComponent(SceneComp);
        }
    }
    Component->RegisterComponent();
    Actor->MarkPackageDirty();
    return Component;
}
bool McpConfigureActorAndComponent(const TSharedPtr<FJsonObject> &Payload, const FString &ActorClassPath,
                                          const FString &DefaultActorName, const FString &ComponentClassPath,
                                          TSharedPtr<FJsonObject> Resp, FString &OutMessage, FString &OutErrorCode)
{
    UClass *ActorClass = LoadClass<AActor>(nullptr, *ActorClassPath);
    if (!ActorClass)
    {
        OutMessage = FString::Printf(TEXT("Required actor class is unavailable: %s"), *ActorClassPath);
        OutErrorCode = TEXT("CLASS_NOT_FOUND");
        Resp->SetStringField(TEXT("classPath"), ActorClassPath);
        return false;
    }

    const FString ActorName = McpGetFirstStringField(Payload, {TEXT("targetActor"), TEXT("actorName"), TEXT("waterBodyName"), TEXT("name")});
    const FVector Location = McpGetVectorField(Payload, TEXT("location"), FVector::ZeroVector);
    const FRotator Rotation = McpGetRotatorField(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    AActor *Actor = McpFindOrSpawnActor(ActorClass, ActorName.IsEmpty() ? DefaultActorName : ActorName, Location, Rotation);
    if (!Actor)
    {
        OutMessage = FString::Printf(TEXT("Failed to create or find actor for class: %s"), *ActorClassPath);
        OutErrorCode = TEXT("SPAWN_FAILED");
        return false;
    }

    UObject *ConfigTarget = Actor;
    UActorComponent *Component = nullptr;
    if (!ComponentClassPath.IsEmpty())
    {
        UClass *ComponentClass = LoadClass<UActorComponent>(nullptr, *ComponentClassPath);
        if (ComponentClass)
        {
            Component = McpFindOrAddComponent(Actor, ComponentClass, ComponentClass->GetName());
            if (Component)
            {
                ConfigTarget = Component;
            }
        }
    }

    TArray<FString> Applied;
    TArray<FString> Failed;
    const int32 ActorApplied = McpApplyPayloadSettings(Actor, Payload, Applied, Failed);
    int32 ComponentApplied = 0;
    if (Component)
    {
        ComponentApplied = McpApplyPayloadSettings(Component, Payload, Applied, Failed);
        Component->MarkRenderStateDirty();
    }

    Resp->SetStringField(TEXT("actorName"), Actor->GetActorLabel());
    Resp->SetStringField(TEXT("actorPath"), Actor->GetPathName());
    Resp->SetStringField(TEXT("classPath"), ActorClassPath);
    Resp->SetNumberField(TEXT("configuredPropertyCount"), ActorApplied + ComponentApplied);
    if (Component)
    {
        Resp->SetStringField(TEXT("componentName"), Component->GetName());
        Resp->SetStringField(TEXT("componentPath"), Component->GetPathName());
    }
    McpAddStringArrayField(Resp, TEXT("configuredProperties"), Applied);
    McpAddStringArrayField(Resp, TEXT("configurationErrors"), Failed);
    McpHandlerUtils::AddVerification(Resp, Actor);
    if (ConfigTarget)
    {
        Resp->SetStringField(TEXT("configuredTarget"), ConfigTarget->GetPathName());
    }
    OutMessage = FString::Printf(TEXT("Configured environment actor %s"), *Actor->GetActorLabel());
    return true;
}

} // namespace McpEnvironmentHandlers
#endif
