// Copyright Epic Games, Inc. All Rights Reserved.

#include "McpZoneSubsystem.h"
#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpZone, Log, All);

void UMcpZoneSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    
    Zones.Empty();
    
    UE_LOG(LogMcpZone, Log, TEXT("MCP Zone Subsystem initialized"));
}

void UMcpZoneSubsystem::Deinitialize()
{
    Zones.Empty();
    
    UE_LOG(LogMcpZone, Log, TEXT("MCP Zone Subsystem deinitialized"));
    
    Super::Deinitialize();
}

bool UMcpZoneSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    // Create for all world types (Editor, PIE, Game)
    return true;
}

bool UMcpZoneSubsystem::CreateZone(const FString& ZoneId, const FString& DisplayName, AActor* VolumeActor)
{
    if (ZoneId.IsEmpty())
    {
        UE_LOG(LogMcpZone, Warning, TEXT("CreateZone: ZoneId cannot be empty"));
        return false;
    }
    
    // Check for duplicate
    if (Zones.Contains(ZoneId))
    {
        UE_LOG(LogMcpZone, Warning, TEXT("CreateZone: Zone '%s' already exists"), *ZoneId);
        return false;
    }
    
    FMcpZoneDefinition NewZone;
    NewZone.ZoneId = ZoneId;
    NewZone.DisplayName = DisplayName.IsEmpty() ? ZoneId : DisplayName;
    NewZone.VolumeActor = VolumeActor;
    
    Zones.Add(ZoneId, NewZone);
    
    UE_LOG(LogMcpZone, Log, TEXT("Created zone '%s' (%s)%s"),
        *ZoneId, *NewZone.DisplayName,
        VolumeActor ? *FString::Printf(TEXT(" with volume %s"), *VolumeActor->GetName()) : TEXT(""));
    
    return true;
}

bool UMcpZoneSubsystem::SetZoneProperty(const FString& ZoneId, const FString& PropertyKey, const FString& PropertyValue)
{
    if (ZoneId.IsEmpty() || PropertyKey.IsEmpty())
    {
        UE_LOG(LogMcpZone, Warning, TEXT("SetZoneProperty: ZoneId and PropertyKey cannot be empty"));
        return false;
    }
    
    FMcpZoneDefinition* Zone = Zones.Find(ZoneId);
    if (!Zone)
    {
        UE_LOG(LogMcpZone, Warning, TEXT("SetZoneProperty: Zone '%s' not found"), *ZoneId);
        return false;
    }
    
    Zone->Properties.Add(PropertyKey, PropertyValue);
    
    UE_LOG(LogMcpZone, Verbose, TEXT("Set property '%s' = '%s' on zone '%s'"),
        *PropertyKey, *PropertyValue, *ZoneId);
    
    return true;
}

bool UMcpZoneSubsystem::GetZoneProperty(const FString& ZoneId, const FString& PropertyKey, FString& OutValue) const
{
    if (ZoneId.IsEmpty() || PropertyKey.IsEmpty())
    {
        UE_LOG(LogMcpZone, Warning, TEXT("GetZoneProperty: ZoneId and PropertyKey cannot be empty"));
        return false;
    }
    
    const FMcpZoneDefinition* Zone = Zones.Find(ZoneId);
    if (!Zone)
    {
        UE_LOG(LogMcpZone, Warning, TEXT("GetZoneProperty: Zone '%s' not found"), *ZoneId);
        return false;
    }
    
    const FString* Value = Zone->Properties.Find(PropertyKey);
    if (!Value)
    {
        UE_LOG(LogMcpZone, Verbose, TEXT("GetZoneProperty: Property '%s' not found in zone '%s'"),
            *PropertyKey, *ZoneId);
        return false;
    }
    
    OutValue = *Value;
    return true;
}

bool UMcpZoneSubsystem::GetActorZone(AActor* Actor, FString& OutZoneId, FString& OutZoneName) const
{
    if (!Actor)
    {
        UE_LOG(LogMcpZone, Warning, TEXT("GetActorZone: Actor is null"));
        return false;
    }
    
    const FVector ActorLocation = Actor->GetActorLocation();
    
    // Check all zones with volume actors
    for (const auto& Pair : Zones)
    {
        const FMcpZoneDefinition& Zone = Pair.Value;
        
        AActor* Volume = Zone.VolumeActor.Get();
        if (!Volume)
        {
            continue;
        }
        
        // Check if actor overlaps with the zone volume
        // Try to get a primitive component for bounds checking
        UPrimitiveComponent* PrimitiveComp = Cast<UPrimitiveComponent>(
            Volume->GetComponentByClass(UPrimitiveComponent::StaticClass()));
        
        if (PrimitiveComp)
        {
            // Use overlapping check
            TArray<AActor*> OverlappingActors;
            PrimitiveComp->GetOverlappingActors(OverlappingActors);
            
            for (AActor* Overlapping : OverlappingActors)
            {
                if (Overlapping == Actor)
                {
                    OutZoneId = Zone.ZoneId;
                    OutZoneName = Zone.DisplayName;
                    return true;
                }
            }
        }
        else
        {
            // Fallback: check if actor is within volume's bounding box
            FVector Origin;
            FVector BoxExtent;
            Volume->GetActorBounds(false, Origin, BoxExtent);
            
            const FBox VolumeBounds(Origin - BoxExtent, Origin + BoxExtent);
            if (VolumeBounds.IsInside(ActorLocation))
            {
                OutZoneId = Zone.ZoneId;
                OutZoneName = Zone.DisplayName;
                return true;
            }
        }
    }
    
    // No zone found for actor
    return false;
}

