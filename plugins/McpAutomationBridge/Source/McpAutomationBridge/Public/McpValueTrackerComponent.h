// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "McpValueTrackerComponent.generated.h"

// Forward declarations
struct FMcpValueThreshold;

// Delegate for value change notifications
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnMcpValueChanged, const FString&, TrackerKey, float, OldValue, float, NewValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMcpThresholdCrossed, const FString&, TrackerKey, float, Threshold);

// Threshold struct for value triggers
USTRUCT(BlueprintType)
struct FMcpValueThreshold
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Threshold")
    float Value = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Threshold")
    FString Direction; // "above", "below", "crossing"

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Threshold")
    FString EventId;

    UPROPERTY()
    bool bHasTriggered = false;
};

/**
 * UMcpValueTrackerComponent
 * 
 * Replicated actor component for tracking numeric values (health, stamina, etc.)
 * with support for thresholds, decay, and regeneration.
 * 
 * Features:
 * - Network replication with OnRep callbacks
 * - Configurable min/max bounds
 * - Passive decay (value decrease over time)
 * - Passive regeneration (value increase over time)
 * - Threshold events when crossing configured values
 * - Blueprint-bindable events for value changes
 */
UCLASS(ClassGroup=(MCP), meta=(BlueprintSpawnableComponent))
class MCPAUTOMATIONBRIDGE_API UMcpValueTrackerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UMcpValueTrackerComponent();

    // Key identifying this tracker (e.g., "Health", "Stamina")
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Value Tracker")
    FString TrackerKey;

    // Current value - uses ReplicatedUsing for OnRep callback
    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_CurrentValue, Category = "Value Tracker")
    float CurrentValue = 0.0f;

    // Min/Max bounds
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Value Tracker")
    float MinValue = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Value Tracker")
    float MaxValue = 100.0f;

    // Pause state for decay/regen
    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_IsPaused, Category = "Value Tracker")
    bool bIsPaused = false;

    // Decay configuration (passive value decrease)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Value Tracker|Decay")
    float DecayRate = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Value Tracker|Decay")
    float DecayInterval = 1.0f;

    // Regen configuration (passive value increase)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Value Tracker|Regen")
    float RegenRate = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Value Tracker|Regen")
    float RegenInterval = 1.0f;

    // Thresholds for triggering events
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Value Tracker")
    TArray<FMcpValueThreshold> Thresholds;

    // Blueprint-bindable events
    UPROPERTY(BlueprintAssignable, Category = "Value Tracker|Events")
    FOnMcpValueChanged OnValueChanged;

    UPROPERTY(BlueprintAssignable, Category = "Value Tracker|Events")
    FOnMcpThresholdCrossed OnThresholdCrossed;

    // Replication
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // OnRep callbacks
    UFUNCTION()
    void OnRep_CurrentValue();

    UFUNCTION()
    void OnRep_IsPaused();

    // Tick for decay/regen
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // Public API - Server-only mutations
    UFUNCTION(BlueprintCallable, Category = "Value Tracker")
    void SetValue(float NewValue);

    UFUNCTION(BlueprintCallable, Category = "Value Tracker")
    void ModifyValue(float Delta);

    UFUNCTION(BlueprintCallable, Category = "Value Tracker")
    float GetValue() const { return CurrentValue; }

    UFUNCTION(BlueprintCallable, Category = "Value Tracker")
    float GetPercentage() const;

    UFUNCTION(BlueprintCallable, Category = "Value Tracker")
    void AddThreshold(float ThresholdValue, const FString& Direction, const FString& EventId);

    UFUNCTION(BlueprintCallable, Category = "Value Tracker")
    void ConfigureDecay(float Rate, float Interval);

    UFUNCTION(BlueprintCallable, Category = "Value Tracker")
    void ConfigureRegen(float Rate, float Interval);

    UFUNCTION(BlueprintCallable, Category = "Value Tracker")
    void SetPaused(bool bPause);

private:
    // Cache for OnRep comparison
    float PreviousValue = 0.0f;

    // Timers for decay/regen
    float DecayTimer = 0.0f;
    float RegenTimer = 0.0f;

    // Process thresholds after value change
    void CheckThresholds(float OldValue, float NewValue);
};
