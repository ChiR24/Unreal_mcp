#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Effect/McpAutomationBridge_EffectHandlersPrivate.h"

#include "Editor.h"
#include "EngineUtils.h"
#include "Misc/PackageName.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

namespace McpEffectHandlers
{
// The Niagara component on Actor, or with AssetPath the one playing that system: on Actor
// when given, else the first in the active world.
static UNiagaraComponent* FindLifecycleComponent(AActor* Actor, const FString& AssetPath)
{
    const FString Package = FPackageName::ObjectPathToPackageName(AssetPath);
    TArray<AActor*> Candidates;
    if (Actor)
    {
        Candidates.Add(Actor);
    }
    else if (UWorld* World = AssetPath.IsEmpty() ? nullptr : (GEditor && GEditor->PlayWorld ? GEditor->PlayWorld.Get() : GetEditorWorld()))
    {
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            Candidates.Add(*It);
        }
    }
    for (AActor* Candidate : Candidates)
    {
        TArray<UNiagaraComponent*> Components;
        Candidate->GetComponents(Components);
        for (UNiagaraComponent* Component : Components)
        {
            if (Component && (AssetPath.IsEmpty() || (Component->GetAsset() && Component->GetAsset()->GetOutermost()->GetName() == Package)))
            {
                return Component;
            }
        }
    }
    return nullptr;
}

bool HandleNiagaraLifecycleAction(
    const FEffectActionContext& Context,
    const FString& LowerSubAction)
{
    // actorName is what activate, deactivate and advance_simulation declare; the undeclared
    // systemName used to win over it.
    const FString SystemName = McpGetFirstStringField(Context.Payload, {TEXT("actorName"), TEXT("systemName")});
    // activate_effect declares assetPath: "by asset" used to be read by nobody.
    const FString AssetPath = GetJsonStringField(Context.Payload, TEXT("assetPath"));

    AActor* Actor = FindActorByLabel(SystemName);
    UNiagaraComponent* NiagaraComponent = (Actor || SystemName.IsEmpty()) ? FindLifecycleComponent(Actor, AssetPath) : nullptr;
    if (NiagaraComponent && !Actor)
    {
        Actor = NiagaraComponent->GetOwner();
    }
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
}
}
