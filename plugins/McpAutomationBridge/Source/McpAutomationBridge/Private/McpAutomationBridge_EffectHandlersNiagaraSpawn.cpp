#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_EffectHandlersPrivate.h"

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
        Context.Payload->TryGetStringField(TEXT("action"), SubAction);
        const FString LowerSubAction = SubAction.ToLower();
        bSpawnNiagara =
            bSpawnNiagara || LowerSubAction == TEXT("niagara") ||
            LowerSubAction == TEXT("spawn_niagara");
    }
    if (!bSpawnNiagara)
    {
        return false;
    }

    FString SystemPath;
    Context.Payload->TryGetStringField(TEXT("systemPath"), SystemPath);
    if (SystemPath.IsEmpty())
    {
        Context.Payload->TryGetStringField(TEXT("system"), SystemPath);
    }
    if (SystemPath.IsEmpty())
    {
        Context.Bridge.SendAutomationResponse(
            Context.Socket, Context.RequestId, false,
            TEXT("systemPath required"), nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

#if WITH_EDITOR
    if (!UEditorAssetLibrary::DoesAssetExist(SystemPath))
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

    UObject* NiagaraObject = UEditorAssetLibrary::LoadAsset(SystemPath);
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

    if (UNiagaraComponent* NiagaraComponent = Spawned->FindComponentByClass<UNiagaraComponent>())
    {
        if (NiagaraObject->IsA<UNiagaraSystem>())
        {
            NiagaraComponent->SetAsset(Cast<UNiagaraSystem>(NiagaraObject));
            NiagaraComponent->SetWorldScale3D(ReadScaleField(Context.Payload));
            NiagaraComponent->Activate(true);
        }
    }

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
    McpHandlerUtils::AddVerification(Response, Spawned);
    Context.Bridge.SendAutomationResponse(
        Context.Socket, Context.RequestId, true,
        TEXT("Niagara spawned"), Response);
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