bool UMcpZoneSubsystem::AddZoneEnterEvent(const FString& ZoneId, const FString& EventId, const FString& ConditionId)
{
    if (ZoneId.IsEmpty() || EventId.IsEmpty())
    {
        UE_LOG(LogMcpZone, Warning, TEXT("AddZoneEnterEvent: ZoneId and EventId cannot be empty"));
        return false;
    }
    
    FMcpZoneDefinition* Zone = Zones.Find(ZoneId);
    if (!Zone)
    {
        UE_LOG(LogMcpZone, Warning, TEXT("AddZoneEnterEvent: Zone '%s' not found"), *ZoneId);
        return false;
    }
    
    // Check for duplicate event
    for (const FMcpZoneEvent& Evt : Zone->Events)
    {
        if (Evt.EventId == EventId && Evt.EventType == EMcpZoneEventType::Enter)
        {
            UE_LOG(LogMcpZone, Warning, TEXT("AddZoneEnterEvent: Enter event '%s' already exists in zone '%s'"),
                *EventId, *ZoneId);
            return false;
        }
    }
    
    FMcpZoneEvent NewEvent;
    NewEvent.EventId = EventId;
    NewEvent.EventType = EMcpZoneEventType::Enter;
    NewEvent.ConditionId = ConditionId;
    
    Zone->Events.Add(NewEvent);
    
    UE_LOG(LogMcpZone, Log, TEXT("Added enter event '%s' to zone '%s'%s"),
        *EventId, *ZoneId,
        ConditionId.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" (condition: %s)"), *ConditionId));
    
    return true;
}

bool UMcpZoneSubsystem::AddZoneExitEvent(const FString& ZoneId, const FString& EventId, const FString& ConditionId)
{
    if (ZoneId.IsEmpty() || EventId.IsEmpty())
    {
        UE_LOG(LogMcpZone, Warning, TEXT("AddZoneExitEvent: ZoneId and EventId cannot be empty"));
        return false;
    }
    
    FMcpZoneDefinition* Zone = Zones.Find(ZoneId);
    if (!Zone)
    {
        UE_LOG(LogMcpZone, Warning, TEXT("AddZoneExitEvent: Zone '%s' not found"), *ZoneId);
        return false;
    }
    
    // Check for duplicate event
    for (const FMcpZoneEvent& Evt : Zone->Events)
    {
        if (Evt.EventId == EventId && Evt.EventType == EMcpZoneEventType::Exit)
        {
            UE_LOG(LogMcpZone, Warning, TEXT("AddZoneExitEvent: Exit event '%s' already exists in zone '%s'"),
                *EventId, *ZoneId);
            return false;
        }
    }
    
    FMcpZoneEvent NewEvent;
    NewEvent.EventId = EventId;
    NewEvent.EventType = EMcpZoneEventType::Exit;
    NewEvent.ConditionId = ConditionId;
    
    Zone->Events.Add(NewEvent);
    
    UE_LOG(LogMcpZone, Log, TEXT("Added exit event '%s' to zone '%s'%s"),
        *EventId, *ZoneId,
        ConditionId.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" (condition: %s)"), *ConditionId));
    
    return true;
}

bool UMcpZoneSubsystem::FindZone(const FString& ZoneId, FMcpZoneDefinition& OutZone)
{
    FMcpZoneDefinition* Found = Zones.Find(ZoneId);
    if (Found)
    {
        OutZone = *Found;
        return true;
    }
    return false;
}

FMcpZoneDefinition* UMcpZoneSubsystem::FindZoneInternal(const FString& ZoneId)
{
    return Zones.Find(ZoneId);
}

TArray<FString> UMcpZoneSubsystem::GetAllZoneIds() const
{
    TArray<FString> ZoneIds;
    Zones.GetKeys(ZoneIds);
    return ZoneIds;
}

void UMcpZoneSubsystem::NotifyActorEnteredZone(const FString& ZoneId, AActor* Actor)
{
    if (ZoneId.IsEmpty() || !Actor)
    {
        return;
    }
    
    const FMcpZoneDefinition* Zone = Zones.Find(ZoneId);
    if (!Zone)
    {
        UE_LOG(LogMcpZone, Warning, TEXT("NotifyActorEnteredZone: Zone '%s' not found"), *ZoneId);
        return;
    }
    
    UE_LOG(LogMcpZone, Log, TEXT("Actor '%s' entered zone '%s'"),
        *Actor->GetName(), *ZoneId);
    
    // Broadcast for each enter event
    for (const FMcpZoneEvent& Evt : Zone->Events)
    {
        if (Evt.EventType == EMcpZoneEventType::Enter)
        {
            // Note: ConditionId checking against a condition system is a future enhancement
            OnZoneEnter.Broadcast(ZoneId, Evt.EventId, Actor);
        }
    }
}

void UMcpZoneSubsystem::NotifyActorExitedZone(const FString& ZoneId, AActor* Actor)
{
    if (ZoneId.IsEmpty() || !Actor)
    {
        return;
    }
    
    const FMcpZoneDefinition* Zone = Zones.Find(ZoneId);
    if (!Zone)
    {
        UE_LOG(LogMcpZone, Warning, TEXT("NotifyActorExitedZone: Zone '%s' not found"), *ZoneId);
        return;
    }
    
    UE_LOG(LogMcpZone, Log, TEXT("Actor '%s' exited zone '%s'"),
        *Actor->GetName(), *ZoneId);
    
    // Broadcast for each exit event
    for (const FMcpZoneEvent& Evt : Zone->Events)
    {
        if (Evt.EventType == EMcpZoneEventType::Exit)
        {
            // Note: ConditionId checking against a condition system is a future enhancement
            OnZoneExit.Broadcast(ZoneId, Evt.EventId, Actor);
        }
    }
}
