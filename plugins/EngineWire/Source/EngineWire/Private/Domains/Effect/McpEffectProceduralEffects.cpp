#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/McpCapabilityPrincipal.h"

#include "Domains/Effect/McpEffectPrivate.h"

#include "Foundation/McpScopedEditorTransaction.h"

#include "Editor.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Subsystems/EditorActorSubsystem.h"

namespace McpEffectHandlers
{
bool HandleCleanup(const FEffectActionContext& Context)
{
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

    UEditorActorSubsystem* ActorSubsystem = GetEditorActorSubsystem();
    if (!ActorSubsystem)
    {
        Context.SendError(TEXT("EditorActorSubsystem not available"),
            TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
        return true;
    }
    // The editor actor subsystem lists nothing while a game plays, so this answered removed=0 as if done.
    if (GEditor->PlayWorld)
    {
        Context.SendError(TEXT("Stop Play In Editor first: cleanup deletes actors from the level being edited"), TEXT("PIE_ACTIVE"));
        return true;
    }
    TArray<UObject*> Matches;
    for (AActor* Actor : ActorSubsystem->GetAllLevelActors())
    {
        if (Actor && McpCapabilityPrincipal::ActiveRequestPermitsObject(Actor) && !Actor->GetActorLabel().IsEmpty() && Actor->GetActorLabel().StartsWith(Filter, ESearchCase::IgnoreCase))
        {
            Matches.Add(Actor);
        }
    }
    // One undo step for the whole sweep: each DestroyActor opened its own.
    FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Clean Up Actors")), EMcpMutationDurability::EditorStateOnly, Matches);
    TArray<FString> Removed;
    for (UObject* Object : Matches)
    {
        AActor* Actor = CastChecked<AActor>(Object);
        const FString Label = Actor->GetActorLabel();
        if (ActorSubsystem->DestroyActor(Actor))
        {
            Removed.Add(Label);
        }
    }
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    Response->SetArrayField(TEXT("deleted"), McpHandlerUtils::ToJsonStringArray(Removed));
    Response->SetNumberField(TEXT("removed"), Removed.Num());
    Context.Bridge.SendAutomationResponse(
        Context.Socket, Context.RequestId, true,
        FString::Printf(TEXT("Cleanup completed (removed=%d)"), Removed.Num()),
        Response);
    return true;
}

static bool HandleCreateVolumetricFog(const FEffectActionContext& Context)
{
    double Density = 0.05;
    double Scattering = 0.5;
    double Extinction = 0.5;
    Context.Payload->TryGetNumberField(TEXT("density"), Density);
    Context.Payload->TryGetNumberField(TEXT("scattering"), Scattering);
    Context.Payload->TryGetNumberField(TEXT("extinction"), Extinction);

    const TUniquePtr<FMcpScopedEditorTransaction> SpawnStep = McpBeginSpawnTransaction(Context.RequestId);
    AActor* Spawned = SpawnActorInActiveWorld<AActor>(
        AExponentialHeightFog::StaticClass(),
        ExtractVectorField(Context.Payload, TEXT("location"), FVector::ZeroVector),
        FRotator::ZeroRotator);
    if (Spawned)
    {
        if (UExponentialHeightFogComponent* FogComponent =
                Spawned->FindComponentByClass<UExponentialHeightFogComponent>())
        {
            FogComponent->SetFogDensity(static_cast<float>(Density));
            FogComponent->SetVolumetricFog(true);
            FogComponent->SetVolumetricFogScatteringDistribution(static_cast<float>(Scattering));
            FogComponent->SetVolumetricFogExtinctionScale(static_cast<float>(Extinction));
        }
        FString Name;
        Context.Payload->TryGetStringField(TEXT("name"), Name);
        Spawned->SetActorLabel(
            !Name.IsEmpty()
                ? Name
                : FString::Printf(TEXT("VolumetricFog_%lld"), FDateTime::Now().ToUnixTimestamp()));
        TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        Response->SetStringField(TEXT("actorName"), McpActorRef(Spawned));
        Response->SetStringField(TEXT("effectType"), TEXT("volumetric_fog"));
        McpHandlerUtils::AddVerification(Response, Spawned);
        Context.Bridge.SendAutomationResponse(
            Context.Socket, Context.RequestId, true,
            TEXT("Volumetric fog created"), Response);
        return true;
    }
    // false reached the dispatcher's UNKNOWN_ACTION "Unhandled manage_effect action".
    Context.SendError(TEXT("Failed to spawn ExponentialHeightFog"), TEXT("SPAWN_FAILED"));
    return true;
}

bool HandleProceduralEffectAction(const FEffectActionContext& Context)
{
    if (Context.SubAction.Equals(TEXT("create_volumetric_fog")))
    {
        return HandleCreateVolumetricFog(Context);
    }

    // The four creators that place, or author and place, a Niagara system; each is its own effect name.
    static const TSet<FString> ProceduralCreators = {TEXT("create_niagara_ribbon"), TEXT("create_particle_trail"),
        TEXT("create_environment_effect"), TEXT("create_impact_effect")};
    if (!ProceduralCreators.Contains(Context.SubAction))
    {
        return false;
    }
    const FString& EffectName = Context.SubAction;

    FString SystemPath = ReadNiagaraSystemPathField(Context.Payload);
    if (!SystemPath.IsEmpty())
    {
        // An existing system was named: place it, never author a duplicate.
        return CreateNiagaraEffectFromPayload(Context, EffectName, SystemPath);
    }
    // No systemPath: the creator authors the effect itself - a new
    // UNiagaraSystem under path/savePath built from the matching engine template
    // emitter - and then places it exactly like the systemPath branch does.
    FString AuthoredSystemPath;
    TSharedPtr<FJsonObject> Details;
    FString Error;
    FString ErrorCode;
    if (!AuthorProceduralNiagaraSystem(Context, EffectName, AuthoredSystemPath, Details, Error, ErrorCode))
    {
        Context.Bridge.SendAutomationResponse(
            Context.Socket, Context.RequestId, false, Error, Details, ErrorCode);
        return true;
    }
    // The authored system is an asset first; placing it is what location asks for. Without one, every effect made
    // dropped a preview actor at the world origin of whatever level was open, one more actor to find and delete.
    if (!Context.Payload->HasField(TEXT("location")))
    {
        Details->SetStringField(TEXT("systemPath"), AuthoredSystemPath);
        Details->SetBoolField(TEXT("placed"), false);
        Context.Bridge.SendAutomationResponse(Context.Socket, Context.RequestId, true,
            FString::Printf(TEXT("%s authored %s (not placed: give location, or use kind niagara with systemPath, or a "
                                 "NiagaraComponent)"), *EffectName, *AuthoredSystemPath), Details);
        return true;
    }
    return CreateNiagaraEffectFromPayload(Context, EffectName, AuthoredSystemPath, Details);
}
}
