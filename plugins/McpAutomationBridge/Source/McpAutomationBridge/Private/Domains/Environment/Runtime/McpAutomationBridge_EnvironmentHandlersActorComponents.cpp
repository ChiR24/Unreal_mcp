#include "Domains/Environment/McpAutomationBridge_EnvironmentHandlersShared.h"
#include "EngineUtils.h"
#include "Editor.h"

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
    const FString EffectiveActorName = ActorName.IsEmpty() ? DefaultActorName : ActorName;
    bool bExistedBefore = false;
    if (UWorld *ProbeWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr)
    {
        for (TActorIterator<AActor> It(ProbeWorld); It; ++It)
        {
            // By label or object name, as the lookup below finds it; a label-only probe
            // reported ExponentialHeightFog_0 (labelled HeightFog) as created.
            if (It->GetActorLabel().Equals(EffectiveActorName, ESearchCase::IgnoreCase) ||
                It->GetName().Equals(EffectiveActorName, ESearchCase::IgnoreCase)) { bExistedBefore = true; break; }
        }
    }
    AActor *Actor = McpFindOrSpawnActor(ActorClass, EffectiveActorName, Location, Rotation);
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

    // Friendly aliases for component properties (dogfood #218): density -> FogDensity, etc.
    TSharedPtr<FJsonObject> EffectivePayload = MakeShared<FJsonObject>(*Payload);
    if (Component)
    {
        static const TPair<const TCHAR*, const TCHAR*> Aliases[] = {
            { TEXT("density"), TEXT("FogDensity") }, { TEXT("falloff"), TEXT("FogHeightFalloff") },
            { TEXT("maxOpacity"), TEXT("FogMaxOpacity") }, { TEXT("startDistance"), TEXT("StartDistance") },
            { TEXT("color"), TEXT("LightColor") }, { TEXT("temperature"), TEXT("Temperature") } };
        for (const auto& Alias : Aliases)
        {
            if (EffectivePayload->HasField(Alias.Key) && !EffectivePayload->HasField(Alias.Value) && Component->GetClass()->FindPropertyByName(Alias.Value))
            {
                EffectivePayload->SetField(Alias.Value, EffectivePayload->TryGetField(Alias.Key));
            }
        }
    }
    TArray<FString> Applied;
    TArray<FString> Failed;
    // A settings key that neither the actor nor its component declares used to be
    // skipped without a word, so a misspelled light color read as configured.
    const TSharedPtr<FJsonObject> *Settings = nullptr;
    if (EffectivePayload->TryGetObjectField(TEXT("settings"), Settings) && Settings)
    {
        for (const auto &Pair : (*Settings)->Values)
        {
            const FString Key(*Pair.Key);
            if (!McpFindPropertyCaseInsensitive(Actor, Key) && !(Component && McpFindPropertyCaseInsensitive(Component, Key)))
            {
                Failed.Add(FString::Printf(TEXT("%s: %s has no such property"), *Key,
                                           *(Component ? Component : static_cast<UObject *>(Actor))->GetClass()->GetName()));
            }
        }
    }
    // A directional light's azimuth/elevation name no property, so the reflection
    // pass skipped them; they are its rotation.
    int32 AnglesApplied = 0;
    if (Actor->IsA<ADirectionalLight>() && (EffectivePayload->HasField(TEXT("azimuth")) || EffectivePayload->HasField(TEXT("elevation"))))
    {
        double Azimuth = Actor->GetActorRotation().Yaw;
        double Elevation = -Actor->GetActorRotation().Pitch;
        EffectivePayload->TryGetNumberField(TEXT("azimuth"), Azimuth);
        EffectivePayload->TryGetNumberField(TEXT("elevation"), Elevation);
        Actor->Modify();
        Actor->SetActorRotation(McpSunRotation(Elevation, Azimuth));
        Resp->SetNumberField(TEXT("azimuth"), Azimuth);
        Resp->SetNumberField(TEXT("elevation"), Elevation);
        Applied.Add(TEXT("Rotation"));
        AnglesApplied = 1;
    }
    const int32 ActorApplied = AnglesApplied + McpApplyPayloadSettings(Actor, EffectivePayload, Applied, Failed);
    int32 ComponentApplied = 0;
    if (Component)
    {
        ComponentApplied = McpApplyPayloadSettings(Component, EffectivePayload, Applied, Failed);
        Component->MarkRenderStateDirty();
        if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
        {
            Primitive->RecreateRenderState_Concurrent();
        }
    }
    const int32 TotalApplied = ActorApplied + ComponentApplied;

    Resp->SetStringField(TEXT("classPath"), ActorClassPath);
    Resp->SetNumberField(TEXT("configuredPropertyCount"), TotalApplied);
    if (Component)
    {
        Resp->SetStringField(TEXT("componentName"), Component->GetName());
        Resp->SetStringField(TEXT("componentPath"), Component->GetPathName());
    }
    McpAddStringArrayField(Resp, TEXT("configuredProperties"), Applied);
    McpAddStringArrayField(Resp, TEXT("configurationErrors"), Failed);

    if (TotalApplied == 0)
    {
        // A find-or-configure with nothing to configure is a true no-op: the
        // receipt must not claim changed objects (MCPBB-085). The gateway
        // derives receipt changes[] from actorPath/actorName, so leave those
        // absent and disclose the no-op in the message instead.
        if (bExistedBefore)
        {
            OutMessage = FString::Printf(
                TEXT("Environment actor %s already existed and nothing was configured; no changes applied"),
                *Actor->GetActorLabel());
        }
        else
        {
            OutMessage = FString::Printf(
                TEXT("Environment actor %s created (%s); no properties were supplied to configure"),
                *Actor->GetActorLabel(), *Actor->GetClass()->GetName());
        }
        Resp->SetBoolField(TEXT("created"), !bExistedBefore);
        Resp->SetStringField(TEXT("actorClass"), Actor->GetClass()->GetName());
        return true;
    }

    Resp->SetStringField(TEXT("actorName"), Actor->GetActorLabel());
    Resp->SetStringField(TEXT("actorPath"), Actor->GetPathName());
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
