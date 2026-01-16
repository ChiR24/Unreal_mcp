// Copyright Epic Games, Inc. All Rights Reserved.

#include "McpScheduleComponent.h"
#include "McpWorldTimeSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpSchedule, Log, All);

UMcpScheduleComponent::UMcpScheduleComponent()
{
    // Enable replication by default
    SetIsReplicatedByDefault(true);
    
    // Enable ticking for time-based updates
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
    
    // Initialize with default values
    ScheduleId = TEXT("");
    CurrentEntryId = TEXT("");
    PreviousEntryId = TEXT("");
    bScheduleActive = true;
    bPreviousScheduleActive = true;
    bLooping = true;
}

void UMcpScheduleComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    
    DOREPLIFETIME(UMcpScheduleComponent, ScheduleId);
    DOREPLIFETIME(UMcpScheduleComponent, Entries);
    DOREPLIFETIME(UMcpScheduleComponent, CurrentEntryId);
    DOREPLIFETIME(UMcpScheduleComponent, bScheduleActive);
    DOREPLIFETIME(UMcpScheduleComponent, bLooping);
}

void UMcpScheduleComponent::OnRep_CurrentEntry()
{
    // Store previous for callback (on clients)
    FString OldEntry = PreviousEntryId;
    PreviousEntryId = CurrentEntryId;
    
    // Find activity name for the new entry
    FString ActivityName = TEXT("");
    const FMcpScheduleEntry* Entry = FindEntry(CurrentEntryId);
    if (Entry)
    {
        ActivityName = Entry->ActivityName;
    }
    
    // Broadcast entry change event
    OnScheduleEntryChanged.Broadcast(OldEntry, CurrentEntryId, ActivityName);
    
    UE_LOG(LogMcpSchedule, Verbose, TEXT("Schedule entry replicated: '%s' -> '%s' (%s)"), 
        *OldEntry, *CurrentEntryId, *ActivityName);
}

void UMcpScheduleComponent::OnRep_ScheduleActive()
{
    bool bOldActive = bPreviousScheduleActive;
    bPreviousScheduleActive = bScheduleActive;
    
    // Broadcast active change event
    OnScheduleActiveChanged.Broadcast(bScheduleActive);
    
    UE_LOG(LogMcpSchedule, Verbose, TEXT("Schedule active replicated: %s -> %s"), 
        bOldActive ? TEXT("true") : TEXT("false"),
        bScheduleActive ? TEXT("true") : TEXT("false"));
}

void UMcpScheduleComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
    // Only process on server
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority())
    {
        return;
    }
    
    // Skip if not active
    if (!bScheduleActive)
    {
        return;
    }
    
    // Query world time subsystem
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }
    
    UMcpWorldTimeSubsystem* WorldTime = World->GetSubsystem<UMcpWorldTimeSubsystem>();
    float CurrentHour = WorldTime ? WorldTime->GetWorldTime() : 0.0f;
    int32 CurrentDay = WorldTime ? WorldTime->GetDay() : 1;
    int32 CurrentDayOfWeek = GetDayOfWeek(CurrentDay);
    
    // Find entry that should be active at current time
    const FMcpScheduleEntry* ActiveEntry = FindActiveEntry(CurrentHour, CurrentDayOfWeek);
    
    FString NewEntryId = ActiveEntry ? ActiveEntry->EntryId : TEXT("");
    
    // Check if entry changed
    if (!NewEntryId.Equals(CurrentEntryId, ESearchCase::CaseSensitive))
    {
        FString OldEntry = CurrentEntryId;
        FString OldActivity = TEXT("");
        
        // Get old activity name
        const FMcpScheduleEntry* OldEntryData = FindEntry(CurrentEntryId);
        if (OldEntryData)
        {
            OldActivity = OldEntryData->ActivityName;
        }
        
        // Update current entry
        PreviousEntryId = CurrentEntryId;
        CurrentEntryId = NewEntryId;
        
        // Get new activity name
        FString NewActivity = ActiveEntry ? ActiveEntry->ActivityName : TEXT("");
        
        // Broadcast on server
        OnScheduleEntryChanged.Broadcast(OldEntry, CurrentEntryId, NewActivity);
        
        UE_LOG(LogMcpSchedule, Log, TEXT("Schedule entry changed: '%s' (%s) -> '%s' (%s)"), 
            *OldEntry, *OldActivity, *CurrentEntryId, *NewActivity);
    }
}

void UMcpScheduleComponent::AddEntry(const FString& EntryId, float StartHour, float EndHour, const FString& Activity, const FString& Data, const TArray<int32>& Days)
{
    // Only allow server to add entries
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority())
    {
        UE_LOG(LogMcpSchedule, Warning, TEXT("AddEntry called on client - ignored"));
        return;
    }
    
    // Check for duplicate
    if (HasEntry(EntryId))
    {
        UE_LOG(LogMcpSchedule, Warning, TEXT("Schedule entry '%s' already exists"), *EntryId);
        return;
    }
    
    // Validate hours
    float ClampedStart = FMath::Clamp(StartHour, 0.0f, 24.0f);
    float ClampedEnd = FMath::Clamp(EndHour, 0.0f, 24.0f);
    
    FMcpScheduleEntry NewEntry;
    NewEntry.EntryId = EntryId;
    NewEntry.StartHour = ClampedStart;
    NewEntry.EndHour = ClampedEnd;
    NewEntry.ActivityName = Activity;
    NewEntry.ActivityData = Data;
    NewEntry.ActiveDays = Days;
    
    Entries.Add(NewEntry);
    
    UE_LOG(LogMcpSchedule, Log, TEXT("Schedule added entry: '%s' (%s) %.1f-%.1f"), 
        *EntryId, *Activity, ClampedStart, ClampedEnd);
}

