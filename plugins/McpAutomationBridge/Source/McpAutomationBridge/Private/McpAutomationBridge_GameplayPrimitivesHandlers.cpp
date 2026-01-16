// Copyright Epic Games, Inc. All Rights Reserved.
// Phase 36: Gameplay Primitives Handlers for MCP Automation Bridge
// 62 actions across 10 systems: ValueTracker, StateMachine, WorldTime, Zone, Faction, Condition, Interaction, Schedule, Spawner, Attachment

#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"

// Gameplay Primitives Components and Subsystems
#include "McpValueTrackerComponent.h"
#include "McpStateMachineComponent.h"
#include "McpInteractableComponent.h"
#include "McpScheduleComponent.h"
#include "McpSpawnerComponent.h"
#include "McpWorldTimeSubsystem.h"
#include "McpFactionSubsystem.h"
#include "McpZoneSubsystem.h"
#include "McpConditionSubsystem.h"
#include "McpActorIdRegistrySubsystem.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Actor.h"
#endif // WITH_EDITOR

// ==================== Helper Functions ====================

namespace McpGameplayPrimitivesHelpers
{
    // Find component of type T on actor, optionally matching a key field
    template<typename T>
    T* FindMcpComponent(AActor* Actor, const FString& OptionalKey = TEXT(""))
    {
        if (!Actor) return nullptr;
        
        for (UActorComponent* Comp : Actor->GetComponents())
        {
            if (T* TypedComp = Cast<T>(Comp))
            {
                if (OptionalKey.IsEmpty())
                {
                    return TypedComp;
                }
                // For ValueTracker, match by TrackerKey
                if constexpr (std::is_same_v<T, UMcpValueTrackerComponent>)
                {
                    if (TypedComp->TrackerKey.Equals(OptionalKey, ESearchCase::IgnoreCase))
                    {
                        return TypedComp;
                    }
                }
                // For Schedule, match by ScheduleId
                else if constexpr (std::is_same_v<T, UMcpScheduleComponent>)
                {
                    if (TypedComp->ScheduleId.Equals(OptionalKey, ESearchCase::IgnoreCase))
                    {
                        return TypedComp;
                    }
                }
            }
        }
        return nullptr;
    }

    // Get actor identifier (McpId if available, else name)
    FString GetActorId(const AActor* Actor)
    {
        if (!Actor) return TEXT("");
        FString McpId = UMcpActorIdRegistrySubsystem::GetMcpIdFromActor(Actor);
        return McpId.IsEmpty() ? Actor->GetName() : McpId;
    }
}

bool UMcpAutomationBridgeSubsystem::HandleManageGameplayPrimitivesAction(
    const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("manage_gameplay_primitives"), ESearchCase::IgnoreCase) &&
        !Lower.StartsWith(TEXT("manage_gameplay_primitives")))
        return false;

    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId,
                           TEXT("manage_gameplay_primitives payload missing."),
                           TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString SubAction;
    Payload->TryGetStringField(TEXT("action"), SubAction);
    if (SubAction.IsEmpty())
    {
        Payload->TryGetStringField(TEXT("action_type"), SubAction);
    }
    const FString LowerSub = SubAction.ToLower();

