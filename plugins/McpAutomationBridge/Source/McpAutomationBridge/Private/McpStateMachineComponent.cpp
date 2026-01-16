// Copyright Epic Games, Inc. All Rights Reserved.

#include "McpStateMachineComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpStateMachine, Log, All);

UMcpStateMachineComponent::UMcpStateMachineComponent()
{
    // Enable replication by default
    SetIsReplicatedByDefault(true);
    
    // Enable ticking for timer processing
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
    
    // Initialize with default values
    CurrentState = TEXT("");
    PreviousState = TEXT("");
    StateStartTime = 0.0f;
    StateTimer = 0.0f;
    AutoTransitionTarget = TEXT("");
    bTimerActive = false;
    TimerElapsed = 0.0f;
}

void UMcpStateMachineComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    
    DOREPLIFETIME(UMcpStateMachineComponent, CurrentState);
    DOREPLIFETIME(UMcpStateMachineComponent, States);
    DOREPLIFETIME(UMcpStateMachineComponent, Transitions);
    DOREPLIFETIME(UMcpStateMachineComponent, StateTimer);
    DOREPLIFETIME(UMcpStateMachineComponent, AutoTransitionTarget);
    DOREPLIFETIME(UMcpStateMachineComponent, bTimerActive);
}

void UMcpStateMachineComponent::OnRep_CurrentState()
{
    // Store previous for callback (on clients)
    FString OldState = PreviousState;
    PreviousState = CurrentState;
    
    // Update start time on clients
    UWorld* World = GetWorld();
    if (World)
    {
        StateStartTime = World->GetTimeSeconds();
    }
    
    // Broadcast state change event
    OnStateChanged.Broadcast(OldState, CurrentState);
    
    UE_LOG(LogMcpStateMachine, Verbose, TEXT("StateMachine replicated: '%s' -> '%s'"), 
        *OldState, *CurrentState);
}

void UMcpStateMachineComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
    // Only process timer on server
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority())
    {
        return;
    }
    
    // Skip if timer not active
    if (!bTimerActive)
    {
        return;
    }
    
    // Process timer
    TimerElapsed += DeltaTime;
    if (TimerElapsed >= StateTimer)
    {
        // Timer expired
        FString FromState = CurrentState;
        FString ToState = AutoTransitionTarget;
        
        // Clear timer first
        bTimerActive = false;
        TimerElapsed = 0.0f;
        
        // Broadcast timer expired event
        OnStateTimerExpired.Broadcast(FromState, ToState);
        
        // Attempt transition (force it since this is a configured auto-transition)
        if (!ToState.IsEmpty())
        {
            SetState(ToState, true);
        }
        
        UE_LOG(LogMcpStateMachine, Log, TEXT("StateMachine timer expired: '%s' -> '%s'"), 
            *FromState, *ToState);
    }
}

void UMcpStateMachineComponent::AddState(const FString& StateName, const FString& StateData)
{
    // Only allow server to add states
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority())
    {
        UE_LOG(LogMcpStateMachine, Warning, TEXT("AddState called on client - ignored"));
        return;
    }
    
    // Check for duplicate
    if (HasState(StateName))
    {
        UE_LOG(LogMcpStateMachine, Warning, TEXT("State '%s' already exists"), *StateName);
        return;
    }
    
    FMcpStateDefinition NewState;
    NewState.StateName = StateName;
    NewState.StateData = StateData;
    
    States.Add(NewState);
    
    UE_LOG(LogMcpStateMachine, Log, TEXT("StateMachine added state: '%s'"), *StateName);
}

void UMcpStateMachineComponent::AddTransition(const FString& FromState, const FString& ToState, const FString& Conditions)
{
    // Only allow server to add transitions
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority())
    {
        UE_LOG(LogMcpStateMachine, Warning, TEXT("AddTransition called on client - ignored"));
        return;
    }
    
    FMcpStateTransition NewTransition;
    NewTransition.FromState = FromState;
    NewTransition.ToState = ToState;
    NewTransition.Conditions = Conditions;
    
    Transitions.Add(NewTransition);
    
    UE_LOG(LogMcpStateMachine, Log, TEXT("StateMachine added transition: '%s' -> '%s'"), *FromState, *ToState);
}

