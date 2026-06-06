#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_EffectHandlersPrivate.h"

#if WITH_EDITOR
#include "NiagaraComponent.h"
#include "Subsystems/EditorActorSubsystem.h"
#endif

namespace McpEffectHandlers
{
static bool FindNiagaraComponent(
    UEditorActorSubsystem& ActorSubsystem,
    const FString& SystemName,
    AActor*& OutActor,
    UNiagaraComponent*& OutComponent)
{
    OutActor = nullptr;
    OutComponent = nullptr;
    for (AActor* Actor : ActorSubsystem.GetAllLevelActors())
    {
        if (!Actor || !Actor->GetActorLabel().Equals(SystemName, ESearchCase::IgnoreCase))
        {
            continue;
        }
        OutActor = Actor;
        OutComponent = Actor->FindComponentByClass<UNiagaraComponent>();
        return OutComponent != nullptr;
    }
    return false;
}

bool HandleNiagaraLifecycleAction(
    const FEffectActionContext& Context,
    const FString& LowerSubAction)
{
    FString SystemName;
    Context.Payload->TryGetStringField(TEXT("systemName"), SystemName);
    if (SystemName.IsEmpty())
    {
        Context.Payload->TryGetStringField(TEXT("actorName"), SystemName);
    }

#if WITH_EDITOR
    UEditorActorSubsystem* ActorSubsystem = GetEditorActorSubsystem();
    AActor* Actor = nullptr;
    UNiagaraComponent* NiagaraComponent = nullptr;
    const bool bFound =
        ActorSubsystem && FindNiagaraComponent(*ActorSubsystem, SystemName, Actor, NiagaraComponent);

    if (LowerSubAction == TEXT("activate_niagara"))
    {
        const bool bReset = Context.Payload->HasField(TEXT("reset"))
            ? GetJsonBoolField(Context.Payload, TEXT("reset"))
            : true;
        if (bFound)
        {
            NiagaraComponent->Activate(bReset);
            TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
            Response->SetBoolField(TEXT("active"), true);
            if (Actor)
            {
                McpHandlerUtils::AddVerification(Response, Actor);
            }
            Context.Bridge.SendAutomationResponse(
                Context.Socket, Context.RequestId, true,
                TEXT("Niagara system activated."), Response);
        }
        else
        {
            Context.Bridge.SendAutomationResponse(
                Context.Socket, Context.RequestId, false,
                TEXT("Niagara system not found."), nullptr,
                TEXT("SYSTEM_NOT_FOUND"));
        }
        return true;
    }

    if (LowerSubAction == TEXT("deactivate_niagara"))
    {
        if (bFound)
        {
            NiagaraComponent->Deactivate();
            TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
            Response->SetBoolField(TEXT("success"), true);
            Response->SetStringField(TEXT("actorName"), SystemName);
            Response->SetBoolField(TEXT("active"), false);
            Context.Bridge.SendAutomationResponse(
                Context.Socket, Context.RequestId, true,
                TEXT("Niagara system deactivated."), Response);
        }
        else
        {
            Context.Bridge.SendAutomationResponse(
                Context.Socket, Context.RequestId, false,
                TEXT("Niagara system not found."), nullptr,
                TEXT("SYSTEM_NOT_FOUND"));
        }
        return true;
    }

    if (LowerSubAction == TEXT("advance_simulation"))
    {
        double DeltaTime = 0.1;
        Context.Payload->TryGetNumberField(TEXT("deltaTime"), DeltaTime);
        int32 Steps = 1;
        Context.Payload->TryGetNumberField(TEXT("steps"), Steps);
        if (bFound)
        {
            for (int32 Index = 0; Index < Steps; ++Index)
            {
                NiagaraComponent->AdvanceSimulation(Steps, static_cast<float>(DeltaTime));
            }
            TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
            Response->SetBoolField(TEXT("success"), true);
            Response->SetStringField(TEXT("actorName"), SystemName);
            Response->SetNumberField(TEXT("steps"), Steps);
            Context.Bridge.SendAutomationResponse(
                Context.Socket, Context.RequestId, true,
                TEXT("Niagara simulation advanced."), Response);
        }
        else
        {
            Context.Bridge.SendAutomationResponse(
                Context.Socket, Context.RequestId, false,
                TEXT("Niagara system not found."), nullptr,
                TEXT("SYSTEM_NOT_FOUND"));
        }
        return true;
    }
    return false;
#else
    const FString Message = FString::Printf(TEXT("%s requires editor build."), *LowerSubAction);
    Context.Bridge.SendAutomationResponse(
        Context.Socket, Context.RequestId, false, Message, nullptr,
        TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}
}
