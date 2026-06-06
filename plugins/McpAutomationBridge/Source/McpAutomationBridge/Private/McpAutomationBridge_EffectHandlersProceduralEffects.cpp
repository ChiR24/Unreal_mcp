#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_EffectHandlersPrivate.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Subsystems/EditorActorSubsystem.h"
#endif

namespace McpEffectHandlers
{
bool HandleCleanup(const FEffectActionContext& Context, bool bIsCreateEffect)
{
    bool bCleanup = Context.Lower.Equals(TEXT("cleanup"));
    if (bIsCreateEffect)
    {
        FString SubAction;
        Context.Payload->TryGetStringField(TEXT("action"), SubAction);
        bCleanup = bCleanup || SubAction.ToLower() == TEXT("cleanup");
    }
    if (!bCleanup)
    {
        return false;
    }

    FString Filter;
    Context.Payload->TryGetStringField(TEXT("filter"), Filter);
    if (Filter.IsEmpty())
    {
        TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        Response->SetNumberField(TEXT("removed"), 0);
        Context.Bridge.SendAutomationResponse(
            Context.Socket, Context.RequestId, true,
            TEXT("Cleanup skipped (empty filter)"), Response);
        return true;
    }

#if WITH_EDITOR
    if (!GEditor)
    {
        Context.Bridge.SendAutomationResponse(
            Context.Socket, Context.RequestId, false,
            TEXT("Editor not available"), nullptr, TEXT("EDITOR_NOT_AVAILABLE"));
        return true;
    }
    UEditorActorSubsystem* ActorSubsystem = GetEditorActorSubsystem();
    if (!ActorSubsystem)
    {
        Context.Bridge.SendAutomationResponse(
            Context.Socket, Context.RequestId, false,
            TEXT("EditorActorSubsystem not available"), nullptr,
            TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
        return true;
    }
    TArray<FString> Removed;
    for (AActor* Actor : ActorSubsystem->GetAllLevelActors())
    {
        if (!Actor)
        {
            continue;
        }
        const FString Label = Actor->GetActorLabel();
        if (!Label.IsEmpty() && Label.StartsWith(Filter, ESearchCase::IgnoreCase) &&
            ActorSubsystem->DestroyActor(Actor))
        {
            Removed.Add(Label);
        }
    }
    TArray<TSharedPtr<FJsonValue>> RemovedActors;
    for (const FString& Label : Removed)
    {
        RemovedActors.Add(MakeShared<FJsonValueString>(Label));
    }
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    Response->SetArrayField(TEXT("removedActors"), RemovedActors);
    Response->SetNumberField(TEXT("removed"), Removed.Num());
    Context.Bridge.SendAutomationResponse(
        Context.Socket, Context.RequestId, true,
        FString::Printf(TEXT("Cleanup completed (removed=%d)"), Removed.Num()),
        Response);
    return true;
#else
    Context.Bridge.SendAutomationResponse(
        Context.Socket, Context.RequestId, false,
        TEXT("cleanup requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

static bool HandleCreateVolumetricFog(const FEffectActionContext& Context)
{
#if WITH_EDITOR
    if (!GEditor)
    {
        Context.Bridge.SendAutomationResponse(
            Context.Socket, Context.RequestId, false,
            TEXT("Editor not available"), nullptr, TEXT("EDITOR_NOT_AVAILABLE"));
        return true;
    }
    if (!GetEditorActorSubsystem())
    {
        Context.Bridge.SendAutomationResponse(
            Context.Socket, Context.RequestId, false,
            TEXT("EditorActorSubsystem not available"), nullptr,
            TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
        return true;
    }

    double Density = 0.05;
    double Scattering = 0.5;
    double Extinction = 0.5;
    Context.Payload->TryGetNumberField(TEXT("density"), Density);
    Context.Payload->TryGetNumberField(TEXT("scattering"), Scattering);
    Context.Payload->TryGetNumberField(TEXT("extinction"), Extinction);

    AActor* Spawned = SpawnActorInActiveWorld<AActor>(
        AExponentialHeightFog::StaticClass(),
        ReadVectorField(Context.Payload, TEXT("location")),
        FRotator::ZeroRotator);
    if (Spawned)
    {
        if (UExponentialHeightFogComponent* FogComponent =
                Spawned->FindComponentByClass<UExponentialHeightFogComponent>())
        {
            FogComponent->SetFogDensity(static_cast<float>(Density));
#if ENGINE_MAJOR_VERSION == 5
            FogComponent->SetVolumetricFog(true);
            FogComponent->SetVolumetricFogScatteringDistribution(static_cast<float>(Scattering));
            FogComponent->SetVolumetricFogExtinctionScale(static_cast<float>(Extinction));
#endif
        }
        FString Name;
        Context.Payload->TryGetStringField(TEXT("name"), Name);
        Spawned->SetActorLabel(
            !Name.IsEmpty()
                ? Name
                : FString::Printf(TEXT("VolumetricFog_%lld"), FDateTime::Now().ToUnixTimestamp()));
        TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("actorName"), Spawned->GetActorLabel());
        Response->SetStringField(TEXT("effectType"), TEXT("volumetric_fog"));
        McpHandlerUtils::AddVerification(Response, Spawned);
        Context.Bridge.SendAutomationResponse(
            Context.Socket, Context.RequestId, true,
            TEXT("Volumetric fog created"), Response);
        return true;
    }
    return false;
#else
    Context.Bridge.SendAutomationResponse(
        Context.Socket, Context.RequestId, false,
        TEXT("create_volumetric_fog requires editor build."), nullptr,
        TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

bool HandleProceduralEffectAction(const FEffectActionContext& Context, bool bIsCreateEffect)
{
    bool bCreateRibbon = Context.Lower.Equals(TEXT("create_niagara_ribbon"));
    bool bCreateFog = Context.Lower.Equals(TEXT("create_volumetric_fog"));
    bool bCreateTrail = Context.Lower.Equals(TEXT("create_particle_trail"));
    bool bCreateEnvironment = Context.Lower.Equals(TEXT("create_environment_effect"));
    bool bCreateImpact = Context.Lower.Equals(TEXT("create_impact_effect"));
    if (bIsCreateEffect)
    {
        FString SubAction;
        Context.Payload->TryGetStringField(TEXT("action"), SubAction);
        const FString LowerSubAction = SubAction.ToLower();
        bCreateRibbon = bCreateRibbon || LowerSubAction == TEXT("create_niagara_ribbon");
        bCreateFog = bCreateFog || LowerSubAction == TEXT("create_volumetric_fog");
        bCreateTrail = bCreateTrail || LowerSubAction == TEXT("create_particle_trail");
        bCreateEnvironment = bCreateEnvironment || LowerSubAction == TEXT("create_environment_effect");
        bCreateImpact = bCreateImpact || LowerSubAction == TEXT("create_impact_effect");
    }
    if (bCreateFog)
    {
        return HandleCreateVolumetricFog(Context);
    }

    FString SystemPath;
    Context.Payload->TryGetStringField(TEXT("systemPath"), SystemPath);
    if (bCreateTrail && SystemPath.IsEmpty())
    {
        Context.Payload->TryGetStringField(TEXT("emitter"), SystemPath);
    }
    if (bCreateTrail && !SystemPath.IsEmpty())
    {
        return CreateNiagaraEffectFromPayload(Context, TEXT("create_particle_trail"), SystemPath);
    }
    if (bCreateEnvironment && !SystemPath.IsEmpty())
    {
        return CreateNiagaraEffectFromPayload(Context, TEXT("create_environment_effect"), SystemPath);
    }
    if (bCreateImpact && !SystemPath.IsEmpty())
    {
        return CreateNiagaraEffectFromPayload(Context, TEXT("create_impact_effect"), SystemPath);
    }
    if (bCreateRibbon)
    {
        return CreateNiagaraEffectFromPayload(Context, TEXT("create_niagara_ribbon"), FString());
    }
    if (bCreateTrail)
    {
#if WITH_EDITOR
        if (!GEditor)
        {
            Context.Bridge.SendAutomationResponse(
                Context.Socket, Context.RequestId, false,
                TEXT("Editor not available"), nullptr, TEXT("EDITOR_NOT_AVAILABLE"));
            return true;
        }
        TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("systemPath or emitter parameter is required for particle trail creation. Please provide a valid Niagara system asset path."));
        Context.Bridge.SendAutomationResponse(
            Context.Socket, Context.RequestId, false,
            TEXT("systemPath required for particle trail"), Response,
            TEXT("INVALID_ARGUMENT"));
        return true;
#else
        Context.Bridge.SendAutomationResponse(
            Context.Socket, Context.RequestId, false,
            TEXT("create_particle_trail requires editor build."), nullptr,
            TEXT("NOT_IMPLEMENTED"));
        return true;
#endif
    }
    if (bCreateEnvironment || bCreateImpact)
    {
        const TCHAR* EffectLabel = bCreateEnvironment ? TEXT("environment effect") : TEXT("impact effect");
        const TCHAR* ErrorText = bCreateEnvironment
            ? TEXT("systemPath parameter is required for environment effect creation. Please provide a valid Niagara system asset path.")
            : TEXT("systemPath parameter is required for impact effect creation. Please provide a valid Niagara system asset path.");
        TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), ErrorText);
        Context.Bridge.SendAutomationResponse(
            Context.Socket, Context.RequestId, false,
            FString::Printf(TEXT("systemPath required for %s"), EffectLabel),
            Response, TEXT("INVALID_ARGUMENT"));
        return true;
    }
    return false;
}
}
