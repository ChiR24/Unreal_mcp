// Copyright Epic Games, Inc. All Rights Reserved.

#include "McpValueTrackerComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpValueTracker, Log, All);

UMcpValueTrackerComponent::UMcpValueTrackerComponent()
{
    // Enable replication by default
    SetIsReplicatedByDefault(true);
    
    // Enable ticking for decay/regen
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
    
    // Initialize with default values
    TrackerKey = TEXT("Value");
    CurrentValue = 100.0f;
    MinValue = 0.0f;
    MaxValue = 100.0f;
    bIsPaused = false;
    DecayRate = 0.0f;
    DecayInterval = 1.0f;
    RegenRate = 0.0f;
    RegenInterval = 1.0f;
    PreviousValue = CurrentValue;
    DecayTimer = 0.0f;
    RegenTimer = 0.0f;
}

void UMcpValueTrackerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    
    DOREPLIFETIME(UMcpValueTrackerComponent, TrackerKey);
    DOREPLIFETIME(UMcpValueTrackerComponent, CurrentValue);
    DOREPLIFETIME(UMcpValueTrackerComponent, MinValue);
    DOREPLIFETIME(UMcpValueTrackerComponent, MaxValue);
    DOREPLIFETIME(UMcpValueTrackerComponent, bIsPaused);
    DOREPLIFETIME(UMcpValueTrackerComponent, DecayRate);
    DOREPLIFETIME(UMcpValueTrackerComponent, DecayInterval);
    DOREPLIFETIME(UMcpValueTrackerComponent, RegenRate);
    DOREPLIFETIME(UMcpValueTrackerComponent, RegenInterval);
    DOREPLIFETIME(UMcpValueTrackerComponent, Thresholds);
}

void UMcpValueTrackerComponent::OnRep_CurrentValue()
{
    // Calculate change from previous value (for clients)
    float OldValue = PreviousValue;
    PreviousValue = CurrentValue;
    
    // Broadcast value change event
    OnValueChanged.Broadcast(TrackerKey, OldValue, CurrentValue);
    
    // Check thresholds on clients too
    CheckThresholds(OldValue, CurrentValue);
    
    UE_LOG(LogMcpValueTracker, Verbose, TEXT("ValueTracker '%s' replicated: %.2f -> %.2f"), 
        *TrackerKey, OldValue, CurrentValue);
}

void UMcpValueTrackerComponent::OnRep_IsPaused()
{
    UE_LOG(LogMcpValueTracker, Verbose, TEXT("ValueTracker '%s' pause state changed to: %s"), 
        *TrackerKey, bIsPaused ? TEXT("Paused") : TEXT("Active"));
    
    // Reset timers when unpausing to prevent immediate decay/regen
    if (!bIsPaused)
    {
        DecayTimer = 0.0f;
        RegenTimer = 0.0f;
    }
}

void UMcpValueTrackerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
    // Only process decay/regen on server
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority())
    {
        return;
    }
    
    // Skip if paused
    if (bIsPaused)
    {
        return;
    }
    
    // Process decay (value decrease over time)
    if (DecayRate > 0.0f && DecayInterval > 0.0f)
    {
        DecayTimer += DeltaTime;
        if (DecayTimer >= DecayInterval)
        {
            DecayTimer -= DecayInterval;
            ModifyValue(-DecayRate);
        }
    }
    
    // Process regen (value increase over time)
    if (RegenRate > 0.0f && RegenInterval > 0.0f)
    {
        RegenTimer += DeltaTime;
        if (RegenTimer >= RegenInterval)
        {
            RegenTimer -= RegenInterval;
            ModifyValue(RegenRate);
        }
    }
}

void UMcpValueTrackerComponent::SetValue(float NewValue)
{
    // Only allow server to mutate
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority())
    {
        UE_LOG(LogMcpValueTracker, Warning, TEXT("SetValue called on client for '%s' - ignored"), *TrackerKey);
        return;
    }
    
    // Clamp to bounds
    float ClampedValue = FMath::Clamp(NewValue, MinValue, MaxValue);
    
    // Early out if no change
    if (FMath::IsNearlyEqual(ClampedValue, CurrentValue, KINDA_SMALL_NUMBER))
    {
        return;
    }
    
    float OldValue = CurrentValue;
    PreviousValue = OldValue;
    CurrentValue = ClampedValue;
    
    // Broadcast value change on server
    OnValueChanged.Broadcast(TrackerKey, OldValue, CurrentValue);
    
    // Check thresholds
    CheckThresholds(OldValue, CurrentValue);
    
    UE_LOG(LogMcpValueTracker, Log, TEXT("ValueTracker '%s' value changed: %.2f -> %.2f"), 
        *TrackerKey, OldValue, CurrentValue);
}

void UMcpValueTrackerComponent::ModifyValue(float Delta)
{
    SetValue(CurrentValue + Delta);
}

