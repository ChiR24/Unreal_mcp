#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Effect/McpAutomationBridge_EffectHandlersPrivate.h"

#include "Editor.h"
#include "EngineUtils.h"
#include "Misc/PackageName.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemInstanceController.h"

namespace McpEffectHandlers
{
// The age in seconds the component's system instance has reached, 0 without one.
static double McpNiagaraAge(UNiagaraComponent* Component)
{
    const FNiagaraSystemInstanceControllerConstPtr Controller = Component->GetSystemInstanceController();
    return Controller.IsValid() && Controller->IsValid() ? Controller->GetAge() : 0.0;
}

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
            // A system still compiling or loading waits to start: it is not active yet.
            const bool bActive = NiagaraComponent->IsActive();
            TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
            Response->SetBoolField(TEXT("active"), bActive);
            if (Actor)
            {
                McpHandlerUtils::AddVerification(Response, Actor);
            }
            Context.Bridge.SendAutomationResponse(
                Context.Socket, Context.RequestId, true,
                bActive ? TEXT("Niagara system activated.")
                        : TEXT("Niagara system asked to activate; it is not running yet (its system is still compiling or loading)."),
                Response);
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
        if (bFound && (Steps < 1 || DeltaTime <= 0.0))
        {
            Context.Bridge.SendAutomationResponse(
                Context.Socket, Context.RequestId, false,
                TEXT("steps must be 1 or more and deltaTime above 0."), nullptr,
                TEXT("INVALID_ARGUMENT"));
        }
        else if (bFound)
        {
            if (GetJsonBoolField(Context.Payload, TEXT("reset")))
            {
                NiagaraComponent->ResetSystem();
            }
            // AdvanceSimulation does nothing, and says nothing, on a system that is not running (never activated,
            // finished, still compiling) or paused: the age it reached is what tells.
            const double AgeBefore = McpNiagaraAge(NiagaraComponent);
            NiagaraComponent->AdvanceSimulation(Steps, static_cast<float>(DeltaTime));
            const double Age = McpNiagaraAge(NiagaraComponent);
            const FNiagaraSystemInstanceControllerConstPtr Controller = NiagaraComponent->GetSystemInstanceController();
            const bool bRunning = Controller.IsValid() && Controller->IsValid();
            const bool bComplete = NiagaraComponent->IsComplete();
            TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
            Response->SetStringField(TEXT("actorName"), SystemName);
            Response->SetNumberField(TEXT("age"), Age);
            Response->SetBoolField(TEXT("complete"), bComplete);
            if (Age <= AgeBefore)
            {
                const FString Why = bRunning && Controller->IsPaused()
                    ? FString(TEXT("it is paused; pass reset true to restart it from age 0"))
                    : bRunning && bComplete
                    ? FString::Printf(TEXT("it finished at age %.2f s; pass reset true to play it again from age 0"), Age)
                    : FString(TEXT("it is not running (never activated, finished, or its system still compiling); pass reset true to start it, or advance again in a moment if it was just spawned"));
                Context.Bridge.SendAutomationResponse(
                    Context.Socket, Context.RequestId, false,
                    FString::Printf(TEXT("The Niagara system did not advance: %s."), *Why), Response,
                    TEXT("NOT_ADVANCED"));
                return true;
            }
            Response->SetNumberField(TEXT("steps"), Steps);
            Context.Bridge.SendAutomationResponse(
                Context.Socket, Context.RequestId, true,
                bComplete ? TEXT("Niagara simulation advanced; it has finished, so nothing is left to show.")
                          : TEXT("Niagara simulation advanced."),
                Response);
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
