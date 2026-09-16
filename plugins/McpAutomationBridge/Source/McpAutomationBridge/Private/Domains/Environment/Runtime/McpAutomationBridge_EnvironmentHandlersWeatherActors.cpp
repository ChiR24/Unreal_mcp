#include "Domains/Environment/McpAutomationBridge_EnvironmentHandlersShared.h"

#if WITH_EDITOR
namespace McpEnvironmentHandlers {

bool McpConfigureParticleEmitter(const TSharedPtr<FJsonObject> &Payload, const FString &DefaultName,
                                        TSharedPtr<FJsonObject> Resp, FString &OutMessage, FString &OutErrorCode)
{
    AActor *Emitter = nullptr;
    UClass *EmitterClass = LoadClass<AActor>(nullptr, TEXT("/Script/Engine.Emitter"));
    if (EmitterClass)
    {
        Emitter = McpFindOrSpawnEnvironmentActor(Payload, EmitterClass, DefaultName);
    }
    if (!Emitter)
    {
        OutMessage = TEXT("Failed to create particle emitter actor");
        OutErrorCode = EmitterClass ? TEXT("SPAWN_FAILED") : TEXT("CLASS_NOT_FOUND");
        return false;
    }

    UParticleSystemComponent *ParticleComponent = Cast<UParticleSystemComponent>(McpFindOrAddComponent(Emitter, UParticleSystemComponent::StaticClass(), TEXT("ParticleSystem")));
    FString ParticleSystemPath;
    if (ParticleComponent && Payload->TryGetStringField(TEXT("particleSystemPath"), ParticleSystemPath) && !ParticleSystemPath.IsEmpty())
    {
        if (UParticleSystem *ParticleSystem = LoadObject<UParticleSystem>(nullptr, *ParticleSystemPath))
        {
            ParticleComponent->Modify();
            ParticleComponent->SetTemplate(ParticleSystem);
            ParticleComponent->MarkRenderStateDirty();
            Resp->SetStringField(TEXT("particleSystemPath"), ParticleSystemPath);
        }
        else
        {
            Resp->SetStringField(TEXT("particleSystemError"), FString::Printf(TEXT("Particle system not found: %s"), *ParticleSystemPath));
        }
    }

    // McpApplyEnvironmentSettings overwrites configuredPropertyCount on each
    // call, so read it back between the actor and component passes and keep the
    // sum. Weather params such as density/speed match no UPROPERTY on AEmitter
    // or UParticleSystemComponent, so this is routinely zero.
    int32 TotalApplied = 0;
    double AppliedProbe = 0.0;
    McpApplyEnvironmentSettings(Emitter, Payload, Resp);
    if (Resp->TryGetNumberField(TEXT("configuredPropertyCount"), AppliedProbe))
    {
        TotalApplied += static_cast<int32>(AppliedProbe);
    }
    if (ParticleComponent)
    {
        McpApplyEnvironmentSettings(ParticleComponent, Payload, Resp);
        if (Resp->TryGetNumberField(TEXT("configuredPropertyCount"), AppliedProbe))
        {
            TotalApplied += static_cast<int32>(AppliedProbe);
        }
        Resp->SetStringField(TEXT("componentName"), ParticleComponent->GetName());
    }
    Resp->SetNumberField(TEXT("configuredPropertyCount"), TotalApplied);
    const bool bHasTemplate = ParticleComponent && ParticleComponent->Template != nullptr;
    Resp->SetBoolField(TEXT("hasParticleSystem"), bHasTemplate);
    Resp->SetStringField(TEXT("actorName"), Emitter->GetActorLabel());
    Resp->SetStringField(TEXT("actorPath"), Emitter->GetPathName());
    McpHandlerUtils::AddVerification(Resp, Emitter);
    if (!bHasTemplate && TotalApplied == 0)
    {
        // Claiming "Configured particle emitter" for an emitter with no particle
        // system and no applied property is the false-success this capability was
        // reported for: nothing renders and nothing was set. Say so, and name the
        // input that would make it render (MCPBB-085 disclosure pattern).
        OutMessage = FString::Printf(
            TEXT("Particle emitter %s created but nothing was configured: no particleSystemPath was supplied and no payload property matched this emitter. Pass particleSystemPath to make it render."),
            *Emitter->GetActorLabel());
        return true;
    }
    OutMessage = FString::Printf(
        TEXT("Configured particle emitter %s (%d propert%s applied, particle system %s)"),
        *Emitter->GetActorLabel(), TotalApplied, (TotalApplied == 1 ? TEXT("y") : TEXT("ies")),
        bHasTemplate ? TEXT("set") : TEXT("not set"));
    return true;
}
bool McpConfigureSunPosition(const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp,
                                    FString &OutMessage, FString &OutErrorCode)
{
    AActor *SunActor = McpFindOrSpawnEnvironmentActor(Payload, ADirectionalLight::StaticClass(), TEXT("SunLight"));
    if (!SunActor)
    {
        OutMessage = TEXT("Failed to create or find directional light");
        OutErrorCode = TEXT("SPAWN_FAILED");
        return false;
    }

    double Azimuth = SunActor->GetActorRotation().Yaw;
    double Elevation = SunActor->GetActorRotation().Pitch;
    double Hour = 0.0;
    if (Payload->TryGetNumberField(TEXT("hour"), Hour) || Payload->TryGetNumberField(TEXT("time"), Hour))
    {
        Elevation = (FMath::Clamp(static_cast<float>(Hour), 0.0f, 24.0f) / 24.0f) * 360.0f - 90.0f;
    }
    Payload->TryGetNumberField(TEXT("azimuth"), Azimuth);
    Payload->TryGetNumberField(TEXT("elevation"), Elevation);

    SunActor->Modify();
    SunActor->SetActorRotation(FRotator(static_cast<float>(Elevation), static_cast<float>(Azimuth), 0.0f));
    McpApplyEnvironmentSettings(SunActor, Payload, Resp);
    if (UDirectionalLightComponent *LightComponent = Cast<UDirectionalLightComponent>(SunActor->FindComponentByClass<UDirectionalLightComponent>()))
    {
        McpApplyEnvironmentSettings(LightComponent, Payload, Resp);
        LightComponent->MarkRenderStateDirty();
    }

    Resp->SetStringField(TEXT("actorName"), SunActor->GetActorLabel());
    Resp->SetNumberField(TEXT("azimuth"), Azimuth);
    Resp->SetNumberField(TEXT("elevation"), Elevation);
    McpHandlerUtils::AddVerification(Resp, SunActor);
    OutMessage = TEXT("Sun position configured");
    return true;
}
bool McpConfigureWaterBody(const TSharedPtr<FJsonObject> &Payload, const FString &ClassPath, const FString &DefaultName,
                                  TSharedPtr<FJsonObject> Resp, FString &OutMessage, FString &OutErrorCode)
{
    const bool bCreatedOrFound = McpConfigureActorAndComponent(Payload, ClassPath, DefaultName, FString(), Resp, OutMessage, OutErrorCode);
    if (bCreatedOrFound)
    {
        if (AActor *WaterActor = McpFindWaterBodyActor(Payload))
        {
            McpSetMaterialOnActor(WaterActor, Payload, Resp);
            McpSetCollisionOnActor(WaterActor, Payload, Resp);
        }
    }
    return bCreatedOrFound;
}
bool McpConfigureWaterBodyActor(const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp,
                                       FString &OutMessage, FString &OutErrorCode)
{
    AActor *WaterActor = McpFindWaterBodyActor(Payload);
    if (!WaterActor)
    {
        OutMessage = TEXT("Water body actor not found");
        OutErrorCode = TEXT("WATER_BODY_NOT_FOUND");
        return false;
    }

    WaterActor->Modify();
    McpApplyEnvironmentSettings(WaterActor, Payload, Resp);
    McpSetMaterialOnActor(WaterActor, Payload, Resp);
    McpSetCollisionOnActor(WaterActor, Payload, Resp);
    WaterActor->MarkPackageDirty();
    Resp->SetStringField(TEXT("waterBodyName"), WaterActor->GetActorLabel());
    Resp->SetStringField(TEXT("actorPath"), WaterActor->GetPathName());
    McpHandlerUtils::AddVerification(Resp, WaterActor);
    OutMessage = TEXT("Water body configured");
    return true;
}

} // namespace McpEnvironmentHandlers
#endif