bool UMcpStateMachineComponent::SetState(const FString& NewState, bool bForce)
{
    // Only allow server to change state
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority())
    {
        UE_LOG(LogMcpStateMachine, Warning, TEXT("SetState called on client - ignored"));
        return false;
    }
    
    // Early out if no change
    if (CurrentState.Equals(NewState, ESearchCase::CaseSensitive))
    {
        return true;
    }
    
    // Validate transition unless forced
    if (!bForce && !ValidateTransition(CurrentState, NewState))
    {
        UE_LOG(LogMcpStateMachine, Warning, TEXT("Invalid transition: '%s' -> '%s'"), 
            *CurrentState, *NewState);
        return false;
    }
    
    // Store old state for callback
    FString OldState = CurrentState;
    PreviousState = OldState;
    CurrentState = NewState;
    
    // Update timing
    UWorld* World = GetWorld();
    if (World)
    {
        StateStartTime = World->GetTimeSeconds();
    }
    
    // Clear timer on state change (must reconfigure for new state)
    bTimerActive = false;
    TimerElapsed = 0.0f;
    
    // Broadcast state change on server
    OnStateChanged.Broadcast(OldState, CurrentState);
    
    UE_LOG(LogMcpStateMachine, Log, TEXT("StateMachine state changed: '%s' -> '%s'"), 
        *OldState, *CurrentState);
    
    return true;
}

FString UMcpStateMachineComponent::GetStateData(const FString& StateName) const
{
    const FMcpStateDefinition* State = FindState(StateName);
    if (State)
    {
        return State->StateData;
    }
    return TEXT("");
}

float UMcpStateMachineComponent::GetTimeInState() const
{
    UWorld* World = GetWorld();
    if (World)
    {
        return World->GetTimeSeconds() - StateStartTime;
    }
    return 0.0f;
}

void UMcpStateMachineComponent::ConfigureStateTimer(float Duration, const FString& TargetState)
{
    // Only allow server to configure timer
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority())
    {
        UE_LOG(LogMcpStateMachine, Warning, TEXT("ConfigureStateTimer called on client - ignored"));
        return;
    }
    
    StateTimer = FMath::Max(0.01f, Duration); // Prevent zero/negative
    AutoTransitionTarget = TargetState;
    bTimerActive = true;
    TimerElapsed = 0.0f;
    
    UE_LOG(LogMcpStateMachine, Log, TEXT("StateMachine timer configured: %.2fs -> '%s'"), 
        StateTimer, *AutoTransitionTarget);
}

void UMcpStateMachineComponent::ClearStateTimer()
{
    // Only allow server to clear timer
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority())
    {
        UE_LOG(LogMcpStateMachine, Warning, TEXT("ClearStateTimer called on client - ignored"));
        return;
    }
    
    bTimerActive = false;
    TimerElapsed = 0.0f;
    StateTimer = 0.0f;
    AutoTransitionTarget = TEXT("");
    
    UE_LOG(LogMcpStateMachine, Log, TEXT("StateMachine timer cleared"));
}

bool UMcpStateMachineComponent::HasState(const FString& StateName) const
{
    return FindState(StateName) != nullptr;
}

bool UMcpStateMachineComponent::IsTransitionValid(const FString& FromState, const FString& ToState) const
{
    return ValidateTransition(FromState, ToState);
}

TArray<FString> UMcpStateMachineComponent::GetAvailableTransitions() const
{
    TArray<FString> Result;
    
    // If no transitions defined, return empty (permissive mode - all transitions allowed)
    if (Transitions.Num() == 0)
    {
        // In permissive mode, list all states as available
        for (const FMcpStateDefinition& State : States)
        {
            if (!State.StateName.Equals(CurrentState, ESearchCase::CaseSensitive))
            {
                Result.Add(State.StateName);
            }
        }
    }
    else
    {
        // In strict mode, list only valid transitions from current state
        for (const FMcpStateTransition& Transition : Transitions)
        {
            if (Transition.FromState.Equals(CurrentState, ESearchCase::CaseSensitive))
            {
                Result.Add(Transition.ToState);
            }
        }
    }
    
    return Result;
}

const FMcpStateDefinition* UMcpStateMachineComponent::FindState(const FString& StateName) const
{
    for (const FMcpStateDefinition& State : States)
    {
        if (State.StateName.Equals(StateName, ESearchCase::CaseSensitive))
        {
            return &State;
        }
    }
    return nullptr;
}

bool UMcpStateMachineComponent::ValidateTransition(const FString& FromState, const FString& ToState) const
{
    // If no transitions defined, allow all transitions (permissive mode)
    if (Transitions.Num() == 0)
    {
        return true;
    }
    
    // Strict mode - check if transition is explicitly defined
    for (const FMcpStateTransition& Transition : Transitions)
    {
        if (Transition.FromState.Equals(FromState, ESearchCase::CaseSensitive) &&
            Transition.ToState.Equals(ToState, ESearchCase::CaseSensitive))
        {
            return true;
        }
    }
    
    return false;
}
