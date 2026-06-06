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
bool CreateNiagaraEffectFromPayload(
    const FEffectActionContext& Context,
    const FString& EffectName,
    const FString& DefaultSystemPath)
{
#if WITH_EDITOR
    if (!GEditor)
    {
        TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("Editor not available"));
        Context.Bridge.SendAutomationResponse(
            Context.Socket, Context.RequestId, false,
            TEXT("Editor not available"), Response, TEXT("EDITOR_NOT_AVAILABLE"));
        return true;
    }
    if (!GEditor->GetEditorSubsystem<UEditorActorSubsystem>())
    {
        TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("EditorActorSubsystem not available"));
        Context.Bridge.SendAutomationResponse(
            Context.Socket, Context.RequestId, false,
            TEXT("EditorActorSubsystem not available"), Response,
            TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
        return true;
    }

    FString SystemPath = DefaultSystemPath;
    Context.Payload->TryGetStringField(TEXT("systemPath"), SystemPath);
    if (SystemPath.IsEmpty())
    {
        TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(
            TEXT("error"),
            FString::Printf(
                TEXT("systemPath is required for %s. Please provide a valid asset path (e.g. /Game/Effects/MySystem)"),
                *EffectName));
        Context.Bridge.SendAutomationResponse(
            Context.Socket, Context.RequestId, false,
            TEXT("systemPath required"), Response, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (!UEditorAssetLibrary::DoesAssetExist(SystemPath))
    {
        Context.Bridge.SendAutomationResponse(
            Context.Socket, Context.RequestId, false,
            FString::Printf(TEXT("Niagara system asset not found: %s"), *SystemPath),
            nullptr, TEXT("SYSTEM_NOT_FOUND"));
        return true;
    }
    UObject* NiagaraObject = UEditorAssetLibrary::LoadAsset(SystemPath);
    if (!NiagaraObject)
    {
        TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("Niagara system asset not found"));
        Response->SetStringField(TEXT("systemPath"), SystemPath);
        Context.Bridge.SendAutomationResponse(
            Context.Socket, Context.RequestId, false,
            TEXT("Niagara system not found"), Response, TEXT("SYSTEM_NOT_FOUND"));
        return true;
    }

    AActor* Spawned = SpawnActorInActiveWorld<AActor>(
        ANiagaraActor::StaticClass(),
        ReadVectorField(Context.Payload, TEXT("location")),
        FRotator::ZeroRotator);
    if (!Spawned)
    {
        TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("Failed to spawn Niagara actor"));
        Context.Bridge.SendAutomationResponse(
            Context.Socket, Context.RequestId, false,
            TEXT("Failed to spawn Niagara actor"), Response, TEXT("SPAWN_FAILED"));
        return true;
    }

    if (UNiagaraComponent* NiagaraComponent = Spawned->FindComponentByClass<UNiagaraComponent>())
    {
        if (NiagaraObject->IsA<UNiagaraSystem>())
        {
            NiagaraComponent->SetAsset(Cast<UNiagaraSystem>(NiagaraObject));
            NiagaraComponent->Activate(true);
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
            : FString::Printf(TEXT("%s_%lld"), *EffectName.Replace(TEXT("create_"), TEXT("")), FDateTime::Now().ToUnixTimestamp()));

    UE_LOG(
        LogMcpAutomationBridgeSubsystem,
        Verbose,
        TEXT("CreateNiagaraEffect: Spawned actor '%s' (ID: %u)"),
        *Spawned->GetActorLabel(),
        Spawned->GetUniqueID());

    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("effectType"), EffectName);
    Response->SetStringField(TEXT("systemPath"), SystemPath);
    Response->SetStringField(TEXT("actorName"), Spawned->GetActorLabel());
    Response->SetNumberField(TEXT("actorId"), Spawned->GetUniqueID());
    Context.Bridge.SendAutomationResponse(
        Context.Socket, Context.RequestId, true,
        FString::Printf(TEXT("%s created successfully"), *EffectName), Response);
    return true;
#else
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    Response->SetBoolField(TEXT("success"), false);
    Response->SetStringField(TEXT("error"), TEXT("Effect creation requires editor build"));
    Context.Bridge.SendAutomationResponse(
        Context.Socket, Context.RequestId, false,
        TEXT("Effect creation not available in non-editor build"), Response,
        TEXT("NOT_AVAILABLE"));
    return true;
#endif
}
}

bool UMcpAutomationBridgeSubsystem::CreateNiagaraEffect(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    const FString& EffectName,
    const FString& DefaultSystemPath)
{
    const FString EmptyAction;
    const FString EmptyLower;
    McpEffectHandlers::FEffectActionContext Context{
        *this, RequestId, EmptyAction, EmptyLower, Payload, RequestingSocket};
    return McpEffectHandlers::CreateNiagaraEffectFromPayload(
        Context, EffectName, DefaultSystemPath);
}
