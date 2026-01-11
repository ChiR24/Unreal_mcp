// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "McpScheduleComponent.generated.h"

// Forward declarations
struct FMcpScheduleEntry;

// Delegate for schedule entry change notifications
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnMcpScheduleEntryChanged, const FString&, OldEntry, const FString&, NewEntry, const FString&, ActivityName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMcpScheduleActiveChanged, bool, bActive);

// Schedule entry struct - represents a time-based activity
USTRUCT(BlueprintType)
struct FMcpScheduleEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Schedule")
    FString EntryId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Schedule")
    float StartHour = 0.0f;  // 0-24 (e.g., 9.5 = 9:30 AM)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Schedule")
    float EndHour = 24.0f;   // 0-24 (e.g., 17.0 = 5:00 PM)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Schedule")
    FString ActivityName;    // "work", "sleep", "patrol", etc.

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Schedule")
    FString ActivityData;    // JSON metadata

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Schedule")
    TArray<int32> ActiveDays; // 0=Sun, 1=Mon, etc. Empty = all days
};

/**
 * UMcpScheduleComponent
 * 
 * Replicated actor component for managing time-based schedules.
 * 
 * Features:
 * - Network replication with OnRep callbacks
 * - Time-based schedule entries with day filtering
 * - Integration with McpWorldTimeSubsystem for time queries
 * - Blueprint-bindable events for schedule changes
 * - Looping/non-looping schedule modes
 * 
 * Use cases:
 * - NPC daily routines (work, eat, sleep)
 * - Shop hours (open, closed, special events)
 * - Game events (day/night activities)
 * - Patrol schedules (guard shifts)
 */
UCLASS(ClassGroup=(MCP), meta=(BlueprintSpawnableComponent))
class MCPAUTOMATIONBRIDGE_API UMcpScheduleComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UMcpScheduleComponent();

    // Schedule identifier
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Schedule")
    FString ScheduleId;

    // All schedule entries
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Schedule")
    TArray<FMcpScheduleEntry> Entries;

    // Current active entry - uses ReplicatedUsing for OnRep callback
    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_CurrentEntry, Category = "Schedule")
    FString CurrentEntryId;

    // Is schedule active - uses ReplicatedUsing for OnRep callback
    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_IsActive, Category = "Schedule")
    bool bIsActive = true;

    // Repeat schedule daily
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Schedule")
    bool bLooping = true;

    // Blueprint-bindable events
    UPROPERTY(BlueprintAssignable, Category = "Schedule|Events")
    FOnMcpScheduleEntryChanged OnScheduleEntryChanged;

    UPROPERTY(BlueprintAssignable, Category = "Schedule|Events")
    FOnMcpScheduleActiveChanged OnScheduleActiveChanged;

    // Replication
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // OnRep callbacks
    UFUNCTION()
    void OnRep_CurrentEntry();

    UFUNCTION()
    void OnRep_IsActive();

    // Tick for time-based updates
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // Public API - Server-only mutations
    UFUNCTION(BlueprintCallable, Category = "Schedule")
    void AddEntry(const FString& EntryId, float StartHour, float EndHour, const FString& Activity, const FString& Data, const TArray<int32>& Days);

    UFUNCTION(BlueprintCallable, Category = "Schedule")
    void RemoveEntry(const FString& EntryId);

    UFUNCTION(BlueprintCallable, Category = "Schedule")
    void SetActive(bool bActive);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Schedule")
    FMcpScheduleEntry GetCurrentEntry() const;

    UFUNCTION(BlueprintCallable, Category = "Schedule")
    void SkipToEntry(const FString& EntryId);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Schedule")
    bool IsEntryActive(const FString& EntryId) const;

    // Query helpers
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Schedule")
    bool HasEntry(const FString& EntryId) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Schedule")
    FString GetScheduleId() const { return ScheduleId; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Schedule")
    bool IsActive() const { return bIsActive; }

private:
    // Cache for OnRep comparison
    FString PreviousEntryId;
    bool bPreviousIsActive = true;

    // Helper to find entry definition
    const FMcpScheduleEntry* FindEntry(const FString& EntryId) const;

    // Find entry that should be active at given time/day
    const FMcpScheduleEntry* FindActiveEntry(float CurrentHour, int32 CurrentDayOfWeek) const;

    // Check if entry is valid for the given day
    bool IsEntryValidForDay(const FMcpScheduleEntry& Entry, int32 DayOfWeek) const;

    // Get day of week from world day (0=Sun, 1=Mon, etc.)
    int32 GetDayOfWeek(int32 WorldDay) const;
};