void UMcpScheduleComponent::RemoveEntry(const FString& EntryId)
{
    // Only allow server to remove entries
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority())
    {
        UE_LOG(LogMcpSchedule, Warning, TEXT("RemoveEntry called on client - ignored"));
        return;
    }
    
    for (int32 i = Entries.Num() - 1; i >= 0; --i)
    {
        if (Entries[i].EntryId.Equals(EntryId, ESearchCase::CaseSensitive))
        {
            Entries.RemoveAt(i);
            
            // Clear current entry if it was the one removed
            if (CurrentEntryId.Equals(EntryId, ESearchCase::CaseSensitive))
            {
                CurrentEntryId = TEXT("");
            }
            
            UE_LOG(LogMcpSchedule, Log, TEXT("Schedule removed entry: '%s'"), *EntryId);
            return;
        }
    }
    
    UE_LOG(LogMcpSchedule, Warning, TEXT("Schedule entry '%s' not found for removal"), *EntryId);
}

void UMcpScheduleComponent::SetScheduleActive(bool bActive)
{
    // Only allow server to set active
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority())
    {
        UE_LOG(LogMcpSchedule, Warning, TEXT("SetScheduleActive called on client - ignored"));
        return;
    }
    
    if (bScheduleActive == bActive)
    {
        return; // No change
    }
    
    bPreviousScheduleActive = bScheduleActive;
    bScheduleActive = bActive;
    
    // Broadcast on server
    OnScheduleActiveChanged.Broadcast(bScheduleActive);
    
    UE_LOG(LogMcpSchedule, Log, TEXT("Schedule active set: %s"), 
        bScheduleActive ? TEXT("true") : TEXT("false"));
}

FMcpScheduleEntry UMcpScheduleComponent::GetCurrentEntry() const
{
    const FMcpScheduleEntry* Entry = FindEntry(CurrentEntryId);
    if (Entry)
    {
        return *Entry;
    }
    
    // Return empty entry if not found
    FMcpScheduleEntry EmptyEntry;
    return EmptyEntry;
}

void UMcpScheduleComponent::SkipToEntry(const FString& EntryId)
{
    // Only allow server to skip
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority())
    {
        UE_LOG(LogMcpSchedule, Warning, TEXT("SkipToEntry called on client - ignored"));
        return;
    }
    
    // Validate entry exists
    const FMcpScheduleEntry* Entry = FindEntry(EntryId);
    if (!Entry)
    {
        UE_LOG(LogMcpSchedule, Warning, TEXT("SkipToEntry: Entry '%s' not found"), *EntryId);
        return;
    }
    
    // Early out if no change
    if (CurrentEntryId.Equals(EntryId, ESearchCase::CaseSensitive))
    {
        return;
    }
    
    FString OldEntry = CurrentEntryId;
    PreviousEntryId = CurrentEntryId;
    CurrentEntryId = EntryId;
    
    // Broadcast on server
    OnScheduleEntryChanged.Broadcast(OldEntry, CurrentEntryId, Entry->ActivityName);
    
    UE_LOG(LogMcpSchedule, Log, TEXT("Schedule skipped to entry: '%s' (%s)"), 
        *EntryId, *Entry->ActivityName);
}

bool UMcpScheduleComponent::IsEntryActive(const FString& EntryId) const
{
    return CurrentEntryId.Equals(EntryId, ESearchCase::CaseSensitive);
}

bool UMcpScheduleComponent::HasEntry(const FString& EntryId) const
{
    return FindEntry(EntryId) != nullptr;
}

const FMcpScheduleEntry* UMcpScheduleComponent::FindEntry(const FString& EntryId) const
{
    for (const FMcpScheduleEntry& Entry : Entries)
    {
        if (Entry.EntryId.Equals(EntryId, ESearchCase::CaseSensitive))
        {
            return &Entry;
        }
    }
    return nullptr;
}

const FMcpScheduleEntry* UMcpScheduleComponent::FindActiveEntry(float CurrentHour, int32 CurrentDayOfWeek) const
{
    for (const FMcpScheduleEntry& Entry : Entries)
    {
        // Check if entry is valid for this day
        if (!IsEntryValidForDay(Entry, CurrentDayOfWeek))
        {
            continue;
        }
        
        // Handle wrapping schedules (e.g., 22:00 - 6:00)
        bool bInTimeRange = false;
        
        if (Entry.StartHour <= Entry.EndHour)
        {
            // Normal range (e.g., 9:00 - 17:00)
            bInTimeRange = (CurrentHour >= Entry.StartHour && CurrentHour < Entry.EndHour);
        }
        else
        {
            // Wrapping range (e.g., 22:00 - 6:00)
            bInTimeRange = (CurrentHour >= Entry.StartHour || CurrentHour < Entry.EndHour);
        }
        
        if (bInTimeRange)
        {
            return &Entry;
        }
    }
    
    return nullptr;
}

bool UMcpScheduleComponent::IsEntryValidForDay(const FMcpScheduleEntry& Entry, int32 DayOfWeek) const
{
    // Empty ActiveDays means all days are valid
    if (Entry.ActiveDays.Num() == 0)
    {
        return true;
    }
    
    // Check if day is in the list
    return Entry.ActiveDays.Contains(DayOfWeek);
}

int32 UMcpScheduleComponent::GetDayOfWeek(int32 WorldDay) const
{
    // Convert world day (1, 2, 3...) to day of week (0=Sun, 1=Mon, ..., 6=Sat)
    // Assume day 1 = Sunday (day 0 in 0-indexed would be Sunday)
    // WorldDay starts at 1, so WorldDay 1 = Sunday (0)
    return (WorldDay - 1) % 7;
}