#if WITH_EDITOR
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetStringField(TEXT("action"), LowerSub);
    bool bSuccess = true;
    FString Message = FString::Printf(TEXT("Gameplay primitives action '%s' completed"), *LowerSub);
    FString ErrorCode;

    UWorld* World = GetActiveWorld();
    if (!World)
    {
        SendAutomationError(RequestingSocket, RequestId,
                           TEXT("No active world found."),
                           TEXT("NO_WORLD"));
        return true;
    }

    // ==================== VALUE TRACKER (8 actions) ====================
    if (LowerSub == TEXT("create_value_tracker"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        FString TrackerKey = TEXT("Value");
        Payload->TryGetStringField(TEXT("trackerKey"), TrackerKey);
        
        double InitialValue = 100.0;
        Payload->TryGetNumberField(TEXT("initialValue"), InitialValue);
        double MinValue = 0.0;
        Payload->TryGetNumberField(TEXT("minValue"), MinValue);
        double MaxValue = 100.0;
        Payload->TryGetNumberField(TEXT("maxValue"), MaxValue);
        
        AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
        if (!TargetActor)
        {
            bSuccess = false;
            Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
            ErrorCode = TEXT("ACTOR_NOT_FOUND");
        }
        else
        {
            // Check if tracker with this key already exists
            UMcpValueTrackerComponent* ExistingComp = McpGameplayPrimitivesHelpers::FindMcpComponent<UMcpValueTrackerComponent>(TargetActor, TrackerKey);
            if (ExistingComp)
            {
                bSuccess = false;
                Message = FString::Printf(TEXT("ValueTracker with key '%s' already exists on '%s'"), *TrackerKey, *ActorName);
                ErrorCode = TEXT("TRACKER_EXISTS");
            }
            else
            {
                UMcpValueTrackerComponent* Comp = NewObject<UMcpValueTrackerComponent>(TargetActor, *FString::Printf(TEXT("ValueTracker_%s"), *TrackerKey));
                if (Comp)
                {
                    Comp->RegisterComponent();
                    Comp->TrackerKey = TrackerKey;
                    Comp->CurrentValue = static_cast<float>(InitialValue);
                    Comp->MinValue = static_cast<float>(MinValue);
                    Comp->MaxValue = static_cast<float>(MaxValue);
                    
                    Resp->SetStringField(TEXT("actorName"), ActorName);
                    Resp->SetStringField(TEXT("trackerKey"), TrackerKey);
                    Resp->SetNumberField(TEXT("currentValue"), Comp->CurrentValue);
                    Resp->SetNumberField(TEXT("minValue"), Comp->MinValue);
                    Resp->SetNumberField(TEXT("maxValue"), Comp->MaxValue);
                    Message = FString::Printf(TEXT("Created ValueTracker '%s' on actor '%s'"), *TrackerKey, *ActorName);
                }
                else
                {
                    bSuccess = false;
                    Message = TEXT("Failed to create ValueTrackerComponent");
                    ErrorCode = TEXT("CREATE_FAILED");
                }
            }
        }
    }
    
    else if (LowerSub == TEXT("modify_value"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        FString TrackerKey;
        Payload->TryGetStringField(TEXT("trackerKey"), TrackerKey);
        double Delta = 0.0;
        Payload->TryGetNumberField(TEXT("delta"), Delta);
        
        AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
        if (!TargetActor)
        {
            bSuccess = false;
            Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
            ErrorCode = TEXT("ACTOR_NOT_FOUND");
        }
        else
        {
            UMcpValueTrackerComponent* Comp = McpGameplayPrimitivesHelpers::FindMcpComponent<UMcpValueTrackerComponent>(TargetActor, TrackerKey);
            if (!Comp)
            {
                bSuccess = false;
                Message = FString::Printf(TEXT("ValueTracker '%s' not found on actor '%s'"), *TrackerKey, *ActorName);
                ErrorCode = TEXT("TRACKER_NOT_FOUND");
            }
            else
            {
                float OldValue = Comp->CurrentValue;
                Comp->ModifyValue(static_cast<float>(Delta));
                
                Resp->SetStringField(TEXT("trackerKey"), TrackerKey);
                Resp->SetNumberField(TEXT("previousValue"), OldValue);
                Resp->SetNumberField(TEXT("currentValue"), Comp->CurrentValue);
                Resp->SetNumberField(TEXT("delta"), Delta);
                Resp->SetNumberField(TEXT("percentage"), Comp->GetPercentage());
                Message = FString::Printf(TEXT("Modified '%s' by %.2f (%.2f -> %.2f)"), *TrackerKey, Delta, OldValue, Comp->CurrentValue);
            }
        }
    }
    
    else if (LowerSub == TEXT("set_value"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        FString TrackerKey;
        Payload->TryGetStringField(TEXT("trackerKey"), TrackerKey);
        double NewValue = 0.0;
        Payload->TryGetNumberField(TEXT("value"), NewValue);
        
        AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
        if (!TargetActor)
        {
            bSuccess = false;
            Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
            ErrorCode = TEXT("ACTOR_NOT_FOUND");
        }
        else
        {
            UMcpValueTrackerComponent* Comp = McpGameplayPrimitivesHelpers::FindMcpComponent<UMcpValueTrackerComponent>(TargetActor, TrackerKey);
            if (!Comp)
            {
                bSuccess = false;
                Message = FString::Printf(TEXT("ValueTracker '%s' not found on actor '%s'"), *TrackerKey, *ActorName);
                ErrorCode = TEXT("TRACKER_NOT_FOUND");
            }
            else
            {
                float OldValue = Comp->CurrentValue;
                Comp->SetValue(static_cast<float>(NewValue));
                
                Resp->SetStringField(TEXT("trackerKey"), TrackerKey);
                Resp->SetNumberField(TEXT("previousValue"), OldValue);
                Resp->SetNumberField(TEXT("currentValue"), Comp->CurrentValue);
                Resp->SetNumberField(TEXT("percentage"), Comp->GetPercentage());
                Message = FString::Printf(TEXT("Set '%s' to %.2f (was %.2f)"), *TrackerKey, Comp->CurrentValue, OldValue);
            }
        }
    }
    
    else if (LowerSub == TEXT("get_value"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        FString TrackerKey;
        Payload->TryGetStringField(TEXT("trackerKey"), TrackerKey);
        
        AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
        if (!TargetActor)
        {
            bSuccess = false;
            Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
            ErrorCode = TEXT("ACTOR_NOT_FOUND");
        }
        else
        {
            UMcpValueTrackerComponent* Comp = McpGameplayPrimitivesHelpers::FindMcpComponent<UMcpValueTrackerComponent>(TargetActor, TrackerKey);
            if (!Comp)
            {
                bSuccess = false;
                Message = FString::Printf(TEXT("ValueTracker '%s' not found on actor '%s'"), *TrackerKey, *ActorName);
                ErrorCode = TEXT("TRACKER_NOT_FOUND");
            }
            else
            {
                Resp->SetStringField(TEXT("trackerKey"), Comp->TrackerKey);
                Resp->SetNumberField(TEXT("currentValue"), Comp->CurrentValue);
                Resp->SetNumberField(TEXT("minValue"), Comp->MinValue);
                Resp->SetNumberField(TEXT("maxValue"), Comp->MaxValue);
                Resp->SetNumberField(TEXT("percentage"), Comp->GetPercentage());
                Resp->SetBoolField(TEXT("isPaused"), Comp->bIsPaused);
                Resp->SetNumberField(TEXT("decayRate"), Comp->DecayRate);
                Resp->SetNumberField(TEXT("regenRate"), Comp->RegenRate);
                Message = FString::Printf(TEXT("ValueTracker '%s': %.2f / %.2f (%.1f%%)"), *TrackerKey, Comp->CurrentValue, Comp->MaxValue, Comp->GetPercentage() * 100.0f);
            }
        }
    }
    
    else if (LowerSub == TEXT("add_value_threshold"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        FString TrackerKey;
        Payload->TryGetStringField(TEXT("trackerKey"), TrackerKey);
        double ThresholdValue = 0.0;
        Payload->TryGetNumberField(TEXT("thresholdValue"), ThresholdValue);
        FString Direction = TEXT("below");
        Payload->TryGetStringField(TEXT("direction"), Direction);
        FString EventId;
        Payload->TryGetStringField(TEXT("eventId"), EventId);
        
        AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
        if (!TargetActor)
        {
            bSuccess = false;
            Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
            ErrorCode = TEXT("ACTOR_NOT_FOUND");
        }
        else
        {
            UMcpValueTrackerComponent* Comp = McpGameplayPrimitivesHelpers::FindMcpComponent<UMcpValueTrackerComponent>(TargetActor, TrackerKey);
            if (!Comp)
            {
                bSuccess = false;
                Message = FString::Printf(TEXT("ValueTracker '%s' not found on actor '%s'"), *TrackerKey, *ActorName);
                ErrorCode = TEXT("TRACKER_NOT_FOUND");
            }
            else
            {
                Comp->AddThreshold(static_cast<float>(ThresholdValue), Direction, EventId);
                
                Resp->SetStringField(TEXT("trackerKey"), TrackerKey);
                Resp->SetNumberField(TEXT("thresholdValue"), ThresholdValue);
                Resp->SetStringField(TEXT("direction"), Direction);
                Resp->SetStringField(TEXT("eventId"), EventId);
                Resp->SetNumberField(TEXT("thresholdCount"), Comp->Thresholds.Num());
                Message = FString::Printf(TEXT("Added threshold %.2f (%s) to '%s'"), ThresholdValue, *Direction, *TrackerKey);
            }
        }
    }
    
    else if (LowerSub == TEXT("configure_value_decay"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        FString TrackerKey;
        Payload->TryGetStringField(TEXT("trackerKey"), TrackerKey);
        double DecayRate = 0.0;
        Payload->TryGetNumberField(TEXT("decayRate"), DecayRate);
        double DecayInterval = 1.0;
        Payload->TryGetNumberField(TEXT("decayInterval"), DecayInterval);
        
        AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
        if (!TargetActor)
        {
            bSuccess = false;
            Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
            ErrorCode = TEXT("ACTOR_NOT_FOUND");
        }
        else
        {
            UMcpValueTrackerComponent* Comp = McpGameplayPrimitivesHelpers::FindMcpComponent<UMcpValueTrackerComponent>(TargetActor, TrackerKey);
            if (!Comp)
            {
                bSuccess = false;
                Message = FString::Printf(TEXT("ValueTracker '%s' not found on actor '%s'"), *TrackerKey, *ActorName);
                ErrorCode = TEXT("TRACKER_NOT_FOUND");
            }
            else
            {
                Comp->ConfigureDecay(static_cast<float>(DecayRate), static_cast<float>(DecayInterval));
                
                Resp->SetStringField(TEXT("trackerKey"), TrackerKey);
                Resp->SetNumberField(TEXT("decayRate"), Comp->DecayRate);
                Resp->SetNumberField(TEXT("decayInterval"), Comp->DecayInterval);
                Message = FString::Printf(TEXT("Configured decay for '%s': %.2f per %.2fs"), *TrackerKey, Comp->DecayRate, Comp->DecayInterval);
            }
        }
    }
    
    else if (LowerSub == TEXT("configure_value_regen"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        FString TrackerKey;
        Payload->TryGetStringField(TEXT("trackerKey"), TrackerKey);
        double RegenRate = 0.0;
        Payload->TryGetNumberField(TEXT("regenRate"), RegenRate);
        double RegenInterval = 1.0;
        Payload->TryGetNumberField(TEXT("regenInterval"), RegenInterval);
        
        AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
        if (!TargetActor)
        {
            bSuccess = false;
            Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
            ErrorCode = TEXT("ACTOR_NOT_FOUND");
        }
        else
        {
            UMcpValueTrackerComponent* Comp = McpGameplayPrimitivesHelpers::FindMcpComponent<UMcpValueTrackerComponent>(TargetActor, TrackerKey);
            if (!Comp)
            {
                bSuccess = false;
                Message = FString::Printf(TEXT("ValueTracker '%s' not found on actor '%s'"), *TrackerKey, *ActorName);
                ErrorCode = TEXT("TRACKER_NOT_FOUND");
            }
            else
            {
                Comp->ConfigureRegen(static_cast<float>(RegenRate), static_cast<float>(RegenInterval));
                
                Resp->SetStringField(TEXT("trackerKey"), TrackerKey);
                Resp->SetNumberField(TEXT("regenRate"), Comp->RegenRate);
                Resp->SetNumberField(TEXT("regenInterval"), Comp->RegenInterval);
                Message = FString::Printf(TEXT("Configured regen for '%s': %.2f per %.2fs"), *TrackerKey, Comp->RegenRate, Comp->RegenInterval);
            }
        }
    }
    
    else if (LowerSub == TEXT("pause_value_changes"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        FString TrackerKey;
        Payload->TryGetStringField(TEXT("trackerKey"), TrackerKey);
        bool bPause = true;
        Payload->TryGetBoolField(TEXT("paused"), bPause);
        
        AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
        if (!TargetActor)
        {
            bSuccess = false;
            Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
            ErrorCode = TEXT("ACTOR_NOT_FOUND");
        }
        else
        {
            UMcpValueTrackerComponent* Comp = McpGameplayPrimitivesHelpers::FindMcpComponent<UMcpValueTrackerComponent>(TargetActor, TrackerKey);
            if (!Comp)
            {
                bSuccess = false;
                Message = FString::Printf(TEXT("ValueTracker '%s' not found on actor '%s'"), *TrackerKey, *ActorName);
                ErrorCode = TEXT("TRACKER_NOT_FOUND");
            }
            else
            {
                Comp->SetPaused(bPause);
                
                Resp->SetStringField(TEXT("trackerKey"), TrackerKey);
                Resp->SetBoolField(TEXT("isPaused"), Comp->bIsPaused);
                Message = FString::Printf(TEXT("ValueTracker '%s' %s"), *TrackerKey, bPause ? TEXT("paused") : TEXT("resumed"));
            }
        }
    }

    // ==================== STATE MACHINE (6 actions) ====================
    else if (LowerSub == TEXT("create_actor_state_machine"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        FString InitialState = TEXT("Idle");
        Payload->TryGetStringField(TEXT("initialState"), InitialState);
        
        AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
        if (!TargetActor)
        {
            bSuccess = false;
            Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
            ErrorCode = TEXT("ACTOR_NOT_FOUND");
        }
        else
        {
            // Check if state machine already exists
            UMcpStateMachineComponent* ExistingComp = TargetActor->FindComponentByClass<UMcpStateMachineComponent>();
            if (ExistingComp)
            {
                bSuccess = false;
                Message = FString::Printf(TEXT("StateMachine already exists on '%s'"), *ActorName);
                ErrorCode = TEXT("STATE_MACHINE_EXISTS");
            }
            else
            {
                UMcpStateMachineComponent* Comp = NewObject<UMcpStateMachineComponent>(TargetActor, TEXT("StateMachine"));
                if (Comp)
                {
                    Comp->RegisterComponent();
                    Comp->AddState(InitialState, TEXT("{}"));
                    Comp->SetState(InitialState, true);
                    
                    Resp->SetStringField(TEXT("actorName"), ActorName);
                    Resp->SetStringField(TEXT("currentState"), Comp->CurrentState);
                    Resp->SetNumberField(TEXT("stateCount"), Comp->States.Num());
                    Message = FString::Printf(TEXT("Created StateMachine on '%s' with initial state '%s'"), *ActorName, *InitialState);
                }
                else
                {
                    bSuccess = false;
                    Message = TEXT("Failed to create StateMachineComponent");
                    ErrorCode = TEXT("CREATE_FAILED");
                }
            }
        }
    }
    
    else if (LowerSub == TEXT("add_actor_state"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        FString StateName;
        Payload->TryGetStringField(TEXT("stateName"), StateName);
        FString StateData = TEXT("{}");
        Payload->TryGetStringField(TEXT("stateData"), StateData);
        
        AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
        if (!TargetActor)
        {
            bSuccess = false;
            Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
            ErrorCode = TEXT("ACTOR_NOT_FOUND");
        }
        else
        {
            UMcpStateMachineComponent* Comp = TargetActor->FindComponentByClass<UMcpStateMachineComponent>();
            if (!Comp)
            {
                bSuccess = false;
                Message = FString::Printf(TEXT("StateMachine not found on actor '%s'"), *ActorName);
                ErrorCode = TEXT("STATE_MACHINE_NOT_FOUND");
            }
            else
            {
                if (Comp->HasState(StateName))
                {
                    bSuccess = false;
                    Message = FString::Printf(TEXT("State '%s' already exists"), *StateName);
                    ErrorCode = TEXT("STATE_EXISTS");
                }
                else
                {
                    Comp->AddState(StateName, StateData);
                    
                    Resp->SetStringField(TEXT("stateName"), StateName);
                    Resp->SetNumberField(TEXT("stateCount"), Comp->States.Num());
                    Message = FString::Printf(TEXT("Added state '%s' to StateMachine"), *StateName);
                }
            }
        }
    }
    
    else if (LowerSub == TEXT("add_actor_state_transition"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        FString FromState;
        Payload->TryGetStringField(TEXT("fromState"), FromState);
        FString ToState;
        Payload->TryGetStringField(TEXT("toState"), ToState);
        FString Conditions = TEXT("");
        Payload->TryGetStringField(TEXT("conditions"), Conditions);
        
        AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
        if (!TargetActor)
        {
            bSuccess = false;
            Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
            ErrorCode = TEXT("ACTOR_NOT_FOUND");
        }
        else
        {
            UMcpStateMachineComponent* Comp = TargetActor->FindComponentByClass<UMcpStateMachineComponent>();
            if (!Comp)
            {
                bSuccess = false;
                Message = FString::Printf(TEXT("StateMachine not found on actor '%s'"), *ActorName);
                ErrorCode = TEXT("STATE_MACHINE_NOT_FOUND");
            }
            else
            {
                Comp->AddTransition(FromState, ToState, Conditions);
                
                Resp->SetStringField(TEXT("fromState"), FromState);
                Resp->SetStringField(TEXT("toState"), ToState);
                Resp->SetNumberField(TEXT("transitionCount"), Comp->Transitions.Num());
                Message = FString::Printf(TEXT("Added transition '%s' -> '%s'"), *FromState, *ToState);
            }
        }
    }
    
    else if (LowerSub == TEXT("set_actor_state"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        FString NewState;
        Payload->TryGetStringField(TEXT("stateName"), NewState);
        bool bForce = false;
        Payload->TryGetBoolField(TEXT("force"), bForce);
        
        AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
        if (!TargetActor)
        {
            bSuccess = false;
            Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
            ErrorCode = TEXT("ACTOR_NOT_FOUND");
        }
        else
        {
            UMcpStateMachineComponent* Comp = TargetActor->FindComponentByClass<UMcpStateMachineComponent>();
            if (!Comp)
            {
                bSuccess = false;
                Message = FString::Printf(TEXT("StateMachine not found on actor '%s'"), *ActorName);
                ErrorCode = TEXT("STATE_MACHINE_NOT_FOUND");
            }
            else
            {
                FString OldState = Comp->CurrentState;
                bool bTransitioned = Comp->SetState(NewState, bForce);
                
                Resp->SetStringField(TEXT("previousState"), OldState);
                Resp->SetStringField(TEXT("currentState"), Comp->CurrentState);
                Resp->SetBoolField(TEXT("transitioned"), bTransitioned);
                
                if (bTransitioned)
                {
                    Message = FString::Printf(TEXT("State changed: '%s' -> '%s'"), *OldState, *NewState);
                }
                else
                {
                    bSuccess = false;
                    Message = FString::Printf(TEXT("Transition from '%s' to '%s' not valid"), *OldState, *NewState);
                    ErrorCode = TEXT("INVALID_TRANSITION");
                }
            }
        }
    }
    
    else if (LowerSub == TEXT("get_actor_state"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        
        AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
        if (!TargetActor)
        {
            bSuccess = false;
            Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
            ErrorCode = TEXT("ACTOR_NOT_FOUND");
        }
        else
        {
            UMcpStateMachineComponent* Comp = TargetActor->FindComponentByClass<UMcpStateMachineComponent>();
            if (!Comp)
            {
                bSuccess = false;
                Message = FString::Printf(TEXT("StateMachine not found on actor '%s'"), *ActorName);
                ErrorCode = TEXT("STATE_MACHINE_NOT_FOUND");
            }
            else
            {
                Resp->SetStringField(TEXT("currentState"), Comp->GetCurrentState());
                Resp->SetNumberField(TEXT("timeInState"), Comp->GetTimeInState());
                Resp->SetStringField(TEXT("stateData"), Comp->GetStateData(Comp->CurrentState));
                Resp->SetNumberField(TEXT("stateCount"), Comp->States.Num());
                
                TArray<TSharedPtr<FJsonValue>> AvailableTransitions;
                for (const FString& Trans : Comp->GetAvailableTransitions())
                {
                    AvailableTransitions.Add(MakeShared<FJsonValueString>(Trans));
                }
                Resp->SetArrayField(TEXT("availableTransitions"), AvailableTransitions);
                
                Message = FString::Printf(TEXT("State: '%s' (%.2fs)"), *Comp->CurrentState, Comp->GetTimeInState());
            }
        }
    }
    
    else if (LowerSub == TEXT("configure_state_timer"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        double Duration = 0.0;
        Payload->TryGetNumberField(TEXT("duration"), Duration);
        FString TargetState;
        Payload->TryGetStringField(TEXT("targetState"), TargetState);
        
        AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
        if (!TargetActor)
        {
            bSuccess = false;
            Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
            ErrorCode = TEXT("ACTOR_NOT_FOUND");
        }
        else
        {
            UMcpStateMachineComponent* Comp = TargetActor->FindComponentByClass<UMcpStateMachineComponent>();
            if (!Comp)
            {
                bSuccess = false;
                Message = FString::Printf(TEXT("StateMachine not found on actor '%s'"), *ActorName);
                ErrorCode = TEXT("STATE_MACHINE_NOT_FOUND");
            }
            else
            {
                Comp->ConfigureStateTimer(static_cast<float>(Duration), TargetState);
                
                Resp->SetNumberField(TEXT("duration"), Duration);
                Resp->SetStringField(TEXT("targetState"), TargetState);
                Resp->SetBoolField(TEXT("timerActive"), Comp->bTimerActive);
                Message = FString::Printf(TEXT("Timer set: transition to '%s' in %.2fs"), *TargetState, Duration);
            }
        }
    }

    // ==================== WORLD TIME (7 actions) ====================
    else if (LowerSub == TEXT("create_world_time"))
    {
        double InitialTime = 6.0;
        Payload->TryGetNumberField(TEXT("initialTime"), InitialTime);
        double DayLengthSeconds = 1200.0;
        Payload->TryGetNumberField(TEXT("dayLengthSeconds"), DayLengthSeconds);
        bool bStartPaused = false;
        Payload->TryGetBoolField(TEXT("startPaused"), bStartPaused);
        
        UMcpWorldTimeSubsystem* TimeSubsystem = World->GetSubsystem<UMcpWorldTimeSubsystem>();
        if (!TimeSubsystem)
        {
            bSuccess = false;
            Message = TEXT("WorldTimeSubsystem not available");
            ErrorCode = TEXT("SUBSYSTEM_NOT_FOUND");
        }
        else
        {
            bool bCreated = TimeSubsystem->CreateWorldTime(static_cast<float>(InitialTime), static_cast<float>(DayLengthSeconds), bStartPaused);
            
            Resp->SetBoolField(TEXT("created"), bCreated);
            Resp->SetNumberField(TEXT("currentTime"), TimeSubsystem->CurrentTime);
            Resp->SetNumberField(TEXT("dayLengthSeconds"), TimeSubsystem->DayLengthSeconds);
            Resp->SetBoolField(TEXT("isPaused"), TimeSubsystem->bIsPaused);
            Resp->SetNumberField(TEXT("currentDay"), TimeSubsystem->CurrentDay);
            
            FString PeriodStr;
            switch (TimeSubsystem->GetCurrentPeriod())
            {
                case EMcpTimePeriod::Dawn: PeriodStr = TEXT("Dawn"); break;
                case EMcpTimePeriod::Day: PeriodStr = TEXT("Day"); break;
                case EMcpTimePeriod::Dusk: PeriodStr = TEXT("Dusk"); break;
                case EMcpTimePeriod::Night: PeriodStr = TEXT("Night"); break;
            }
            Resp->SetStringField(TEXT("currentPeriod"), PeriodStr);
            Message = bCreated ? TEXT("WorldTime initialized") : TEXT("WorldTime already initialized");
        }
    }
    
    else if (LowerSub == TEXT("set_world_time"))
    {
        double NewTime = 12.0;
        Payload->TryGetNumberField(TEXT("time"), NewTime);
        
        UMcpWorldTimeSubsystem* TimeSubsystem = World->GetSubsystem<UMcpWorldTimeSubsystem>();
        if (!TimeSubsystem)
        {
            bSuccess = false;
            Message = TEXT("WorldTimeSubsystem not available");
            ErrorCode = TEXT("SUBSYSTEM_NOT_FOUND");
        }
        else
        {
            float OldTime = TimeSubsystem->CurrentTime;
            TimeSubsystem->SetWorldTime(static_cast<float>(NewTime));
            
            Resp->SetNumberField(TEXT("previousTime"), OldTime);
            Resp->SetNumberField(TEXT("currentTime"), TimeSubsystem->CurrentTime);
            Resp->SetNumberField(TEXT("hour"), TimeSubsystem->GetHour());
            Resp->SetNumberField(TEXT("minute"), TimeSubsystem->GetMinute());
            Message = FString::Printf(TEXT("Time set to %.2f (was %.2f)"), TimeSubsystem->CurrentTime, OldTime);
        }
    }
    
    else if (LowerSub == TEXT("get_world_time"))
    {
        UMcpWorldTimeSubsystem* TimeSubsystem = World->GetSubsystem<UMcpWorldTimeSubsystem>();
        if (!TimeSubsystem)
        {
            bSuccess = false;
            Message = TEXT("WorldTimeSubsystem not available");
            ErrorCode = TEXT("SUBSYSTEM_NOT_FOUND");
        }
        else
        {
            Resp->SetNumberField(TEXT("currentTime"), TimeSubsystem->CurrentTime);
            Resp->SetNumberField(TEXT("currentDay"), TimeSubsystem->CurrentDay);
            Resp->SetNumberField(TEXT("hour"), TimeSubsystem->GetHour());
            Resp->SetNumberField(TEXT("minute"), TimeSubsystem->GetMinute());
            Resp->SetNumberField(TEXT("timeScale"), TimeSubsystem->TimeScale);
            Resp->SetBoolField(TEXT("isPaused"), TimeSubsystem->bIsPaused);
            Resp->SetNumberField(TEXT("dayLengthSeconds"), TimeSubsystem->DayLengthSeconds);
            
            FString PeriodStr;
            switch (TimeSubsystem->GetCurrentPeriod())
            {
                case EMcpTimePeriod::Dawn: PeriodStr = TEXT("Dawn"); break;
                case EMcpTimePeriod::Day: PeriodStr = TEXT("Day"); break;
                case EMcpTimePeriod::Dusk: PeriodStr = TEXT("Dusk"); break;
                case EMcpTimePeriod::Night: PeriodStr = TEXT("Night"); break;
            }
            Resp->SetStringField(TEXT("currentPeriod"), PeriodStr);
            
            Message = FString::Printf(TEXT("Day %d, %02d:%02d (%s)"), TimeSubsystem->CurrentDay, TimeSubsystem->GetHour(), TimeSubsystem->GetMinute(), *PeriodStr);
        }
    }
    
    else if (LowerSub == TEXT("set_time_scale"))
    {
        double NewScale = 1.0;
        Payload->TryGetNumberField(TEXT("timeScale"), NewScale);
        
        UMcpWorldTimeSubsystem* TimeSubsystem = World->GetSubsystem<UMcpWorldTimeSubsystem>();
        if (!TimeSubsystem)
        {
            bSuccess = false;
            Message = TEXT("WorldTimeSubsystem not available");
            ErrorCode = TEXT("SUBSYSTEM_NOT_FOUND");
        }
        else
        {
            float OldScale = TimeSubsystem->TimeScale;
            TimeSubsystem->SetTimeScale(static_cast<float>(NewScale));
            
            Resp->SetNumberField(TEXT("previousScale"), OldScale);
            Resp->SetNumberField(TEXT("timeScale"), TimeSubsystem->TimeScale);
            Message = FString::Printf(TEXT("Time scale set to %.2fx (was %.2fx)"), TimeSubsystem->TimeScale, OldScale);
        }
    }
    
    else if (LowerSub == TEXT("pause_world_time"))
    {
        bool bPause = true;
        Payload->TryGetBoolField(TEXT("paused"), bPause);
        
        UMcpWorldTimeSubsystem* TimeSubsystem = World->GetSubsystem<UMcpWorldTimeSubsystem>();
        if (!TimeSubsystem)
        {
            bSuccess = false;
            Message = TEXT("WorldTimeSubsystem not available");
            ErrorCode = TEXT("SUBSYSTEM_NOT_FOUND");
        }
        else
        {
            TimeSubsystem->PauseWorldTime(bPause);
            
            Resp->SetBoolField(TEXT("isPaused"), TimeSubsystem->bIsPaused);
            Message = FString::Printf(TEXT("World time %s"), bPause ? TEXT("paused") : TEXT("resumed"));
        }
    }
    
    else if (LowerSub == TEXT("add_time_event"))
    {
        FString EventId;
        Payload->TryGetStringField(TEXT("eventId"), EventId);
        double TriggerTime = 0.0;
        Payload->TryGetNumberField(TEXT("triggerTime"), TriggerTime);
        bool bRecurring = false;
        Payload->TryGetBoolField(TEXT("recurring"), bRecurring);
        double Interval = 24.0;
        Payload->TryGetNumberField(TEXT("interval"), Interval);
        
        UMcpWorldTimeSubsystem* TimeSubsystem = World->GetSubsystem<UMcpWorldTimeSubsystem>();
        if (!TimeSubsystem)
        {
            bSuccess = false;
            Message = TEXT("WorldTimeSubsystem not available");
            ErrorCode = TEXT("SUBSYSTEM_NOT_FOUND");
        }
        else
        {
            bool bAdded = TimeSubsystem->AddTimeEvent(EventId, static_cast<float>(TriggerTime), bRecurring, static_cast<float>(Interval));
            
            Resp->SetStringField(TEXT("eventId"), EventId);
            Resp->SetNumberField(TEXT("triggerTime"), TriggerTime);
            Resp->SetBoolField(TEXT("recurring"), bRecurring);
            Resp->SetBoolField(TEXT("added"), bAdded);
            Resp->SetNumberField(TEXT("eventCount"), TimeSubsystem->TimeEvents.Num());
            
            if (bAdded)
            {
                Message = FString::Printf(TEXT("Added time event '%s' at %.2f hours"), *EventId, TriggerTime);
            }
            else
            {
                bSuccess = false;
                Message = FString::Printf(TEXT("Event '%s' already exists"), *EventId);
                ErrorCode = TEXT("EVENT_EXISTS");
            }
        }
    }
    
    else if (LowerSub == TEXT("get_time_period"))
    {
        UMcpWorldTimeSubsystem* TimeSubsystem = World->GetSubsystem<UMcpWorldTimeSubsystem>();
        if (!TimeSubsystem)
        {
            bSuccess = false;
            Message = TEXT("WorldTimeSubsystem not available");
            ErrorCode = TEXT("SUBSYSTEM_NOT_FOUND");
        }
        else
        {
            EMcpTimePeriod Period = TimeSubsystem->GetCurrentPeriod();
            
            FString PeriodStr;
            float StartBound, EndBound;
            switch (Period)
            {
                case EMcpTimePeriod::Dawn:
                    PeriodStr = TEXT("Dawn");
                    break;
                case EMcpTimePeriod::Day:
                    PeriodStr = TEXT("Day");
                    break;
                case EMcpTimePeriod::Dusk:
                    PeriodStr = TEXT("Dusk");
                    break;
                case EMcpTimePeriod::Night:
                    PeriodStr = TEXT("Night");
                    break;
            }
            TimeSubsystem->GetPeriodBounds(Period, StartBound, EndBound);
            
            Resp->SetStringField(TEXT("currentPeriod"), PeriodStr);
            Resp->SetNumberField(TEXT("periodStart"), StartBound);
            Resp->SetNumberField(TEXT("periodEnd"), EndBound);
            Resp->SetNumberField(TEXT("currentTime"), TimeSubsystem->CurrentTime);
            Message = FString::Printf(TEXT("Current period: %s (%.2f - %.2f)"), *PeriodStr, StartBound, EndBound);
        }
    }

    // ==================== ZONE (6 actions) ====================
    else if (LowerSub == TEXT("create_zone"))
    {
        FString ZoneId;
        Payload->TryGetStringField(TEXT("zoneId"), ZoneId);
        FString DisplayName;
        Payload->TryGetStringField(TEXT("displayName"), DisplayName);
        FString VolumeActorName;
        Payload->TryGetStringField(TEXT("volumeActorName"), VolumeActorName);
        
        UMcpZoneSubsystem* ZoneSubsystem = World->GetSubsystem<UMcpZoneSubsystem>();
        if (!ZoneSubsystem)
        {
            bSuccess = false;
            Message = TEXT("ZoneSubsystem not available");
            ErrorCode = TEXT("SUBSYSTEM_NOT_FOUND");
        }
        else
        {
            AActor* VolumeActor = nullptr;
            if (!VolumeActorName.IsEmpty())
            {
                VolumeActor = FindActorByLabelOrName<AActor>(VolumeActorName);
            }
            
            bool bCreated = ZoneSubsystem->CreateZone(ZoneId, DisplayName, VolumeActor);
            
            Resp->SetStringField(TEXT("zoneId"), ZoneId);
            Resp->SetStringField(TEXT("displayName"), DisplayName);
            Resp->SetBoolField(TEXT("hasVolume"), VolumeActor != nullptr);
            Resp->SetBoolField(TEXT("created"), bCreated);
            
            if (bCreated)
            {
                Message = FString::Printf(TEXT("Created zone '%s' (%s)"), *ZoneId, *DisplayName);
            }
            else
            {
                bSuccess = false;
                Message = FString::Printf(TEXT("Zone '%s' already exists"), *ZoneId);
                ErrorCode = TEXT("ZONE_EXISTS");
            }
        }
    }
    
    else if (LowerSub == TEXT("set_zone_property"))
    {
        FString ZoneId;
        Payload->TryGetStringField(TEXT("zoneId"), ZoneId);
        FString PropertyKey;
        Payload->TryGetStringField(TEXT("propertyKey"), PropertyKey);
        FString PropertyValue;
        Payload->TryGetStringField(TEXT("propertyValue"), PropertyValue);
        
        UMcpZoneSubsystem* ZoneSubsystem = World->GetSubsystem<UMcpZoneSubsystem>();
        if (!ZoneSubsystem)
        {
            bSuccess = false;
            Message = TEXT("ZoneSubsystem not available");
            ErrorCode = TEXT("SUBSYSTEM_NOT_FOUND");
        }
        else
        {
            bool bSet = ZoneSubsystem->SetZoneProperty(ZoneId, PropertyKey, PropertyValue);
            
            Resp->SetStringField(TEXT("zoneId"), ZoneId);
            Resp->SetStringField(TEXT("propertyKey"), PropertyKey);
            Resp->SetStringField(TEXT("propertyValue"), PropertyValue);
            Resp->SetBoolField(TEXT("set"), bSet);
            
            if (bSet)
            {
                Message = FString::Printf(TEXT("Set zone '%s' property '%s' = '%s'"), *ZoneId, *PropertyKey, *PropertyValue);
            }
            else
            {
                bSuccess = false;
                Message = FString::Printf(TEXT("Zone '%s' not found"), *ZoneId);
                ErrorCode = TEXT("ZONE_NOT_FOUND");
            }
        }
    }
    
    else if (LowerSub == TEXT("get_zone_property"))
    {
        FString ZoneId;
        Payload->TryGetStringField(TEXT("zoneId"), ZoneId);
        FString PropertyKey;
        Payload->TryGetStringField(TEXT("propertyKey"), PropertyKey);
        
        UMcpZoneSubsystem* ZoneSubsystem = World->GetSubsystem<UMcpZoneSubsystem>();
        if (!ZoneSubsystem)
        {
            bSuccess = false;
            Message = TEXT("ZoneSubsystem not available");
            ErrorCode = TEXT("SUBSYSTEM_NOT_FOUND");
        }
        else
        {
            FString PropertyValue;
            bool bFound = ZoneSubsystem->GetZoneProperty(ZoneId, PropertyKey, PropertyValue);
            
            Resp->SetStringField(TEXT("zoneId"), ZoneId);
            Resp->SetStringField(TEXT("propertyKey"), PropertyKey);
            Resp->SetBoolField(TEXT("found"), bFound);
            
            if (bFound)
            {
                Resp->SetStringField(TEXT("propertyValue"), PropertyValue);
                Message = FString::Printf(TEXT("Zone '%s' property '%s' = '%s'"), *ZoneId, *PropertyKey, *PropertyValue);
            }
            else
            {
                bSuccess = false;
                Message = FString::Printf(TEXT("Property '%s' not found in zone '%s'"), *PropertyKey, *ZoneId);
                ErrorCode = TEXT("PROPERTY_NOT_FOUND");
            }
        }
    }
    
    else if (LowerSub == TEXT("get_actor_zone"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        
        UMcpZoneSubsystem* ZoneSubsystem = World->GetSubsystem<UMcpZoneSubsystem>();
        if (!ZoneSubsystem)
        {
            bSuccess = false;
            Message = TEXT("ZoneSubsystem not available");
            ErrorCode = TEXT("SUBSYSTEM_NOT_FOUND");
        }
        else
        {
            AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
            if (!TargetActor)
            {
                bSuccess = false;
                Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
                ErrorCode = TEXT("ACTOR_NOT_FOUND");
            }
            else
            {
                FString ZoneId, ZoneName;
                bool bInZone = ZoneSubsystem->GetActorZone(TargetActor, ZoneId, ZoneName);
                
                Resp->SetStringField(TEXT("actorName"), ActorName);
                Resp->SetBoolField(TEXT("inZone"), bInZone);
                
                if (bInZone)
                {
                    Resp->SetStringField(TEXT("zoneId"), ZoneId);
                    Resp->SetStringField(TEXT("zoneName"), ZoneName);
                    Message = FString::Printf(TEXT("Actor '%s' is in zone '%s' (%s)"), *ActorName, *ZoneId, *ZoneName);
                }
                else
                {
                    Message = FString::Printf(TEXT("Actor '%s' is not in any zone"), *ActorName);
                }
            }
        }
    }
    
    else if (LowerSub == TEXT("add_zone_enter_event"))
    {
        FString ZoneId;
        Payload->TryGetStringField(TEXT("zoneId"), ZoneId);
        FString EventId;
        Payload->TryGetStringField(TEXT("eventId"), EventId);
        FString ConditionId;
        Payload->TryGetStringField(TEXT("conditionId"), ConditionId);
        
        UMcpZoneSubsystem* ZoneSubsystem = World->GetSubsystem<UMcpZoneSubsystem>();
        if (!ZoneSubsystem)
        {
            bSuccess = false;
            Message = TEXT("ZoneSubsystem not available");
            ErrorCode = TEXT("SUBSYSTEM_NOT_FOUND");
        }
        else
        {
            bool bAdded = ZoneSubsystem->AddZoneEnterEvent(ZoneId, EventId, ConditionId);
            
            Resp->SetStringField(TEXT("zoneId"), ZoneId);
            Resp->SetStringField(TEXT("eventId"), EventId);
            Resp->SetBoolField(TEXT("added"), bAdded);
            
            if (bAdded)
            {
                Message = FString::Printf(TEXT("Added enter event '%s' to zone '%s'"), *EventId, *ZoneId);
            }
            else
            {
                bSuccess = false;
                Message = FString::Printf(TEXT("Zone '%s' not found"), *ZoneId);
                ErrorCode = TEXT("ZONE_NOT_FOUND");
            }
        }
    }
    
    else if (LowerSub == TEXT("add_zone_exit_event"))
    {
        FString ZoneId;
        Payload->TryGetStringField(TEXT("zoneId"), ZoneId);
        FString EventId;
        Payload->TryGetStringField(TEXT("eventId"), EventId);
        FString ConditionId;
        Payload->TryGetStringField(TEXT("conditionId"), ConditionId);
        
        UMcpZoneSubsystem* ZoneSubsystem = World->GetSubsystem<UMcpZoneSubsystem>();
        if (!ZoneSubsystem)
        {
            bSuccess = false;
            Message = TEXT("ZoneSubsystem not available");
            ErrorCode = TEXT("SUBSYSTEM_NOT_FOUND");
        }
        else
        {
            bool bAdded = ZoneSubsystem->AddZoneExitEvent(ZoneId, EventId, ConditionId);
            
            Resp->SetStringField(TEXT("zoneId"), ZoneId);
            Resp->SetStringField(TEXT("eventId"), EventId);
            Resp->SetBoolField(TEXT("added"), bAdded);
            
            if (bAdded)
            {
                Message = FString::Printf(TEXT("Added exit event '%s' to zone '%s'"), *EventId, *ZoneId);
            }
            else
            {
                bSuccess = false;
                Message = FString::Printf(TEXT("Zone '%s' not found"), *ZoneId);
                ErrorCode = TEXT("ZONE_NOT_FOUND");
            }
        }
    }

    // ==================== FACTION (8 actions) ====================
    else if (LowerSub == TEXT("create_faction"))
    {
        FString FactionId;
        Payload->TryGetStringField(TEXT("factionId"), FactionId);
        FString DisplayName;
        Payload->TryGetStringField(TEXT("displayName"), DisplayName);
        
        FLinearColor Color = FLinearColor::White;
        const TSharedPtr<FJsonObject>* ColorObj;
        if (Payload->TryGetObjectField(TEXT("color"), ColorObj))
        {
            (*ColorObj)->TryGetNumberField(TEXT("r"), Color.R);
            (*ColorObj)->TryGetNumberField(TEXT("g"), Color.G);
            (*ColorObj)->TryGetNumberField(TEXT("b"), Color.B);
            double Alpha = 1.0;
            if ((*ColorObj)->TryGetNumberField(TEXT("a"), Alpha))
            {
                Color.A = static_cast<float>(Alpha);
            }
        }
        
        UMcpFactionSubsystem* FactionSubsystem = World->GetSubsystem<UMcpFactionSubsystem>();
        if (!FactionSubsystem)
        {
            bSuccess = false;
            Message = TEXT("FactionSubsystem not available");
            ErrorCode = TEXT("SUBSYSTEM_NOT_FOUND");
        }
        else
        {
            bool bCreated = FactionSubsystem->CreateFaction(FactionId, DisplayName, Color);
            
            Resp->SetStringField(TEXT("factionId"), FactionId);
            Resp->SetStringField(TEXT("displayName"), DisplayName);
            Resp->SetBoolField(TEXT("created"), bCreated);
            
            if (bCreated)
            {
                Message = FString::Printf(TEXT("Created faction '%s' (%s)"), *FactionId, *DisplayName);
            }
            else
            {
                bSuccess = false;
                Message = FString::Printf(TEXT("Faction '%s' already exists"), *FactionId);
                ErrorCode = TEXT("FACTION_EXISTS");
            }
        }
    }
    
    else if (LowerSub == TEXT("set_faction_relationship"))
    {
        FString FactionA;
        Payload->TryGetStringField(TEXT("factionA"), FactionA);
        FString FactionB;
        Payload->TryGetStringField(TEXT("factionB"), FactionB);
        FString RelationshipStr = TEXT("Neutral");
        Payload->TryGetStringField(TEXT("relationship"), RelationshipStr);
        bool bBidirectional = true;
        Payload->TryGetBoolField(TEXT("bidirectional"), bBidirectional);
        
        EMcpFactionRelationship Relationship = EMcpFactionRelationship::Neutral;
        if (RelationshipStr.Equals(TEXT("Friendly"), ESearchCase::IgnoreCase))
        {
            Relationship = EMcpFactionRelationship::Friendly;
        }
        else if (RelationshipStr.Equals(TEXT("Allied"), ESearchCase::IgnoreCase))
        {
            Relationship = EMcpFactionRelationship::Allied;
        }
        else if (RelationshipStr.Equals(TEXT("Hostile"), ESearchCase::IgnoreCase))
        {
            Relationship = EMcpFactionRelationship::Hostile;
        }
        else if (RelationshipStr.Equals(TEXT("Enemy"), ESearchCase::IgnoreCase))
        {
            Relationship = EMcpFactionRelationship::Enemy;
        }
        
        UMcpFactionSubsystem* FactionSubsystem = World->GetSubsystem<UMcpFactionSubsystem>();
        if (!FactionSubsystem)
        {
            bSuccess = false;
            Message = TEXT("FactionSubsystem not available");
            ErrorCode = TEXT("SUBSYSTEM_NOT_FOUND");
        }
        else
        {
            bool bSet = FactionSubsystem->SetFactionRelationship(FactionA, FactionB, Relationship, bBidirectional);
            
            Resp->SetStringField(TEXT("factionA"), FactionA);
            Resp->SetStringField(TEXT("factionB"), FactionB);
            Resp->SetStringField(TEXT("relationship"), RelationshipStr);
            Resp->SetBoolField(TEXT("bidirectional"), bBidirectional);
            Resp->SetBoolField(TEXT("set"), bSet);
            
            if (bSet)
            {
                Message = FString::Printf(TEXT("Set relationship: '%s' <-> '%s' = %s"), *FactionA, *FactionB, *RelationshipStr);
            }
            else
            {
                bSuccess = false;
                Message = TEXT("Failed to set faction relationship");
                ErrorCode = TEXT("SET_FAILED");
            }
        }
    }
    
    else if (LowerSub == TEXT("assign_to_faction"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        FString FactionId;
        Payload->TryGetStringField(TEXT("factionId"), FactionId);
        
        UMcpFactionSubsystem* FactionSubsystem = World->GetSubsystem<UMcpFactionSubsystem>();
        if (!FactionSubsystem)
        {
            bSuccess = false;
            Message = TEXT("FactionSubsystem not available");
            ErrorCode = TEXT("SUBSYSTEM_NOT_FOUND");
        }
        else
        {
            bool bAssigned = FactionSubsystem->AssignToFaction(ActorName, FactionId);
            
            Resp->SetStringField(TEXT("actorName"), ActorName);
            Resp->SetStringField(TEXT("factionId"), FactionId);
            Resp->SetBoolField(TEXT("assigned"), bAssigned);
            
            if (bAssigned)
            {
                Message = FString::Printf(TEXT("Assigned actor '%s' to faction '%s'"), *ActorName, *FactionId);
            }
            else
            {
                bSuccess = false;
                Message = FString::Printf(TEXT("Faction '%s' not found"), *FactionId);
                ErrorCode = TEXT("FACTION_NOT_FOUND");
            }
        }
    }
    
    else if (LowerSub == TEXT("get_faction"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        
        UMcpFactionSubsystem* FactionSubsystem = World->GetSubsystem<UMcpFactionSubsystem>();
        if (!FactionSubsystem)
        {
            bSuccess = false;
            Message = TEXT("FactionSubsystem not available");
            ErrorCode = TEXT("SUBSYSTEM_NOT_FOUND");
        }
        else
        {
            FString FactionId;
            FMcpFactionDefinition Faction;
            bool bFound = FactionSubsystem->GetFaction(ActorName, FactionId, Faction);
            
            Resp->SetStringField(TEXT("actorName"), ActorName);
            Resp->SetBoolField(TEXT("hasFaction"), bFound);
            
            if (bFound)
            {
                Resp->SetStringField(TEXT("factionId"), FactionId);
                Resp->SetStringField(TEXT("factionName"), Faction.DisplayName);
                
                TSharedPtr<FJsonObject> ColorObj = MakeShared<FJsonObject>();
                ColorObj->SetNumberField(TEXT("r"), Faction.Color.R);
                ColorObj->SetNumberField(TEXT("g"), Faction.Color.G);
                ColorObj->SetNumberField(TEXT("b"), Faction.Color.B);
                ColorObj->SetNumberField(TEXT("a"), Faction.Color.A);
                Resp->SetObjectField(TEXT("color"), ColorObj);
                
                Message = FString::Printf(TEXT("Actor '%s' belongs to faction '%s'"), *ActorName, *FactionId);
            }
            else
            {
                Message = FString::Printf(TEXT("Actor '%s' has no faction"), *ActorName);
            }
        }
    }
    
    else if (LowerSub == TEXT("modify_reputation"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        FString FactionId;
        Payload->TryGetStringField(TEXT("factionId"), FactionId);
        double Delta = 0.0;
        Payload->TryGetNumberField(TEXT("delta"), Delta);
        double MinRep = -100.0;
        Payload->TryGetNumberField(TEXT("minReputation"), MinRep);
        double MaxRep = 100.0;
        Payload->TryGetNumberField(TEXT("maxReputation"), MaxRep);
        
        UMcpFactionSubsystem* FactionSubsystem = World->GetSubsystem<UMcpFactionSubsystem>();
        if (!FactionSubsystem)
        {
            bSuccess = false;
            Message = TEXT("FactionSubsystem not available");
            ErrorCode = TEXT("SUBSYSTEM_NOT_FOUND");
        }
        else
        {
            float OldRep = 0.0f;
            FactionSubsystem->GetReputation(ActorName, FactionId, OldRep);
            
            bool bModified = FactionSubsystem->ModifyReputation(ActorName, FactionId, static_cast<float>(Delta), static_cast<float>(MinRep), static_cast<float>(MaxRep));
            
            float NewRep = 0.0f;
            FactionSubsystem->GetReputation(ActorName, FactionId, NewRep);
            
            Resp->SetStringField(TEXT("actorName"), ActorName);
            Resp->SetStringField(TEXT("factionId"), FactionId);
            Resp->SetNumberField(TEXT("previousReputation"), OldRep);
            Resp->SetNumberField(TEXT("currentReputation"), NewRep);
            Resp->SetNumberField(TEXT("delta"), Delta);
            Resp->SetBoolField(TEXT("modified"), bModified);
            
            if (bModified)
            {
                Message = FString::Printf(TEXT("Reputation with '%s': %.2f -> %.2f (delta: %.2f)"), *FactionId, OldRep, NewRep, Delta);
            }
            else
            {
                bSuccess = false;
                Message = TEXT("Failed to modify reputation");
                ErrorCode = TEXT("MODIFY_FAILED");
            }
        }
    }
    
    else if (LowerSub == TEXT("get_reputation"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        FString FactionId;
        Payload->TryGetStringField(TEXT("factionId"), FactionId);
        
        UMcpFactionSubsystem* FactionSubsystem = World->GetSubsystem<UMcpFactionSubsystem>();
        if (!FactionSubsystem)
        {
            bSuccess = false;
            Message = TEXT("FactionSubsystem not available");
            ErrorCode = TEXT("SUBSYSTEM_NOT_FOUND");
        }
        else
        {
            float Reputation = 0.0f;
            bool bFound = FactionSubsystem->GetReputation(ActorName, FactionId, Reputation);
            
            Resp->SetStringField(TEXT("actorName"), ActorName);
            Resp->SetStringField(TEXT("factionId"), FactionId);
            Resp->SetBoolField(TEXT("found"), bFound);
            Resp->SetNumberField(TEXT("reputation"), Reputation);
            
            Message = FString::Printf(TEXT("Actor '%s' reputation with '%s': %.2f"), *ActorName, *FactionId, Reputation);
        }
    }
    
    else if (LowerSub == TEXT("add_reputation_threshold"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        FString FactionId;
        Payload->TryGetStringField(TEXT("factionId"), FactionId);
        double ThresholdValue = 0.0;
        Payload->TryGetNumberField(TEXT("thresholdValue"), ThresholdValue);
        FString Direction = TEXT("above");
        Payload->TryGetStringField(TEXT("direction"), Direction);
        FString EventId;
        Payload->TryGetStringField(TEXT("eventId"), EventId);
        
        UMcpFactionSubsystem* FactionSubsystem = World->GetSubsystem<UMcpFactionSubsystem>();
        if (!FactionSubsystem)
        {
            bSuccess = false;
            Message = TEXT("FactionSubsystem not available");
            ErrorCode = TEXT("SUBSYSTEM_NOT_FOUND");
        }
        else
        {
            bool bAdded = FactionSubsystem->AddReputationThreshold(ActorName, FactionId, static_cast<float>(ThresholdValue), Direction, EventId);
            
            Resp->SetStringField(TEXT("actorName"), ActorName);
            Resp->SetStringField(TEXT("factionId"), FactionId);
            Resp->SetNumberField(TEXT("thresholdValue"), ThresholdValue);
            Resp->SetStringField(TEXT("direction"), Direction);
            Resp->SetStringField(TEXT("eventId"), EventId);
            Resp->SetBoolField(TEXT("added"), bAdded);
            
            if (bAdded)
            {
                Message = FString::Printf(TEXT("Added reputation threshold %.2f (%s) for '%s' with '%s'"), ThresholdValue, *Direction, *ActorName, *FactionId);
            }
            else
            {
                bSuccess = false;
                Message = TEXT("Failed to add reputation threshold");
                ErrorCode = TEXT("ADD_FAILED");
            }
        }
    }
    
    else if (LowerSub == TEXT("check_faction_relationship"))
    {
        FString ActorA;
        Payload->TryGetStringField(TEXT("actorA"), ActorA);
        FString ActorB;
        Payload->TryGetStringField(TEXT("actorB"), ActorB);
        
        UMcpFactionSubsystem* FactionSubsystem = World->GetSubsystem<UMcpFactionSubsystem>();
        if (!FactionSubsystem)
        {
            bSuccess = false;
            Message = TEXT("FactionSubsystem not available");
            ErrorCode = TEXT("SUBSYSTEM_NOT_FOUND");
        }
        else
        {
            EMcpFactionRelationship Relationship;
            bool bIsFriendly, bIsHostile;
            bool bFound = FactionSubsystem->CheckFactionRelationship(ActorA, ActorB, Relationship, bIsFriendly, bIsHostile);
            
            FString RelStr;
            switch (Relationship)
            {
                case EMcpFactionRelationship::Neutral: RelStr = TEXT("Neutral"); break;
                case EMcpFactionRelationship::Friendly: RelStr = TEXT("Friendly"); break;
                case EMcpFactionRelationship::Allied: RelStr = TEXT("Allied"); break;
                case EMcpFactionRelationship::Hostile: RelStr = TEXT("Hostile"); break;
                case EMcpFactionRelationship::Enemy: RelStr = TEXT("Enemy"); break;
            }
            
            Resp->SetStringField(TEXT("actorA"), ActorA);
            Resp->SetStringField(TEXT("actorB"), ActorB);
            Resp->SetBoolField(TEXT("found"), bFound);
            Resp->SetStringField(TEXT("relationship"), RelStr);
            Resp->SetBoolField(TEXT("isFriendly"), bIsFriendly);
            Resp->SetBoolField(TEXT("isHostile"), bIsHostile);
            
            Message = FString::Printf(TEXT("Relationship between '%s' and '%s': %s"), *ActorA, *ActorB, *RelStr);
        }
    }

    // ==================== CONDITION (4 actions) ====================
    else if (LowerSub == TEXT("create_condition"))
    {
        FString ConditionId;
        Payload->TryGetStringField(TEXT("conditionId"), ConditionId);
        FString PredicateJson;
        Payload->TryGetStringField(TEXT("predicateJson"), PredicateJson);
        
        UMcpConditionSubsystem* ConditionSubsystem = World->GetSubsystem<UMcpConditionSubsystem>();
        if (!ConditionSubsystem)
        {
            bSuccess = false;
            Message = TEXT("ConditionSubsystem not available");
            ErrorCode = TEXT("SUBSYSTEM_NOT_FOUND");
        }
        else
        {
            bool bCreated = ConditionSubsystem->CreateCondition(ConditionId, PredicateJson);
            
            Resp->SetStringField(TEXT("conditionId"), ConditionId);
            Resp->SetBoolField(TEXT("created"), bCreated);
            Resp->SetNumberField(TEXT("conditionCount"), ConditionSubsystem->Conditions.Num());
            
            if (bCreated)
            {
                Message = FString::Printf(TEXT("Created condition '%s'"), *ConditionId);
            }
            else
            {
                bSuccess = false;
                Message = FString::Printf(TEXT("Condition '%s' already exists or invalid predicate"), *ConditionId);
                ErrorCode = TEXT("CREATE_FAILED");
            }
        }
    }
    
    else if (LowerSub == TEXT("create_compound_condition"))
    {
        FString ConditionId;
        Payload->TryGetStringField(TEXT("conditionId"), ConditionId);
        FString Operator = TEXT("all");
        Payload->TryGetStringField(TEXT("operator"), Operator);
        
        TArray<FString> ConditionIds;
        const TArray<TSharedPtr<FJsonValue>>* IdsArray;
        if (Payload->TryGetArrayField(TEXT("conditionIds"), IdsArray))
        {
            for (const TSharedPtr<FJsonValue>& Val : *IdsArray)
            {
                ConditionIds.Add(Val->AsString());
            }
        }
        
        UMcpConditionSubsystem* ConditionSubsystem = World->GetSubsystem<UMcpConditionSubsystem>();
        if (!ConditionSubsystem)
        {
            bSuccess = false;
            Message = TEXT("ConditionSubsystem not available");
            ErrorCode = TEXT("SUBSYSTEM_NOT_FOUND");
        }
        else
        {
            bool bCreated = ConditionSubsystem->CreateCompoundCondition(ConditionId, Operator, ConditionIds);
            
            Resp->SetStringField(TEXT("conditionId"), ConditionId);
            Resp->SetStringField(TEXT("operator"), Operator);
            Resp->SetNumberField(TEXT("childCount"), ConditionIds.Num());
            Resp->SetBoolField(TEXT("created"), bCreated);
            
            if (bCreated)
            {
                Message = FString::Printf(TEXT("Created compound condition '%s' (%s of %d conditions)"), *ConditionId, *Operator, ConditionIds.Num());
            }
            else
            {
                bSuccess = false;
                Message = FString::Printf(TEXT("Failed to create compound condition '%s'"), *ConditionId);
                ErrorCode = TEXT("CREATE_FAILED");
            }
        }
    }
    
    else if (LowerSub == TEXT("evaluate_condition"))
    {
        FString ConditionId;
        Payload->TryGetStringField(TEXT("conditionId"), ConditionId);
        
        UMcpConditionSubsystem* ConditionSubsystem = World->GetSubsystem<UMcpConditionSubsystem>();
        if (!ConditionSubsystem)
        {
            bSuccess = false;
            Message = TEXT("ConditionSubsystem not available");
            ErrorCode = TEXT("SUBSYSTEM_NOT_FOUND");
        }
        else
        {
            bool bResult = false;
            bool bFound = ConditionSubsystem->EvaluateCondition(ConditionId, bResult);
            
            Resp->SetStringField(TEXT("conditionId"), ConditionId);
            Resp->SetBoolField(TEXT("found"), bFound);
            Resp->SetBoolField(TEXT("result"), bResult);
            
            if (bFound)
            {
                Message = FString::Printf(TEXT("Condition '%s' evaluated to %s"), *ConditionId, bResult ? TEXT("TRUE") : TEXT("FALSE"));
            }
            else
            {
                bSuccess = false;
                Message = FString::Printf(TEXT("Condition '%s' not found"), *ConditionId);
                ErrorCode = TEXT("CONDITION_NOT_FOUND");
            }
        }
    }
    
    else if (LowerSub == TEXT("add_condition_listener"))
    {
        FString ConditionId;
        Payload->TryGetStringField(TEXT("conditionId"), ConditionId);
        FString ListenerId;
        Payload->TryGetStringField(TEXT("listenerId"), ListenerId);
        bool bOneShot = false;
        Payload->TryGetBoolField(TEXT("oneShot"), bOneShot);
        
        UMcpConditionSubsystem* ConditionSubsystem = World->GetSubsystem<UMcpConditionSubsystem>();
        if (!ConditionSubsystem)
        {
            bSuccess = false;
            Message = TEXT("ConditionSubsystem not available");
            ErrorCode = TEXT("SUBSYSTEM_NOT_FOUND");
        }
        else
        {
            bool bAdded = ConditionSubsystem->AddConditionListener(ConditionId, ListenerId, bOneShot);
            
            Resp->SetStringField(TEXT("conditionId"), ConditionId);
            Resp->SetStringField(TEXT("listenerId"), ListenerId);
            Resp->SetBoolField(TEXT("oneShot"), bOneShot);
            Resp->SetBoolField(TEXT("added"), bAdded);
            Resp->SetNumberField(TEXT("listenerCount"), ConditionSubsystem->Listeners.Num());
            
            if (bAdded)
            {
                Message = FString::Printf(TEXT("Added listener '%s' for condition '%s'"), *ListenerId, *ConditionId);
            }
            else
            {
                bSuccess = false;
                Message = FString::Printf(TEXT("Condition '%s' not found"), *ConditionId);
                ErrorCode = TEXT("CONDITION_NOT_FOUND");
            }
        }
    }

    // ==================== INTERACTION (6 actions) ====================
    else if (LowerSub == TEXT("add_interactable_component"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        FString InteractionType = TEXT("use");
        Payload->TryGetStringField(TEXT("interactionType"), InteractionType);
        FString InteractionPrompt = TEXT("Press E to interact");
        Payload->TryGetStringField(TEXT("interactionPrompt"), InteractionPrompt);
        double InteractionRange = 200.0;
        Payload->TryGetNumberField(TEXT("interactionRange"), InteractionRange);
        double InteractionPriority = 0;
        Payload->TryGetNumberField(TEXT("interactionPriority"), InteractionPriority);
        
        AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
        if (!TargetActor)
        {
            bSuccess = false;
            Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
            ErrorCode = TEXT("ACTOR_NOT_FOUND");
        }
        else
        {
            UMcpInteractableComponent* ExistingComp = TargetActor->FindComponentByClass<UMcpInteractableComponent>();
            if (ExistingComp)
            {
                bSuccess = false;
                Message = FString::Printf(TEXT("InteractableComponent already exists on '%s'"), *ActorName);
                ErrorCode = TEXT("COMPONENT_EXISTS");
            }
            else
            {
                UMcpInteractableComponent* Comp = NewObject<UMcpInteractableComponent>(TargetActor, TEXT("InteractableComponent"));
                if (Comp)
                {
                    Comp->RegisterComponent();
                    Comp->ConfigureInteraction(InteractionType, InteractionPrompt, static_cast<float>(InteractionRange), static_cast<int32>(InteractionPriority));
                    
                    Resp->SetStringField(TEXT("actorName"), ActorName);
                    Resp->SetStringField(TEXT("interactionType"), InteractionType);
                    Resp->SetStringField(TEXT("interactionPrompt"), InteractionPrompt);
                    Resp->SetNumberField(TEXT("interactionRange"), InteractionRange);
                    Resp->SetNumberField(TEXT("interactionPriority"), InteractionPriority);
                    Message = FString::Printf(TEXT("Added InteractableComponent to '%s'"), *ActorName);
                }
                else
                {
                    bSuccess = false;
                    Message = TEXT("Failed to create InteractableComponent");
                    ErrorCode = TEXT("CREATE_FAILED");
                }
            }
        }
    }
    
    else if (LowerSub == TEXT("configure_interaction"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        FString InteractionType;
        Payload->TryGetStringField(TEXT("interactionType"), InteractionType);
        FString InteractionPrompt;
        Payload->TryGetStringField(TEXT("interactionPrompt"), InteractionPrompt);
        double InteractionRange = 0.0;
        Payload->TryGetNumberField(TEXT("interactionRange"), InteractionRange);
        double InteractionPriority = 0;
        Payload->TryGetNumberField(TEXT("interactionPriority"), InteractionPriority);
        
        AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
        if (!TargetActor)
        {
            bSuccess = false;
            Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
            ErrorCode = TEXT("ACTOR_NOT_FOUND");
        }
        else
        {
            UMcpInteractableComponent* Comp = TargetActor->FindComponentByClass<UMcpInteractableComponent>();
            if (!Comp)
            {
                bSuccess = false;
                Message = FString::Printf(TEXT("InteractableComponent not found on '%s'"), *ActorName);
                ErrorCode = TEXT("COMPONENT_NOT_FOUND");
            }
            else
            {
                // Only update provided fields
                FString Type = InteractionType.IsEmpty() ? Comp->InteractionType : InteractionType;
                FString Prompt = InteractionPrompt.IsEmpty() ? Comp->InteractionPrompt : InteractionPrompt;
                float Range = InteractionRange > 0 ? static_cast<float>(InteractionRange) : Comp->InteractionRange;
                int32 Priority = static_cast<int32>(InteractionPriority);
                
                Comp->ConfigureInteraction(Type, Prompt, Range, Priority);
                
                Resp->SetStringField(TEXT("actorName"), ActorName);
                Resp->SetStringField(TEXT("interactionType"), Comp->InteractionType);
                Resp->SetStringField(TEXT("interactionPrompt"), Comp->InteractionPrompt);
                Resp->SetNumberField(TEXT("interactionRange"), Comp->InteractionRange);
                Resp->SetNumberField(TEXT("interactionPriority"), Comp->InteractionPriority);
                Message = FString::Printf(TEXT("Configured interaction on '%s'"), *ActorName);
            }
        }
    }
    
    else if (LowerSub == TEXT("set_interaction_enabled"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        bool bEnabled = true;
        Payload->TryGetBoolField(TEXT("enabled"), bEnabled);
        
        AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
        if (!TargetActor)
        {
            bSuccess = false;
            Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
            ErrorCode = TEXT("ACTOR_NOT_FOUND");
        }
        else
        {
            UMcpInteractableComponent* Comp = TargetActor->FindComponentByClass<UMcpInteractableComponent>();
            if (!Comp)
            {
                bSuccess = false;
                Message = FString::Printf(TEXT("InteractableComponent not found on '%s'"), *ActorName);
                ErrorCode = TEXT("COMPONENT_NOT_FOUND");
            }
            else
            {
                Comp->SetEnabled(bEnabled);
                
                Resp->SetStringField(TEXT("actorName"), ActorName);
                Resp->SetBoolField(TEXT("enabled"), Comp->bIsEnabled);
                Message = FString::Printf(TEXT("Interaction on '%s' %s"), *ActorName, bEnabled ? TEXT("enabled") : TEXT("disabled"));
            }
        }
    }
    
    else if (LowerSub == TEXT("get_nearby_interactables"))
    {
        FVector Location = FVector::ZeroVector;
        const TSharedPtr<FJsonObject>* LocObj;
        if (Payload->TryGetObjectField(TEXT("location"), LocObj))
        {
            (*LocObj)->TryGetNumberField(TEXT("x"), Location.X);
            (*LocObj)->TryGetNumberField(TEXT("y"), Location.Y);
            (*LocObj)->TryGetNumberField(TEXT("z"), Location.Z);
        }
        double Radius = 500.0;
        Payload->TryGetNumberField(TEXT("radius"), Radius);
        
        TArray<UMcpInteractableComponent*> Interactables = UMcpInteractableComponent::GetNearbyInteractables(World, Location, static_cast<float>(Radius));
        
        TArray<TSharedPtr<FJsonValue>> InteractablesArray;
        for (UMcpInteractableComponent* Comp : Interactables)
        {
            if (Comp && Comp->GetOwner())
            {
                TSharedPtr<FJsonObject> IntObj = MakeShared<FJsonObject>();
                IntObj->SetStringField(TEXT("actorName"), Comp->GetOwner()->GetActorLabel());
                IntObj->SetStringField(TEXT("interactionType"), Comp->InteractionType);
                IntObj->SetStringField(TEXT("interactionPrompt"), Comp->InteractionPrompt);
                IntObj->SetNumberField(TEXT("interactionRange"), Comp->InteractionRange);
                IntObj->SetNumberField(TEXT("priority"), Comp->InteractionPriority);
                IntObj->SetBoolField(TEXT("enabled"), Comp->bIsEnabled);
                IntObj->SetBoolField(TEXT("focused"), Comp->bIsFocused);
                
                FVector ActorLoc = Comp->GetOwner()->GetActorLocation();
                IntObj->SetNumberField(TEXT("distance"), FVector::Dist(Location, ActorLoc));
                
                InteractablesArray.Add(MakeShared<FJsonValueObject>(IntObj));
            }
        }
        
        Resp->SetArrayField(TEXT("interactables"), InteractablesArray);
        Resp->SetNumberField(TEXT("count"), InteractablesArray.Num());
        Resp->SetNumberField(TEXT("radius"), Radius);
        Message = FString::Printf(TEXT("Found %d interactables within %.0f units"), InteractablesArray.Num(), Radius);
    }
    
    else if (LowerSub == TEXT("focus_interaction"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        bool bFocused = true;
        Payload->TryGetBoolField(TEXT("focused"), bFocused);
        FString FocusingActorName;
        Payload->TryGetStringField(TEXT("focusingActorName"), FocusingActorName);
        
        AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
        if (!TargetActor)
        {
            bSuccess = false;
            Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
            ErrorCode = TEXT("ACTOR_NOT_FOUND");
        }
        else
        {
            UMcpInteractableComponent* Comp = TargetActor->FindComponentByClass<UMcpInteractableComponent>();
            if (!Comp)
            {
                bSuccess = false;
                Message = FString::Printf(TEXT("InteractableComponent not found on '%s'"), *ActorName);
                ErrorCode = TEXT("COMPONENT_NOT_FOUND");
            }
            else
            {
                AActor* FocusingActor = nullptr;
                if (!FocusingActorName.IsEmpty())
                {
                    FocusingActor = FindActorByLabelOrName<AActor>(FocusingActorName);
                }
                
                Comp->SetFocused(bFocused, FocusingActor);
                
                Resp->SetStringField(TEXT("actorName"), ActorName);
                Resp->SetBoolField(TEXT("focused"), Comp->bIsFocused);
                Message = FString::Printf(TEXT("Interaction on '%s' %s"), *ActorName, bFocused ? TEXT("focused") : TEXT("unfocused"));
            }
        }
    }
    
    else if (LowerSub == TEXT("execute_interaction"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        FString InteractingActorName;
        Payload->TryGetStringField(TEXT("interactingActorName"), InteractingActorName);
        
        AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
        if (!TargetActor)
        {
            bSuccess = false;
            Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
            ErrorCode = TEXT("ACTOR_NOT_FOUND");
        }
        else
        {
            UMcpInteractableComponent* Comp = TargetActor->FindComponentByClass<UMcpInteractableComponent>();
            if (!Comp)
            {
                bSuccess = false;
                Message = FString::Printf(TEXT("InteractableComponent not found on '%s'"), *ActorName);
                ErrorCode = TEXT("COMPONENT_NOT_FOUND");
            }
            else
            {
                AActor* InteractingActor = nullptr;
                if (!InteractingActorName.IsEmpty())
                {
                    InteractingActor = FindActorByLabelOrName<AActor>(InteractingActorName);
                }
                
                bool bExecuted = Comp->ExecuteInteraction(InteractingActor);
                
                Resp->SetStringField(TEXT("actorName"), ActorName);
                Resp->SetStringField(TEXT("interactionType"), Comp->InteractionType);
                Resp->SetBoolField(TEXT("executed"), bExecuted);
                
                if (bExecuted)
                {
                    Message = FString::Printf(TEXT("Executed '%s' interaction on '%s'"), *Comp->InteractionType, *ActorName);
                }
                else
                {
                    bSuccess = false;
                    Message = FString::Printf(TEXT("Interaction on '%s' is disabled or out of range"), *ActorName);
                    ErrorCode = TEXT("INTERACTION_FAILED");
                }
            }
        }
    }

    // ==================== SCHEDULE (5 actions) ====================
    else if (LowerSub == TEXT("create_schedule"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        FString ScheduleId;
        Payload->TryGetStringField(TEXT("scheduleId"), ScheduleId);
        bool bLooping = true;
        Payload->TryGetBoolField(TEXT("looping"), bLooping);
        
        AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
        if (!TargetActor)
        {
            bSuccess = false;
            Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
            ErrorCode = TEXT("ACTOR_NOT_FOUND");
        }
        else
        {
            UMcpScheduleComponent* ExistingComp = McpGameplayPrimitivesHelpers::FindMcpComponent<UMcpScheduleComponent>(TargetActor, ScheduleId);
            if (ExistingComp)
            {
                bSuccess = false;
                Message = FString::Printf(TEXT("Schedule '%s' already exists on '%s'"), *ScheduleId, *ActorName);
                ErrorCode = TEXT("SCHEDULE_EXISTS");
            }
            else
            {
                UMcpScheduleComponent* Comp = NewObject<UMcpScheduleComponent>(TargetActor, *FString::Printf(TEXT("Schedule_%s"), *ScheduleId));
                if (Comp)
                {
                    Comp->RegisterComponent();
                    Comp->ScheduleId = ScheduleId;
                    Comp->bLooping = bLooping;
                    
                    Resp->SetStringField(TEXT("actorName"), ActorName);
                    Resp->SetStringField(TEXT("scheduleId"), ScheduleId);
                    Resp->SetBoolField(TEXT("looping"), bLooping);
                    Resp->SetBoolField(TEXT("active"), Comp->bScheduleActive);
                    Message = FString::Printf(TEXT("Created schedule '%s' on '%s'"), *ScheduleId, *ActorName);
                }
                else
                {
                    bSuccess = false;
                    Message = TEXT("Failed to create ScheduleComponent");
                    ErrorCode = TEXT("CREATE_FAILED");
                }
            }
        }
    }
    
    else if (LowerSub == TEXT("add_schedule_entry"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        FString ScheduleId;
        Payload->TryGetStringField(TEXT("scheduleId"), ScheduleId);
        FString EntryId;
        Payload->TryGetStringField(TEXT("entryId"), EntryId);
        double StartHour = 0.0;
        Payload->TryGetNumberField(TEXT("startHour"), StartHour);
        double EndHour = 24.0;
        Payload->TryGetNumberField(TEXT("endHour"), EndHour);
        FString Activity;
        Payload->TryGetStringField(TEXT("activity"), Activity);
        FString ActivityData = TEXT("{}");
        Payload->TryGetStringField(TEXT("activityData"), ActivityData);
        
        TArray<int32> ActiveDays;
        const TArray<TSharedPtr<FJsonValue>>* DaysArray;
        if (Payload->TryGetArrayField(TEXT("activeDays"), DaysArray))
        {
            for (const TSharedPtr<FJsonValue>& Val : *DaysArray)
            {
                ActiveDays.Add(static_cast<int32>(Val->AsNumber()));
            }
        }
        
        AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
        if (!TargetActor)
        {
            bSuccess = false;
            Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
            ErrorCode = TEXT("ACTOR_NOT_FOUND");
        }
        else
        {
            UMcpScheduleComponent* Comp = McpGameplayPrimitivesHelpers::FindMcpComponent<UMcpScheduleComponent>(TargetActor, ScheduleId);
            if (!Comp)
            {
                bSuccess = false;
                Message = FString::Printf(TEXT("Schedule '%s' not found on '%s'"), *ScheduleId, *ActorName);
                ErrorCode = TEXT("SCHEDULE_NOT_FOUND");
            }
            else
            {
                Comp->AddEntry(EntryId, static_cast<float>(StartHour), static_cast<float>(EndHour), Activity, ActivityData, ActiveDays);
                
                Resp->SetStringField(TEXT("scheduleId"), ScheduleId);
                Resp->SetStringField(TEXT("entryId"), EntryId);
                Resp->SetNumberField(TEXT("startHour"), StartHour);
                Resp->SetNumberField(TEXT("endHour"), EndHour);
                Resp->SetStringField(TEXT("activity"), Activity);
                Resp->SetNumberField(TEXT("entryCount"), Comp->Entries.Num());
                Message = FString::Printf(TEXT("Added entry '%s' (%.0f:00 - %.0f:00) to schedule '%s'"), *EntryId, StartHour, EndHour, *ScheduleId);
            }
        }
    }
    
    else if (LowerSub == TEXT("set_schedule_active"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        FString ScheduleId;
        Payload->TryGetStringField(TEXT("scheduleId"), ScheduleId);
        bool bActive = true;
        Payload->TryGetBoolField(TEXT("active"), bActive);
        
        AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
        if (!TargetActor)
        {
            bSuccess = false;
            Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
            ErrorCode = TEXT("ACTOR_NOT_FOUND");
        }
        else
        {
            UMcpScheduleComponent* Comp = McpGameplayPrimitivesHelpers::FindMcpComponent<UMcpScheduleComponent>(TargetActor, ScheduleId);
            if (!Comp)
            {
                bSuccess = false;
                Message = FString::Printf(TEXT("Schedule '%s' not found on '%s'"), *ScheduleId, *ActorName);
                ErrorCode = TEXT("SCHEDULE_NOT_FOUND");
            }
            else
            {
                Comp->SetScheduleActive(bActive);
                
                Resp->SetStringField(TEXT("scheduleId"), ScheduleId);
                Resp->SetBoolField(TEXT("active"), Comp->IsScheduleActive());
                Message = FString::Printf(TEXT("Schedule '%s' %s"), *ScheduleId, bActive ? TEXT("activated") : TEXT("deactivated"));
            }
        }
    }
    
    else if (LowerSub == TEXT("get_current_schedule_entry"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        FString ScheduleId;
        Payload->TryGetStringField(TEXT("scheduleId"), ScheduleId);
        
        AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
        if (!TargetActor)
        {
            bSuccess = false;
            Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
            ErrorCode = TEXT("ACTOR_NOT_FOUND");
        }
        else
        {
            UMcpScheduleComponent* Comp = McpGameplayPrimitivesHelpers::FindMcpComponent<UMcpScheduleComponent>(TargetActor, ScheduleId);
            if (!Comp)
            {
                bSuccess = false;
                Message = FString::Printf(TEXT("Schedule '%s' not found on '%s'"), *ScheduleId, *ActorName);
                ErrorCode = TEXT("SCHEDULE_NOT_FOUND");
            }
            else
            {
                FMcpScheduleEntry Entry = Comp->GetCurrentEntry();
                
                Resp->SetStringField(TEXT("scheduleId"), ScheduleId);
                Resp->SetStringField(TEXT("currentEntryId"), Entry.EntryId);
                Resp->SetStringField(TEXT("activity"), Entry.ActivityName);
                Resp->SetNumberField(TEXT("startHour"), Entry.StartHour);
                Resp->SetNumberField(TEXT("endHour"), Entry.EndHour);
                Resp->SetStringField(TEXT("activityData"), Entry.ActivityData);
                Resp->SetBoolField(TEXT("active"), Comp->IsScheduleActive());
                
                Message = FString::Printf(TEXT("Current activity: '%s' (%s)"), *Entry.ActivityName, *Entry.EntryId);
            }
        }
    }
    
    else if (LowerSub == TEXT("skip_to_schedule_entry"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        FString ScheduleId;
        Payload->TryGetStringField(TEXT("scheduleId"), ScheduleId);
        FString EntryId;
        Payload->TryGetStringField(TEXT("entryId"), EntryId);
        
        AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
        if (!TargetActor)
        {
            bSuccess = false;
            Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
            ErrorCode = TEXT("ACTOR_NOT_FOUND");
        }
        else
        {
            UMcpScheduleComponent* Comp = McpGameplayPrimitivesHelpers::FindMcpComponent<UMcpScheduleComponent>(TargetActor, ScheduleId);
            if (!Comp)
            {
                bSuccess = false;
                Message = FString::Printf(TEXT("Schedule '%s' not found on '%s'"), *ScheduleId, *ActorName);
                ErrorCode = TEXT("SCHEDULE_NOT_FOUND");
            }
            else
            {
                if (!Comp->HasEntry(EntryId))
                {
                    bSuccess = false;
                    Message = FString::Printf(TEXT("Entry '%s' not found in schedule '%s'"), *EntryId, *ScheduleId);
                    ErrorCode = TEXT("ENTRY_NOT_FOUND");
                }
                else
                {
                    Comp->SkipToEntry(EntryId);
                    
                    Resp->SetStringField(TEXT("scheduleId"), ScheduleId);
                    Resp->SetStringField(TEXT("entryId"), EntryId);
                    Resp->SetStringField(TEXT("currentEntryId"), Comp->CurrentEntryId);
                    Message = FString::Printf(TEXT("Skipped to entry '%s' in schedule '%s'"), *EntryId, *ScheduleId);
                }
            }
        }
    }

    // ==================== SPAWNER (6 actions) ====================
    else if (LowerSub == TEXT("create_spawner"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        FString SpawnClassPath;
        Payload->TryGetStringField(TEXT("spawnClassPath"), SpawnClassPath);
        double MaxCount = 5;
        Payload->TryGetNumberField(TEXT("maxCount"), MaxCount);
        double Interval = 5.0;
        Payload->TryGetNumberField(TEXT("interval"), Interval);
        double Radius = 200.0;
        Payload->TryGetNumberField(TEXT("radius"), Radius);
        
        AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
        if (!TargetActor)
        {
            bSuccess = false;
            Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
            ErrorCode = TEXT("ACTOR_NOT_FOUND");
        }
        else
        {
            UMcpSpawnerComponent* ExistingComp = TargetActor->FindComponentByClass<UMcpSpawnerComponent>();
            if (ExistingComp)
            {
                bSuccess = false;
                Message = FString::Printf(TEXT("SpawnerComponent already exists on '%s'"), *ActorName);
                ErrorCode = TEXT("SPAWNER_EXISTS");
            }
            else
            {
                UMcpSpawnerComponent* Comp = NewObject<UMcpSpawnerComponent>(TargetActor, TEXT("SpawnerComponent"));
                if (Comp)
                {
                    Comp->RegisterComponent();
                    Comp->ConfigureSpawner(SpawnClassPath, static_cast<int32>(MaxCount), static_cast<float>(Interval), static_cast<float>(Radius));
                    
                    Resp->SetStringField(TEXT("actorName"), ActorName);
                    Resp->SetStringField(TEXT("spawnClassPath"), SpawnClassPath);
                    Resp->SetNumberField(TEXT("maxCount"), MaxCount);
                    Resp->SetNumberField(TEXT("interval"), Interval);
                    Resp->SetNumberField(TEXT("radius"), Radius);
                    Resp->SetBoolField(TEXT("enabled"), Comp->bIsEnabled);
                    Message = FString::Printf(TEXT("Created spawner on '%s' for '%s'"), *ActorName, *SpawnClassPath);
                }
                else
                {
                    bSuccess = false;
                    Message = TEXT("Failed to create SpawnerComponent");
                    ErrorCode = TEXT("CREATE_FAILED");
                }
            }
        }
    }
    
    else if (LowerSub == TEXT("configure_spawner"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        FString SpawnClassPath;
        Payload->TryGetStringField(TEXT("spawnClassPath"), SpawnClassPath);
        double MaxCount = 0;
        Payload->TryGetNumberField(TEXT("maxCount"), MaxCount);
        double Interval = 0.0;
        Payload->TryGetNumberField(TEXT("interval"), Interval);
        double Radius = 0.0;
        Payload->TryGetNumberField(TEXT("radius"), Radius);
        
        AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
        if (!TargetActor)
        {
            bSuccess = false;
            Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
            ErrorCode = TEXT("ACTOR_NOT_FOUND");
        }
        else
        {
            UMcpSpawnerComponent* Comp = TargetActor->FindComponentByClass<UMcpSpawnerComponent>();
            if (!Comp)
            {
                bSuccess = false;
                Message = FString::Printf(TEXT("SpawnerComponent not found on '%s'"), *ActorName);
                ErrorCode = TEXT("SPAWNER_NOT_FOUND");
            }
            else
            {
                // Only update provided values
                FString ClassPath = SpawnClassPath.IsEmpty() ? Comp->SpawnClassPath : SpawnClassPath;
                int32 Count = MaxCount > 0 ? static_cast<int32>(MaxCount) : Comp->MaxSpawnCount;
                float Int = Interval > 0 ? static_cast<float>(Interval) : Comp->SpawnInterval;
                float Rad = Radius > 0 ? static_cast<float>(Radius) : Comp->SpawnRadius;
                
                Comp->ConfigureSpawner(ClassPath, Count, Int, Rad);
                
                Resp->SetStringField(TEXT("actorName"), ActorName);
                Resp->SetStringField(TEXT("spawnClassPath"), Comp->SpawnClassPath);
                Resp->SetNumberField(TEXT("maxCount"), Comp->MaxSpawnCount);
                Resp->SetNumberField(TEXT("interval"), Comp->SpawnInterval);
                Resp->SetNumberField(TEXT("radius"), Comp->SpawnRadius);
                Message = FString::Printf(TEXT("Configured spawner on '%s'"), *ActorName);
            }
        }
    }
    
    else if (LowerSub == TEXT("set_spawner_enabled"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        bool bEnabled = true;
        Payload->TryGetBoolField(TEXT("enabled"), bEnabled);
        
        AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
        if (!TargetActor)
        {
            bSuccess = false;
            Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
            ErrorCode = TEXT("ACTOR_NOT_FOUND");
        }
        else
        {
            UMcpSpawnerComponent* Comp = TargetActor->FindComponentByClass<UMcpSpawnerComponent>();
            if (!Comp)
            {
                bSuccess = false;
                Message = FString::Printf(TEXT("SpawnerComponent not found on '%s'"), *ActorName);
                ErrorCode = TEXT("SPAWNER_NOT_FOUND");
            }
            else
            {
                Comp->SetEnabled(bEnabled);
                
                Resp->SetStringField(TEXT("actorName"), ActorName);
                Resp->SetBoolField(TEXT("enabled"), Comp->bIsEnabled);
                Resp->SetNumberField(TEXT("spawnedCount"), Comp->GetSpawnedCount());
                Message = FString::Printf(TEXT("Spawner on '%s' %s"), *ActorName, bEnabled ? TEXT("enabled") : TEXT("disabled"));
            }
        }
    }
    
    else if (LowerSub == TEXT("configure_spawn_conditions"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        FString ConditionsJson;
        Payload->TryGetStringField(TEXT("conditionsJson"), ConditionsJson);
        
        AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
        if (!TargetActor)
        {
            bSuccess = false;
            Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
            ErrorCode = TEXT("ACTOR_NOT_FOUND");
        }
        else
        {
            UMcpSpawnerComponent* Comp = TargetActor->FindComponentByClass<UMcpSpawnerComponent>();
            if (!Comp)
            {
                bSuccess = false;
                Message = FString::Printf(TEXT("SpawnerComponent not found on '%s'"), *ActorName);
                ErrorCode = TEXT("SPAWNER_NOT_FOUND");
            }
            else
            {
                Comp->SetSpawnConditions(ConditionsJson);
                
                Resp->SetStringField(TEXT("actorName"), ActorName);
                Resp->SetStringField(TEXT("conditionsJson"), Comp->SpawnConditions);
                Message = FString::Printf(TEXT("Set spawn conditions on '%s'"), *ActorName);
            }
        }
    }
    
    else if (LowerSub == TEXT("despawn_managed_actors"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        
        AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
        if (!TargetActor)
        {
            bSuccess = false;
            Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
            ErrorCode = TEXT("ACTOR_NOT_FOUND");
        }
        else
        {
            UMcpSpawnerComponent* Comp = TargetActor->FindComponentByClass<UMcpSpawnerComponent>();
            if (!Comp)
            {
                bSuccess = false;
                Message = FString::Printf(TEXT("SpawnerComponent not found on '%s'"), *ActorName);
                ErrorCode = TEXT("SPAWNER_NOT_FOUND");
            }
            else
            {
                int32 CountBefore = Comp->GetSpawnedCount();
                Comp->DespawnAll();
                
                Resp->SetStringField(TEXT("actorName"), ActorName);
                Resp->SetNumberField(TEXT("despawnedCount"), CountBefore);
                Resp->SetNumberField(TEXT("currentCount"), Comp->GetSpawnedCount());
                Message = FString::Printf(TEXT("Despawned %d actors from spawner on '%s'"), CountBefore, *ActorName);
            }
        }
    }
    
    else if (LowerSub == TEXT("get_spawned_count"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        
        AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
        if (!TargetActor)
        {
            bSuccess = false;
            Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
            ErrorCode = TEXT("ACTOR_NOT_FOUND");
        }
        else
        {
            UMcpSpawnerComponent* Comp = TargetActor->FindComponentByClass<UMcpSpawnerComponent>();
            if (!Comp)
            {
                bSuccess = false;
                Message = FString::Printf(TEXT("SpawnerComponent not found on '%s'"), *ActorName);
                ErrorCode = TEXT("SPAWNER_NOT_FOUND");
            }
            else
            {
                int32 Count = Comp->GetSpawnedCount();
                
                Resp->SetStringField(TEXT("actorName"), ActorName);
                Resp->SetNumberField(TEXT("spawnedCount"), Count);
                Resp->SetNumberField(TEXT("maxCount"), Comp->MaxSpawnCount);
                Resp->SetBoolField(TEXT("canSpawn"), Comp->CanSpawn());
                Resp->SetBoolField(TEXT("enabled"), Comp->bIsEnabled);
                Resp->SetBoolField(TEXT("hasValidClass"), Comp->HasValidSpawnClass());
                
                // Get list of spawned actors
                TArray<TSharedPtr<FJsonValue>> SpawnedArray;
                for (AActor* SpawnedActor : Comp->GetSpawnedActors())
                {
                    if (SpawnedActor)
                    {
                        TSharedPtr<FJsonObject> ActorObj = MakeShared<FJsonObject>();
                        ActorObj->SetStringField(TEXT("name"), SpawnedActor->GetActorLabel());
                        ActorObj->SetStringField(TEXT("class"), SpawnedActor->GetClass()->GetName());
                        
                        FVector Loc = SpawnedActor->GetActorLocation();
                        TSharedPtr<FJsonObject> LocObj = MakeShared<FJsonObject>();
                        LocObj->SetNumberField(TEXT("x"), Loc.X);
                        LocObj->SetNumberField(TEXT("y"), Loc.Y);
                        LocObj->SetNumberField(TEXT("z"), Loc.Z);
                        ActorObj->SetObjectField(TEXT("location"), LocObj);
                        
                        SpawnedArray.Add(MakeShared<FJsonValueObject>(ActorObj));
                    }
                }
                Resp->SetArrayField(TEXT("spawnedActors"), SpawnedArray);
                
                Message = FString::Printf(TEXT("Spawner on '%s': %d/%d actors"), *ActorName, Count, Comp->MaxSpawnCount);
            }
        }
    }

    // ==================== ATTACHMENT (6 actions - native UE) ====================
    else if (LowerSub == TEXT("attach_to_socket"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        FString ParentActorName;
        Payload->TryGetStringField(TEXT("parentActorName"), ParentActorName);
        FString SocketName;
        Payload->TryGetStringField(TEXT("socketName"), SocketName);
        FString AttachRule = TEXT("KeepRelative");
        Payload->TryGetStringField(TEXT("attachRule"), AttachRule);
        
        AActor* ChildActor = FindActorByLabelOrName<AActor>(ActorName);
        AActor* ParentActor = FindActorByLabelOrName<AActor>(ParentActorName);
        
        if (!ChildActor)
        {
            bSuccess = false;
            Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
            ErrorCode = TEXT("ACTOR_NOT_FOUND");
        }
        else if (!ParentActor)
        {
            bSuccess = false;
            Message = FString::Printf(TEXT("Parent actor '%s' not found"), *ParentActorName);
            ErrorCode = TEXT("PARENT_NOT_FOUND");
        }
        else
        {
            FAttachmentTransformRules Rules = FAttachmentTransformRules::KeepRelativeTransform;
            if (AttachRule.Equals(TEXT("KeepWorld"), ESearchCase::IgnoreCase))
            {
                Rules = FAttachmentTransformRules::KeepWorldTransform;
            }
            else if (AttachRule.Equals(TEXT("SnapToTarget"), ESearchCase::IgnoreCase))
            {
                Rules = FAttachmentTransformRules::SnapToTargetNotIncludingScale;
            }
            
            FName Socket = SocketName.IsEmpty() ? NAME_None : FName(*SocketName);
            ChildActor->AttachToActor(ParentActor, Rules, Socket);
            
            Resp->SetStringField(TEXT("actorName"), ActorName);
            Resp->SetStringField(TEXT("parentActorName"), ParentActorName);
            Resp->SetStringField(TEXT("socketName"), SocketName);
            Resp->SetStringField(TEXT("attachRule"), AttachRule);
            Message = FString::Printf(TEXT("Attached '%s' to '%s'%s"), *ActorName, *ParentActorName, SocketName.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" at socket '%s'"), *SocketName));
        }
    }
    
    else if (LowerSub == TEXT("detach_from_parent"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        FString DetachRule = TEXT("KeepWorld");
        Payload->TryGetStringField(TEXT("detachRule"), DetachRule);
        
        AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
        if (!TargetActor)
        {
            bSuccess = false;
            Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
            ErrorCode = TEXT("ACTOR_NOT_FOUND");
        }
        else
        {
            AActor* ParentActor = TargetActor->GetAttachParentActor();
            FString ParentName = ParentActor ? ParentActor->GetActorLabel() : TEXT("none");
            
            FDetachmentTransformRules Rules = FDetachmentTransformRules::KeepWorldTransform;
            if (DetachRule.Equals(TEXT("KeepRelative"), ESearchCase::IgnoreCase))
            {
                Rules = FDetachmentTransformRules::KeepRelativeTransform;
            }
            
            TargetActor->DetachFromActor(Rules);
            
            Resp->SetStringField(TEXT("actorName"), ActorName);
            Resp->SetStringField(TEXT("previousParent"), ParentName);
            Resp->SetStringField(TEXT("detachRule"), DetachRule);
            Message = FString::Printf(TEXT("Detached '%s' from '%s'"), *ActorName, *ParentName);
        }
    }
    
    else if (LowerSub == TEXT("transfer_control"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        FString NewOwnerName;
        Payload->TryGetStringField(TEXT("newOwnerName"), NewOwnerName);
        
        AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
        AActor* NewOwner = nullptr;
        
        if (!NewOwnerName.IsEmpty())
        {
            NewOwner = FindActorByLabelOrName<AActor>(NewOwnerName);
        }
        
        if (!TargetActor)
        {
            bSuccess = false;
            Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
            ErrorCode = TEXT("ACTOR_NOT_FOUND");
        }
        else
        {
            AActor* OldOwner = TargetActor->GetOwner();
            FString OldOwnerName = OldOwner ? OldOwner->GetActorLabel() : TEXT("none");
            
            TargetActor->SetOwner(NewOwner);
            
            Resp->SetStringField(TEXT("actorName"), ActorName);
            Resp->SetStringField(TEXT("previousOwner"), OldOwnerName);
            Resp->SetStringField(TEXT("newOwner"), NewOwner ? NewOwner->GetActorLabel() : TEXT("none"));
            Message = FString::Printf(TEXT("Transferred ownership of '%s' from '%s' to '%s'"), *ActorName, *OldOwnerName, NewOwner ? *NewOwner->GetActorLabel() : TEXT("none"));
        }
    }
    
    else if (LowerSub == TEXT("configure_attachment_rules"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        bool bWeldSimulatedBodies = false;
        Payload->TryGetBoolField(TEXT("weldSimulatedBodies"), bWeldSimulatedBodies);
        
        AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
        if (!TargetActor)
        {
            bSuccess = false;
            Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
            ErrorCode = TEXT("ACTOR_NOT_FOUND");
        }
        else
        {
            USceneComponent* Root = TargetActor->GetRootComponent();
            if (Root)
            {
                // Store configuration as component tags
                Root->ComponentTags.RemoveAll([](const FName& Tag) {
                    return Tag.ToString().StartsWith(TEXT("AttachRule_"));
                });
                Root->ComponentTags.Add(*FString::Printf(TEXT("AttachRule_WeldBodies:%s"), bWeldSimulatedBodies ? TEXT("true") : TEXT("false")));
                
                Resp->SetStringField(TEXT("actorName"), ActorName);
                Resp->SetBoolField(TEXT("weldSimulatedBodies"), bWeldSimulatedBodies);
                Message = FString::Printf(TEXT("Configured attachment rules for '%s'"), *ActorName);
            }
            else
            {
                bSuccess = false;
                Message = FString::Printf(TEXT("Actor '%s' has no root component"), *ActorName);
                ErrorCode = TEXT("NO_ROOT_COMPONENT");
            }
        }
    }
    
    else if (LowerSub == TEXT("get_attached_actors"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        bool bRecursive = false;
        Payload->TryGetBoolField(TEXT("recursive"), bRecursive);
        
        AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
        if (!TargetActor)
        {
            bSuccess = false;
            Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
            ErrorCode = TEXT("ACTOR_NOT_FOUND");
        }
        else
        {
            TArray<AActor*> AttachedActors;
            TargetActor->GetAttachedActors(AttachedActors, true, bRecursive);
            
            TArray<TSharedPtr<FJsonValue>> AttachedArray;
            for (AActor* Attached : AttachedActors)
            {
                if (Attached)
                {
                    TSharedPtr<FJsonObject> AttachedObj = MakeShared<FJsonObject>();
                    AttachedObj->SetStringField(TEXT("name"), Attached->GetActorLabel());
                    AttachedObj->SetStringField(TEXT("class"), Attached->GetClass()->GetName());
                    
                    FName SocketName = Attached->GetAttachParentSocketName();
                    if (!SocketName.IsNone())
                    {
                        AttachedObj->SetStringField(TEXT("socketName"), SocketName.ToString());
                    }
                    
                    AttachedArray.Add(MakeShared<FJsonValueObject>(AttachedObj));
                }
            }
            
            Resp->SetStringField(TEXT("actorName"), ActorName);
            Resp->SetArrayField(TEXT("attachedActors"), AttachedArray);
            Resp->SetNumberField(TEXT("count"), AttachedArray.Num());
            Resp->SetBoolField(TEXT("recursive"), bRecursive);
            Message = FString::Printf(TEXT("Found %d attached actors on '%s'"), AttachedArray.Num(), *ActorName);
        }
    }
    
    else if (LowerSub == TEXT("get_attachment_parent"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        
        AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
        if (!TargetActor)
        {
            bSuccess = false;
            Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
            ErrorCode = TEXT("ACTOR_NOT_FOUND");
        }
        else
        {
            AActor* ParentActor = TargetActor->GetAttachParentActor();
            FName SocketName = TargetActor->GetAttachParentSocketName();
            
            Resp->SetStringField(TEXT("actorName"), ActorName);
            Resp->SetBoolField(TEXT("hasParent"), ParentActor != nullptr);
            
            if (ParentActor)
            {
                Resp->SetStringField(TEXT("parentName"), ParentActor->GetActorLabel());
                Resp->SetStringField(TEXT("parentClass"), ParentActor->GetClass()->GetName());
                
                if (!SocketName.IsNone())
                {
                    Resp->SetStringField(TEXT("socketName"), SocketName.ToString());
                }
                
                Message = FString::Printf(TEXT("'%s' is attached to '%s'%s"), *ActorName, *ParentActor->GetActorLabel(), SocketName.IsNone() ? TEXT("") : *FString::Printf(TEXT(" at socket '%s'"), *SocketName.ToString()));
            }
            else
            {
                Message = FString::Printf(TEXT("'%s' is not attached to any parent"), *ActorName);
            }
        }
    }

    // ==================== UNKNOWN ACTION ====================
    else
    {
        SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Unknown gameplay primitives action: %s"), *SubAction),
            TEXT("UNKNOWN_ACTION"));
        return true;
    }

    // ==================== SEND RESPONSE ====================
    if (bSuccess)
    {
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetStringField(TEXT("message"), Message);
        SendAutomationResponse(RequestingSocket, RequestId, true, Message, Resp);
    }
    else
    {
        Resp->SetBoolField(TEXT("success"), false);
        Resp->SetStringField(TEXT("message"), Message);
        Resp->SetStringField(TEXT("errorCode"), ErrorCode);
        SendAutomationError(RequestingSocket, RequestId, Message, ErrorCode);
    }
    
    return true;

#else
    SendAutomationError(RequestingSocket, RequestId,
                       TEXT("Gameplay primitives require Editor build"),
                       TEXT("EDITOR_ONLY"));
    return true;
#endif
}
