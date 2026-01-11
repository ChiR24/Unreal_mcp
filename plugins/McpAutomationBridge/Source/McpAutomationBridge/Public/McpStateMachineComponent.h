// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "McpStateMachineComponent.generated.h"

// Forward declarations
struct FMcpStateDefinition;
struct FMcpStateTransition;

// Delegate for state change notifications
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMcpStateChanged, const FString&, OldState, const FString&, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMcpStateTimerExpired, const FString&, FromState, const FString&, ToState);

// State definition struct
USTRUCT(BlueprintType)
struct FMcpStateDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
    FString StateName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
    FString StateData; // JSON metadata
};

// State transition struct
USTRUCT(BlueprintType)
struct FMcpStateTransition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition")
    FString FromState;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition")
    FString ToState;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition")
    FString Conditions; // JSON predicate (for future condition system integration)
};

/**
 * UMcpStateMachineComponent
 * 
 * Replicated actor component for managing finite state machines on actors.
 * 
 * Features:
 * - Network replication with OnRep callbacks
 * - Configurable states with metadata
 * - Validated state transitions (permissive or strict mode)
 * - Timer-based auto-transitions
 * - Blueprint-bindable events for state changes
 * 
 * Use cases:
 * - AI states (idle, patrol, combat, flee)
 * - Door states (closed, opening, open, closing)
 * - Game object states (inactive, active, cooldown)
 */
UCLASS(ClassGroup=(MCP), meta=(BlueprintSpawnableComponent))
class MCPAUTOMATIONBRIDGE_API UMcpStateMachineComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UMcpStateMachineComponent();

    // Current state - uses ReplicatedUsing for OnRep callback
    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_CurrentState, Category = "State Machine")
    FString CurrentState;

    // All defined states
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "State Machine")
    TArray<FMcpStateDefinition> States;

    // All defined transitions
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "State Machine")
    TArray<FMcpStateTransition> Transitions;

    // Timer configuration
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "State Machine|Timer")
    float StateTimer = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "State Machine|Timer")
    FString AutoTransitionTarget;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "State Machine|Timer")
    bool bTimerActive = false;

    // Blueprint-bindable events
    UPROPERTY(BlueprintAssignable, Category = "State Machine|Events")
    FOnMcpStateChanged OnStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "State Machine|Events")
    FOnMcpStateTimerExpired OnStateTimerExpired;

    // Replication
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // OnRep callback
    UFUNCTION()
    void OnRep_CurrentState();

    // Tick for timer processing
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // Public API - Server-only mutations
    UFUNCTION(BlueprintCallable, Category = "State Machine")
    void AddState(const FString& StateName, const FString& StateData);

    UFUNCTION(BlueprintCallable, Category = "State Machine")
    void AddTransition(const FString& FromState, const FString& ToState, const FString& Conditions);

    UFUNCTION(BlueprintCallable, Category = "State Machine")
    bool SetState(const FString& NewState, bool bForce = false);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "State Machine")
    FString GetCurrentState() const { return CurrentState; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "State Machine")
    FString GetStateData(const FString& StateName) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "State Machine")
    float GetTimeInState() const;

    UFUNCTION(BlueprintCallable, Category = "State Machine")
    void ConfigureStateTimer(float Duration, const FString& TargetState);

    UFUNCTION(BlueprintCallable, Category = "State Machine")
    void ClearStateTimer();

    // Query helpers
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "State Machine")
    bool HasState(const FString& StateName) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "State Machine")
    bool IsTransitionValid(const FString& FromState, const FString& ToState) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "State Machine")
    TArray<FString> GetAvailableTransitions() const;

private:
    // Cache for OnRep comparison
    FString PreviousState;

    // Time tracking
    float StateStartTime = 0.0f;
    float TimerElapsed = 0.0f;

    // Helper to find state definition
    const FMcpStateDefinition* FindState(const FString& StateName) const;

    // Check if transition is valid based on defined transitions
    bool ValidateTransition(const FString& FromState, const FString& ToState) const;
};
