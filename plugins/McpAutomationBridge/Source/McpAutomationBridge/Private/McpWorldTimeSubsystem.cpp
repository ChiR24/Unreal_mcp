// Copyright Epic Games, Inc. All Rights Reserved.

#include "McpWorldTimeSubsystem.h"
#include "Math/UnrealMathUtility.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpWorldTime, Log, All);

void UMcpWorldTimeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    
    // Initialize with defaults - call CreateWorldTime() to fully initialize
    LastPeriod = GetCurrentPeriod();
    
    UE_LOG(LogMcpWorldTime, Log, TEXT("MCP World Time Subsystem initialized (awaiting CreateWorldTime call)"));
}

void UMcpWorldTimeSubsystem::Deinitialize()
{
    TimeEvents.Empty();
    bIsInitialized = false;
    
    UE_LOG(LogMcpWorldTime, Log, TEXT("MCP World Time Subsystem deinitialized"));
    
    Super::Deinitialize();
}

bool UMcpWorldTimeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    if (UWorld* World = Cast<UWorld>(Outer))
    {
        // Don't create for preview worlds to avoid overhead and RHI crashes during editor automation
        if (World->WorldType == EWorldType::EditorPreview)
        {
            return false;
        }
    }
    return Super::ShouldCreateSubsystem(Outer);
}


void UMcpWorldTimeSubsystem::Tick(float DeltaTime)
{
    if (!bIsInitialized || bIsPaused || DayLengthSeconds <= 0.0f)
    {
        return;
    }
    
    float OldTime = CurrentTime;
    
    // Calculate time advancement
    // 24 hours in-game = DayLengthSeconds real seconds
    // So 1 real second = 24/DayLengthSeconds in-game hours
    float HoursPerRealSecond = 24.0f / DayLengthSeconds;
    float TimeAdvance = DeltaTime * HoursPerRealSecond * TimeScale;
    
    CurrentTime += TimeAdvance;
    
    // Handle day wrap
    while (CurrentTime >= 24.0f)
    {
        CurrentTime -= 24.0f;
        CurrentDay++;
        
        // Reset daily triggers
        for (FMcpTimeEvent& Event : TimeEvents)
        {
            Event.bHasTriggeredToday = false;
        }
        
        UE_LOG(LogMcpWorldTime, Verbose, TEXT("Day advanced to %d"), CurrentDay);
    }
    
    // Check for time events
    CheckTimeEvents(OldTime, CurrentTime);
    
    // Check for period change
    CheckPeriodChange();
}

TStatId UMcpWorldTimeSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UMcpWorldTimeSubsystem, STATGROUP_Tickables);
}

bool UMcpWorldTimeSubsystem::CreateWorldTime(float InitialTime, float InDayLengthSeconds, bool bStartPaused)
{
    if (InDayLengthSeconds <= 0.0f)
    {
        UE_LOG(LogMcpWorldTime, Warning, TEXT("CreateWorldTime: Invalid DayLengthSeconds (%.2f), using default 1200.0"), InDayLengthSeconds);
        InDayLengthSeconds = 1200.0f;
    }
    
    // Clamp initial time to valid range
    CurrentTime = FMath::Fmod(InitialTime, 24.0f);
    if (CurrentTime < 0.0f)
    {
        CurrentTime += 24.0f;
    }
    
    DayLengthSeconds = InDayLengthSeconds;
    CurrentDay = 1;
    TimeScale = 1.0f;
    bIsPaused = bStartPaused;
    bIsInitialized = true;
    
    // Initialize period tracking
    LastPeriod = GetCurrentPeriod();
    
    // Clear any existing time events
    TimeEvents.Empty();
    
    UE_LOG(LogMcpWorldTime, Log, TEXT("World Time created: Time=%.2f, DayLength=%.0fs, Paused=%s"),
        CurrentTime, DayLengthSeconds, bIsPaused ? TEXT("true") : TEXT("false"));
    
    return true;
}

void UMcpWorldTimeSubsystem::SetWorldTime(float NewTime)
{
    float OldTime = CurrentTime;
    
    // Wrap to valid range
    CurrentTime = FMath::Fmod(NewTime, 24.0f);
    if (CurrentTime < 0.0f)
    {
        CurrentTime += 24.0f;
    }
    
    // If setting time backwards, advance day
    if (NewTime >= 24.0f)
    {
        int32 DaysToAdd = FMath::FloorToInt(NewTime / 24.0f);
        CurrentDay += DaysToAdd;
        
        // Reset daily triggers when day changes
        for (FMcpTimeEvent& Event : TimeEvents)
        {
            Event.bHasTriggeredToday = false;
        }
    }
    
    UE_LOG(LogMcpWorldTime, Verbose, TEXT("World Time set to %.2f (Day %d)"), CurrentTime, CurrentDay);
    
    // Check for period change
    CheckPeriodChange();
}

void UMcpWorldTimeSubsystem::SetTimeScale(float NewScale)
{
    // Clamp to reasonable range (0 to 100x)
    TimeScale = FMath::Clamp(NewScale, 0.0f, 100.0f);
    
    UE_LOG(LogMcpWorldTime, Verbose, TEXT("Time scale set to %.2f"), TimeScale);
}

void UMcpWorldTimeSubsystem::PauseWorldTime(bool bPause)
{
    bIsPaused = bPause;
    
    UE_LOG(LogMcpWorldTime, Log, TEXT("World Time %s"), bIsPaused ? TEXT("PAUSED") : TEXT("RESUMED"));
}

