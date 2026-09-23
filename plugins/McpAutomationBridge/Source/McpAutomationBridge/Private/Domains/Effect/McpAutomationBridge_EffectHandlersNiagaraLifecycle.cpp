#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Effect/McpAutomationBridge_EffectHandlersPrivate.h"

#if WITH_EDITOR
#include "NiagaraComponent.h"
#endif

namespace McpEffectHandlers
{
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
    AActor* Actor = FindActorByLabel(SystemName);
    UNiagaraComponent* NiagaraComponent = Actor ? Actor->FindComponentByClass<UNiagaraComponent>() : nullptr;
    const bool bFound = NiagaraComponent != nullptr;

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
            NiagaraComponent->AdvanceSimulation(Steps, static_cast<float>(DeltaTime));
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
