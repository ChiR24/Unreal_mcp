#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Effect/McpAutomationBridge_EffectHandlersPrivate.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "NiagaraActor.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Subsystems/EditorActorSubsystem.h"
#endif

namespace McpEffectHandlers
{
bool HandleSpawnNiagara(const FEffectActionContext& Context, bool bIsCreateEffect)
{
    bool bSpawnNiagara = Context.Lower.Equals(TEXT("spawn_niagara"));
    if (bIsCreateEffect)
    {
        FString SubAction;
        if (!Context.Payload->TryGetStringField(TEXT("subAction"), SubAction) || SubAction.IsEmpty())
        {
            Context.Payload->TryGetStringField(TEXT("action"), SubAction);
        }
        const FString LowerSubAction = SubAction.ToLower();
        bSpawnNiagara =
            bSpawnNiagara || LowerSubAction == TEXT("niagara") ||
            LowerSubAction == TEXT("spawn_niagara");
    }
    if (!bSpawnNiagara)
    {
        return false;
    }

    // systemPath is canonical; system / niagaraSystemPath / assetPath are accepted aliases.
    const FString SystemPath = ReadNiagaraSystemPathField(Context.Payload);
    if (SystemPath.IsEmpty())
    {
        Context.Bridge.SendAutomationResponse(
            Context.Socket, Context.RequestId, false,
            TEXT("systemPath required (aliases: system, niagaraSystemPath, assetPath)"), nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // BB-028: canonicalize the path so both package ('/Game/Dir/NS') and object
    // ('/Game/Dir/NS.NS') forms resolve identically for the existence check and load.
    const FString CanonicalPath = FPackageName::ObjectPathToPackageName(SystemPath);

#if WITH_EDITOR
    if (!UEditorAssetLibrary::DoesAssetExist(CanonicalPath))
    {
        Context.Bridge.SendAutomationResponse(
            Context.Socket, Context.RequestId, false,
            FString::Printf(TEXT("Niagara system asset not found: %s"), *SystemPath),
            nullptr, TEXT("SYSTEM_NOT_FOUND"));
        return true;
    }
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

    UObject* NiagaraObject = UEditorAssetLibrary::LoadAsset(CanonicalPath);
    if (!NiagaraObject)
    {
        TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("Niagara system asset not found"));
        Context.Bridge.SendAutomationResponse(
            Context.Socket, Context.RequestId, false,
            TEXT("Niagara system not found"), Response, TEXT("SYSTEM_NOT_FOUND"));
        return true;
    }

    AActor* Spawned = SpawnActorInActiveWorld<AActor>(
        ANiagaraActor::StaticClass(),
        ReadVectorField(Context.Payload, TEXT("location")),
        ReadRotatorField(Context.Payload, TEXT("rotation")));
    if (!Spawned)
    {
        Context.Bridge.SendAutomationResponse(
            Context.Socket, Context.RequestId, false,
            TEXT("Failed to spawn NiagaraActor"), nullptr, TEXT("SPAWN_FAILED"));
        return true;
    }

    UNiagaraComponent* NiagaraComponent = Spawned->FindComponentByClass<UNiagaraComponent>();
    UNiagaraSystem* NiagaraSystem = Cast<UNiagaraSystem>(NiagaraObject);
    if (!NiagaraComponent || !NiagaraSystem)
    {
        Spawned->Destroy();
        Context.Bridge.SendAutomationResponse(
            Context.Socket, Context.RequestId, false,
            NiagaraSystem
                ? TEXT("Spawned NiagaraActor has no NiagaraComponent")
                : *FString::Printf(TEXT("%s is not a Niagara system asset"), *SystemPath),
            nullptr, NiagaraSystem ? TEXT("SPAWN_FAILED") : TEXT("ASSET_TYPE_MISMATCH"));
        return true;
    }
    NiagaraComponent->SetAsset(NiagaraSystem);
    NiagaraComponent->SetWorldScale3D(ReadScaleField(Context.Payload));
    NiagaraComponent->Activate(true);
    // Activation needs a ticking world: in edit mode the component stays inactive even
    // though the asset is assigned, so only a missing asset is a failure (dogfood #107).
    if (!NiagaraComponent->GetAsset())
    {
        Context.Bridge.SendAutomationResponse(
            Context.Socket, Context.RequestId, false,
            TEXT("NiagaraComponent asset not set after spawn"), nullptr, TEXT("SPAWN_FAILED"));
        return true;
    }
    const bool bActive = NiagaraComponent->IsActive();

    FString AttachToActor;
    Context.Payload->TryGetStringField(TEXT("attachToActor"), AttachToActor);
    if (!AttachToActor.IsEmpty())
    {
        if (AActor* Parent = FindActorByLabel(*ActorSubsystem, AttachToActor))
        {
            Spawned->AttachToActor(Parent, FAttachmentTransformRules::KeepWorldTransform);
        }
    }

    FString Name;
    Context.Payload->TryGetStringField(TEXT("name"), Name);
    if (Name.IsEmpty())
    {
        Context.Payload->TryGetStringField(TEXT("actorName"), Name);
    }
    Spawned->SetActorLabel(
        !Name.IsEmpty()
            ? Name
            : FString::Printf(TEXT("Niagara_%lld"), FDateTime::Now().ToUnixTimestamp()));

    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    Response->SetStringField(TEXT("actorName"), Spawned->GetActorLabel());
    Response->SetStringField(TEXT("systemPath"), NiagaraSystem->GetPathName());
    Response->SetBoolField(TEXT("active"), bActive);
    McpHandlerUtils::AddVerification(Response, Spawned);
    Context.Bridge.SendAutomationResponse(
        Context.Socket, Context.RequestId, true,
        bActive ? TEXT("Niagara spawned") : TEXT("Niagara spawned (inactive until the world ticks)"), Response);
    return true;
#else
    Context.Bridge.SendAutomationResponse(
        Context.Socket, Context.RequestId, false,
        TEXT("spawn_niagara requires editor build."), nullptr,
        TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}
}
