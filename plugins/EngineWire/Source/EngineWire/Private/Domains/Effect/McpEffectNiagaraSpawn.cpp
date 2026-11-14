#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Effect/McpEffectPrivate.h"

#include "Editor.h"
#include "NiagaraActor.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

namespace McpEffectHandlers
{
bool HandleSpawnNiagara(const FEffectActionContext& Context)
{
    // spawn_niagara, and create_effect's niagara kind.
    if (!Context.SubAction.Equals(TEXT("spawn_niagara")) && !Context.SubAction.Equals(TEXT("niagara")))
    {
        return false;
    }

    // systemPath is canonical; system / assetPath are accepted aliases.
    const FString SystemPath = ReadNiagaraSystemPathField(Context.Payload);
    if (SystemPath.IsEmpty())
    {
        Context.SendError(TEXT("systemPath required (aliases: system, assetPath)"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Package ('/Game/Dir/NS') and object ('/Game/Dir/NS.NS') forms resolve identically.
    UObject* NiagaraObject = LoadEffectAsset(SystemPath);
    if (!NiagaraObject)
    {
        Context.SendError(FString::Printf(TEXT("Niagara system asset not found: %s"), *SystemPath),
            TEXT("SYSTEM_NOT_FOUND"));
        return true;
    }

    // Resolved before the spawn so a wrong name leaves nothing behind; it used to be
    // dropped silently and the effect spawned unattached.
    FString AttachToActor;
    Context.Payload->TryGetStringField(TEXT("attachToActor"), AttachToActor);
    AActor* Parent = FindActorByLabel(AttachToActor);
    if (!AttachToActor.IsEmpty() && !Parent)
    {
        Context.SendError(FString::Printf(TEXT("attachToActor '%s' is not in the active world"), *AttachToActor),
            TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UNiagaraSystem* NiagaraSystem = Cast<UNiagaraSystem>(NiagaraObject);
    if (!NiagaraSystem)
    {
        Context.SendError(FString::Printf(TEXT("%s is not a Niagara system asset"), *SystemPath), TEXT("ASSET_TYPE_MISMATCH"));
        return true;
    }

    const TUniquePtr<FMcpScopedEditorTransaction> SpawnStep = McpBeginSpawnTransaction(Context.RequestId);
    AActor* Spawned = SpawnActorInActiveWorld<AActor>(
        ANiagaraActor::StaticClass(),
        ExtractVectorField(Context.Payload, TEXT("location"), FVector::ZeroVector),
        ExtractRotatorField(Context.Payload, TEXT("rotation"), FRotator::ZeroRotator));
    if (!Spawned)
    {
        Context.SendError(TEXT("Failed to spawn NiagaraActor"), TEXT("SPAWN_FAILED"));
        return true;
    }

    UNiagaraComponent* NiagaraComponent = Spawned->FindComponentByClass<UNiagaraComponent>();
    if (!NiagaraComponent)
    {
        Spawned->Destroy();
        Context.SendError(TEXT("Spawned NiagaraActor has no NiagaraComponent"), TEXT("SPAWN_FAILED"));
        return true;
    }
    NiagaraComponent->SetAsset(NiagaraSystem);
    NiagaraComponent->SetWorldScale3D(ReadScaleField(Context.Payload));
    // autoActivate false places the effect switched off, for a sequence particle track or a Blueprint to start; it
    // used to fire once at level start whatever cue it was placed for. The flag is written directly because
    // SetAutoActivate is refused once the component is registered, which spawning already did. Deactivate only
    // lets a running system wind down, which an editor world never ticks, so the reply said active: true.
    const bool bAutoActivate = GetJsonBoolField(Context.Payload, TEXT("autoActivate"), true);
    NiagaraComponent->bAutoActivate = bAutoActivate;
    if (bAutoActivate)
    {
        NiagaraComponent->Activate(true);
    }
    else
    {
        NiagaraComponent->DeactivateImmediate();
    }
    // Activation needs a ticking world: in edit mode the component stays inactive even
    // though the asset is assigned, so only a missing asset is a failure.
    if (!NiagaraComponent->GetAsset())
    {
        Context.SendError(TEXT("NiagaraComponent asset not set after spawn"), TEXT("SPAWN_FAILED"));
        return true;
    }
    const bool bActive = NiagaraComponent->IsActive();
    if (Parent)
    {
        Spawned->AttachToActor(Parent, FAttachmentTransformRules::KeepWorldTransform);
    }

    // actorName is the label spawn_niagara declares; the undeclared `name` used to win over it.
    const FString Name = McpGetFirstStringField(Context.Payload, {TEXT("actorName"), TEXT("name")});
    Spawned->SetActorLabel(
        !Name.IsEmpty()
            ? Name
            : FString::Printf(TEXT("Niagara_%lld"), FDateTime::Now().ToUnixTimestamp()));

    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    Response->SetStringField(TEXT("actorName"), McpActorRef(Spawned));
    Response->SetStringField(TEXT("systemPath"), NiagaraSystem->GetPathName());
    Response->SetBoolField(TEXT("active"), bActive);
    Response->SetBoolField(TEXT("autoActivate"), bAutoActivate);
    if (Parent)
    {
        Response->SetStringField(TEXT("attachedTo"), McpActorRef(Parent));
    }
    McpHandlerUtils::AddVerification(Response, Spawned);
    Context.Bridge.SendAutomationResponse(
        Context.Socket, Context.RequestId, true,
        !bAutoActivate ? TEXT("Niagara spawned switched off; a particle track or Activate starts it")
        : bActive      ? TEXT("Niagara spawned")
                       : TEXT("Niagara spawned (inactive until the world ticks)"), Response);
    return true;
}
}