float UMcpValueTrackerComponent::GetPercentage() const
{
    // Protect against division by zero
    float Range = MaxValue - MinValue;
    if (FMath::IsNearlyZero(Range))
    {
        return 0.0f;
    }
    
    return ((CurrentValue - MinValue) / Range) * 100.0f;
}

void UMcpValueTrackerComponent::AddThreshold(float ThresholdValue, const FString& Direction, const FString& EventId)
{
    // Only allow server to add thresholds
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority())
    {
        UE_LOG(LogMcpValueTracker, Warning, TEXT("AddThreshold called on client for '%s' - ignored"), *TrackerKey);
        return;
    }
    
    FMcpValueThreshold NewThreshold;
    NewThreshold.Value = ThresholdValue;
    NewThreshold.Direction = Direction;
    NewThreshold.EventId = EventId;
    NewThreshold.bHasTriggered = false;
    
    Thresholds.Add(NewThreshold);
    
    UE_LOG(LogMcpValueTracker, Log, TEXT("ValueTracker '%s' added threshold: %.2f (%s) -> %s"), 
        *TrackerKey, ThresholdValue, *Direction, *EventId);
}

void UMcpValueTrackerComponent::ConfigureDecay(float Rate, float Interval)
{
    // Only allow server to configure
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority())
    {
        UE_LOG(LogMcpValueTracker, Warning, TEXT("ConfigureDecay called on client for '%s' - ignored"), *TrackerKey);
        return;
    }
    
    DecayRate = FMath::Max(0.0f, Rate);
    DecayInterval = FMath::Max(0.01f, Interval); // Prevent division by zero
    DecayTimer = 0.0f;
    
    UE_LOG(LogMcpValueTracker, Log, TEXT("ValueTracker '%s' decay configured: Rate=%.2f, Interval=%.2fs"), 
        *TrackerKey, DecayRate, DecayInterval);
}

void UMcpValueTrackerComponent::ConfigureRegen(float Rate, float Interval)
{
    // Only allow server to configure
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority())
    {
        UE_LOG(LogMcpValueTracker, Warning, TEXT("ConfigureRegen called on client for '%s' - ignored"), *TrackerKey);
        return;
    }
    
    RegenRate = FMath::Max(0.0f, Rate);
    RegenInterval = FMath::Max(0.01f, Interval); // Prevent division by zero
    RegenTimer = 0.0f;
    
    UE_LOG(LogMcpValueTracker, Log, TEXT("ValueTracker '%s' regen configured: Rate=%.2f, Interval=%.2fs"), 
        *TrackerKey, RegenRate, RegenInterval);
}

void UMcpValueTrackerComponent::SetPaused(bool bPause)
{
    // Only allow server to pause
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority())
    {
        UE_LOG(LogMcpValueTracker, Warning, TEXT("SetPaused called on client for '%s' - ignored"), *TrackerKey);
        return;
    }
    
    if (bIsPaused == bPause)
    {
        return; // No change
    }
    
    bIsPaused = bPause;
    
    // Reset timers when unpausing
    if (!bIsPaused)
    {
        DecayTimer = 0.0f;
        RegenTimer = 0.0f;
    }
    
    UE_LOG(LogMcpValueTracker, Log, TEXT("ValueTracker '%s' paused: %s"), 
        *TrackerKey, bIsPaused ? TEXT("true") : TEXT("false"));
}

void UMcpValueTrackerComponent::CheckThresholds(float OldValue, float NewValue)
{
    for (FMcpValueThreshold& Threshold : Thresholds)
    {
        bool bShouldTrigger = false;
        
        if (Threshold.Direction.Equals(TEXT("above"), ESearchCase::IgnoreCase))
        {
            // Trigger when value goes above threshold
            if (OldValue <= Threshold.Value && NewValue > Threshold.Value)
            {
                bShouldTrigger = true;
            }
        }
        else if (Threshold.Direction.Equals(TEXT("below"), ESearchCase::IgnoreCase))
        {
            // Trigger when value goes below threshold
            if (OldValue >= Threshold.Value && NewValue < Threshold.Value)
            {
                bShouldTrigger = true;
            }
        }
        else if (Threshold.Direction.Equals(TEXT("crossing"), ESearchCase::IgnoreCase))
        {
            // Trigger when crossing in either direction
            bool bWasAbove = OldValue > Threshold.Value;
            bool bIsAbove = NewValue > Threshold.Value;
            if (bWasAbove != bIsAbove)
            {
                bShouldTrigger = true;
            }
        }
        
        if (bShouldTrigger)
        {
            Threshold.bHasTriggered = true;
            OnThresholdCrossed.Broadcast(TrackerKey, Threshold.Value);
            
            UE_LOG(LogMcpValueTracker, Log, TEXT("ValueTracker '%s' threshold crossed: %.2f (%s) -> Event: %s"), 
                *TrackerKey, Threshold.Value, *Threshold.Direction, *Threshold.EventId);
        }
    }
}
