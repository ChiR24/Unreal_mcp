// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "McpWorldTimeSubsystem.generated.h"

// Time event struct
USTRUCT(BlueprintType)
struct FMcpTimeEvent
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString EventId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TriggerTime = 0.0f;  // In-game time (0-24 hours)

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bRecurring = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Interval = 24.0f;  // Recurrence interval in hours

    UPROPERTY()
    bool bHasTriggeredToday = false;

    UPROPERTY()
    int32 LastTriggerDay = -1;
};

// Time period enum for dawn/day/dusk/night
UENUM(BlueprintType)
enum class EMcpTimePeriod : uint8
{
    Dawn    UMETA(DisplayName = "Dawn"),     // 5:00 - 8:00
    Day     UMETA(DisplayName = "Day"),      // 8:00 - 17:00
    Dusk    UMETA(DisplayName = "Dusk"),     // 17:00 - 20:00
    Night   UMETA(DisplayName = "Night")     // 20:00 - 5:00
};

// Delegate for time events
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMcpTimeEventTriggered, const FString&, EventId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMcpPeriodChanged, EMcpTimePeriod, NewPeriod, EMcpTimePeriod, OldPeriod);

/**
 * UMcpWorldTimeSubsystem
 * 
 * World subsystem for managing in-game time with:
 * - Configurable day length (real seconds per in-game day)
 * - Time scaling (speed up/slow down time)
 * - Pause functionality
 * - Time-based events (trigger at specific times)
 * - Time period detection (dawn, day, dusk, night)
 */
UCLASS()
class MCPAUTOMATIONBRIDGE_API UMcpWorldTimeSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;

    // World Time State
    UPROPERTY(BlueprintReadOnly, Category = "World Time")
    bool bIsInitialized = false;

    UPROPERTY(BlueprintReadOnly, Category = "World Time")
    float CurrentTime = 6.0f;  // Current time in hours (0-24)

    UPROPERTY(BlueprintReadOnly, Category = "World Time")
    int32 CurrentDay = 1;

    UPROPERTY(BlueprintReadWrite, Category = "World Time")
    float DayLengthSeconds = 1200.0f;  // 20 minutes = 1 in-game day

    UPROPERTY(BlueprintReadWrite, Category = "World Time")
    float TimeScale = 1.0f;

    UPROPERTY(BlueprintReadWrite, Category = "World Time")
    bool bIsPaused = false;

    // Events
    UPROPERTY(BlueprintAssignable, Category = "World Time|Events")
    FOnMcpTimeEventTriggered OnTimeEventTriggered;

    UPROPERTY(BlueprintAssignable, Category = "World Time|Events")
    FOnMcpPeriodChanged OnPeriodChanged;

    // Time Events
    UPROPERTY()
    TArray<FMcpTimeEvent> TimeEvents;

    // Public API
    UFUNCTION(BlueprintCallable, Category = "World Time")
    bool CreateWorldTime(float InitialTime = 6.0f, float InDayLengthSeconds = 1200.0f, bool bStartPaused = false);

    UFUNCTION(BlueprintCallable, Category = "World Time")
    void SetWorldTime(float NewTime);

    UFUNCTION(BlueprintCallable, Category = "World Time")
    float GetWorldTime() const { return CurrentTime; }

    UFUNCTION(BlueprintCallable, Category = "World Time")
    int32 GetDay() const { return CurrentDay; }

    UFUNCTION(BlueprintCallable, Category = "World Time")
    int32 GetHour() const { return FMath::FloorToInt(CurrentTime); }

    UFUNCTION(BlueprintCallable, Category = "World Time")
    int32 GetMinute() const { return FMath::FloorToInt(FMath::Frac(CurrentTime) * 60.0f); }

    UFUNCTION(BlueprintCallable, Category = "World Time")
    void SetTimeScale(float NewScale);

    UFUNCTION(BlueprintCallable, Category = "World Time")
    void PauseWorldTime(bool bPause);

    UFUNCTION(BlueprintCallable, Category = "World Time")
    bool AddTimeEvent(const FString& EventId, float TriggerTime, bool bRecurring = false, float Interval = 24.0f);

    UFUNCTION(BlueprintCallable, Category = "World Time")
    bool RemoveTimeEvent(const FString& EventId);

    UFUNCTION(BlueprintCallable, Category = "World Time")
    EMcpTimePeriod GetCurrentPeriod() const;

    UFUNCTION(BlueprintCallable, Category = "World Time")
    void GetPeriodBounds(EMcpTimePeriod Period, float& OutStart, float& OutEnd) const;

private:
    EMcpTimePeriod LastPeriod = EMcpTimePeriod::Day;
    void CheckTimeEvents(float OldTime, float NewTime);
    void CheckPeriodChange();
};