bool UMcpWorldTimeSubsystem::AddTimeEvent(const FString& EventId, float TriggerTime, bool bRecurring, float Interval)
{
    if (EventId.IsEmpty())
    {
        UE_LOG(LogMcpWorldTime, Warning, TEXT("AddTimeEvent: EventId cannot be empty"));
        return false;
    }
    
    // Check for duplicate
    for (const FMcpTimeEvent& Event : TimeEvents)
    {
        if (Event.EventId == EventId)
        {
            UE_LOG(LogMcpWorldTime, Warning, TEXT("AddTimeEvent: Event '%s' already exists"), *EventId);
            return false;
        }
    }
    
    // Normalize trigger time
    float NormalizedTime = FMath::Fmod(TriggerTime, 24.0f);
    if (NormalizedTime < 0.0f)
    {
        NormalizedTime += 24.0f;
    }
    
    FMcpTimeEvent NewEvent;
    NewEvent.EventId = EventId;
    NewEvent.TriggerTime = NormalizedTime;
    NewEvent.bRecurring = bRecurring;
    NewEvent.Interval = FMath::Max(0.1f, Interval);  // Minimum 6-minute interval
    NewEvent.bHasTriggeredToday = false;
    NewEvent.LastTriggerDay = -1;
    
    TimeEvents.Add(NewEvent);
    
    UE_LOG(LogMcpWorldTime, Log, TEXT("Added time event '%s' at %.2f (Recurring=%s, Interval=%.2f)"),
        *EventId, NormalizedTime, bRecurring ? TEXT("true") : TEXT("false"), Interval);
    
    return true;
}

bool UMcpWorldTimeSubsystem::RemoveTimeEvent(const FString& EventId)
{
    for (int32 i = TimeEvents.Num() - 1; i >= 0; --i)
    {
        if (TimeEvents[i].EventId == EventId)
        {
            TimeEvents.RemoveAt(i);
            UE_LOG(LogMcpWorldTime, Log, TEXT("Removed time event '%s'"), *EventId);
            return true;
        }
    }
    
    UE_LOG(LogMcpWorldTime, Warning, TEXT("RemoveTimeEvent: Event '%s' not found"), *EventId);
    return false;
}

EMcpTimePeriod UMcpWorldTimeSubsystem::GetCurrentPeriod() const
{
    // Dawn: 5:00 - 8:00
    if (CurrentTime >= 5.0f && CurrentTime < 8.0f)
    {
        return EMcpTimePeriod::Dawn;
    }
    // Day: 8:00 - 17:00
    else if (CurrentTime >= 8.0f && CurrentTime < 17.0f)
    {
        return EMcpTimePeriod::Day;
    }
    // Dusk: 17:00 - 20:00
    else if (CurrentTime >= 17.0f && CurrentTime < 20.0f)
    {
        return EMcpTimePeriod::Dusk;
    }
    // Night: 20:00 - 5:00 (wraps around midnight)
    else
    {
        return EMcpTimePeriod::Night;
    }
}

void UMcpWorldTimeSubsystem::GetPeriodBounds(EMcpTimePeriod Period, float& OutStart, float& OutEnd) const
{
    switch (Period)
    {
    case EMcpTimePeriod::Dawn:
        OutStart = 5.0f;
        OutEnd = 8.0f;
        break;
    case EMcpTimePeriod::Day:
        OutStart = 8.0f;
        OutEnd = 17.0f;
        break;
    case EMcpTimePeriod::Dusk:
        OutStart = 17.0f;
        OutEnd = 20.0f;
        break;
    case EMcpTimePeriod::Night:
    default:
        OutStart = 20.0f;
        OutEnd = 5.0f;  // Wraps around midnight
        break;
    }
}

void UMcpWorldTimeSubsystem::CheckTimeEvents(float OldTime, float NewTime)
{
    for (FMcpTimeEvent& Event : TimeEvents)
    {
        // Skip if already triggered today (for non-recurring)
        if (!Event.bRecurring && Event.bHasTriggeredToday)
        {
            continue;
        }
        
        // Skip recurring events that have already triggered this interval
        if (Event.bRecurring && Event.LastTriggerDay == CurrentDay)
        {
            continue;
        }
        
        // Check if event time was crossed
        bool bShouldTrigger = false;
        
        if (OldTime <= NewTime)
        {
            // Normal case: time moved forward within same day
            bShouldTrigger = (OldTime < Event.TriggerTime && NewTime >= Event.TriggerTime);
        }
        else
        {
            // Day wrapped (OldTime > NewTime means we crossed midnight)
            // Check if event time is after OldTime OR before NewTime
            bShouldTrigger = (OldTime < Event.TriggerTime) || (NewTime >= Event.TriggerTime);
        }
        
        if (bShouldTrigger)
        {
            Event.bHasTriggeredToday = true;
            Event.LastTriggerDay = CurrentDay;
            
            UE_LOG(LogMcpWorldTime, Log, TEXT("Time event triggered: '%s' at Day %d, Time %.2f"),
                *Event.EventId, CurrentDay, Event.TriggerTime);
            
            // Broadcast the event
            OnTimeEventTriggered.Broadcast(Event.EventId);
        }
    }
}

void UMcpWorldTimeSubsystem::CheckPeriodChange()
{
    EMcpTimePeriod CurrentPeriod = GetCurrentPeriod();
    
    if (CurrentPeriod != LastPeriod)
    {
        UE_LOG(LogMcpWorldTime, Log, TEXT("Time period changed: %d -> %d"),
            static_cast<int32>(LastPeriod), static_cast<int32>(CurrentPeriod));
        
        // Broadcast the period change
        OnPeriodChanged.Broadcast(CurrentPeriod, LastPeriod);
        
        LastPeriod = CurrentPeriod;
    }
}
