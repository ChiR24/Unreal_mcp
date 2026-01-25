// Copyright Epic Games, Inc. All Rights Reserved.

#include "McpFactionSubsystem.h"
#include "Math/UnrealMathUtility.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpFaction, Log, All);

void UMcpFactionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    
    UE_LOG(LogMcpFaction, Log, TEXT("MCP Faction Subsystem initialized"));
}

void UMcpFactionSubsystem::Deinitialize()
{
    Factions.Empty();
    Relationships.Empty();
    ActorFactions.Empty();
    ActorReputations.Empty();
    
    UE_LOG(LogMcpFaction, Log, TEXT("MCP Faction Subsystem deinitialized"));
    
    Super::Deinitialize();
}

bool UMcpFactionSubsystem::ShouldCreateSubsystem(UObject* Outer) const
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


bool UMcpFactionSubsystem::CreateFaction(const FString& FactionId, const FString& DisplayName, FLinearColor Color)
{
    if (FactionId.IsEmpty())
    {
        UE_LOG(LogMcpFaction, Warning, TEXT("CreateFaction: FactionId cannot be empty"));
        return false;
    }
    
    if (Factions.Contains(FactionId))
    {
        UE_LOG(LogMcpFaction, Warning, TEXT("CreateFaction: Faction '%s' already exists"), *FactionId);
        return false;
    }
    
    FMcpFactionDefinition NewFaction;
    NewFaction.FactionId = FactionId;
    NewFaction.DisplayName = DisplayName.IsEmpty() ? FactionId : DisplayName;
    NewFaction.Color = Color;
    
    Factions.Add(FactionId, NewFaction);
    
    UE_LOG(LogMcpFaction, Log, TEXT("Created faction '%s' (%s)"), *FactionId, *NewFaction.DisplayName);
    
    return true;
}

FString UMcpFactionSubsystem::MakeRelationshipKey(const FString& FactionA, const FString& FactionB) const
{
    // Create a sorted key for bidirectional lookup
    if (FactionA < FactionB)
    {
        return FactionA + TEXT("_") + FactionB;
    }
    return FactionB + TEXT("_") + FactionA;
}

bool UMcpFactionSubsystem::SetFactionRelationship(const FString& FactionA, const FString& FactionB, EMcpFactionRelationship Relationship, bool bBidirectional)
{
    if (FactionA.IsEmpty() || FactionB.IsEmpty())
    {
        UE_LOG(LogMcpFaction, Warning, TEXT("SetFactionRelationship: Faction IDs cannot be empty"));
        return false;
    }
    
    if (FactionA == FactionB)
    {
        UE_LOG(LogMcpFaction, Warning, TEXT("SetFactionRelationship: Cannot set relationship of faction with itself"));
        return false;
    }
    
    if (!Factions.Contains(FactionA))
    {
        UE_LOG(LogMcpFaction, Warning, TEXT("SetFactionRelationship: Faction '%s' does not exist"), *FactionA);
        return false;
    }
    
    if (!Factions.Contains(FactionB))
    {
        UE_LOG(LogMcpFaction, Warning, TEXT("SetFactionRelationship: Faction '%s' does not exist"), *FactionB);
        return false;
    }
    
    if (bBidirectional)
    {
        // Use sorted key for bidirectional relationships
        FString Key = MakeRelationshipKey(FactionA, FactionB);
        Relationships.Add(Key, Relationship);
        
        UE_LOG(LogMcpFaction, Log, TEXT("Set bidirectional relationship: %s <-> %s = %d"),
            *FactionA, *FactionB, static_cast<int32>(Relationship));
    }
    else
    {
        // Use directional key: A_to_B
        FString Key = FactionA + TEXT("_to_") + FactionB;
        Relationships.Add(Key, Relationship);
        
        UE_LOG(LogMcpFaction, Log, TEXT("Set directional relationship: %s -> %s = %d"),
            *FactionA, *FactionB, static_cast<int32>(Relationship));
    }
    
    return true;
}

bool UMcpFactionSubsystem::AssignToFaction(const FString& ActorId, const FString& FactionId)
{
    if (ActorId.IsEmpty())
    {
        UE_LOG(LogMcpFaction, Warning, TEXT("AssignToFaction: ActorId cannot be empty"));
        return false;
    }
    
    if (FactionId.IsEmpty())
    {
        // Empty faction means remove from faction
        ActorFactions.Remove(ActorId);
        UE_LOG(LogMcpFaction, Log, TEXT("Removed actor '%s' from faction"), *ActorId);
        return true;
    }
    
    if (!Factions.Contains(FactionId))
    {
        UE_LOG(LogMcpFaction, Warning, TEXT("AssignToFaction: Faction '%s' does not exist"), *FactionId);
        return false;
    }
    
    ActorFactions.Add(ActorId, FactionId);
    
    UE_LOG(LogMcpFaction, Log, TEXT("Assigned actor '%s' to faction '%s'"), *ActorId, *FactionId);
    
    return true;
}

bool UMcpFactionSubsystem::GetFaction(const FString& ActorId, FString& OutFactionId, FMcpFactionDefinition& OutFaction) const
{
    if (ActorId.IsEmpty())
    {
        return false;
    }
    
    const FString* FoundFactionId = ActorFactions.Find(ActorId);
    if (!FoundFactionId)
    {
        return false;
    }
    
    OutFactionId = *FoundFactionId;
    
    const FMcpFactionDefinition* FoundFaction = Factions.Find(OutFactionId);
    if (FoundFaction)
    {
        OutFaction = *FoundFaction;
    }
    
    return true;
}

bool UMcpFactionSubsystem::ModifyReputation(const FString& ActorId, const FString& FactionId, float Delta, float MinRep, float MaxRep)
{
    if (ActorId.IsEmpty() || FactionId.IsEmpty())
    {
        UE_LOG(LogMcpFaction, Warning, TEXT("ModifyReputation: ActorId and FactionId cannot be empty"));
        return false;
    }
    
    if (!Factions.Contains(FactionId))
    {
        UE_LOG(LogMcpFaction, Warning, TEXT("ModifyReputation: Faction '%s' does not exist"), *FactionId);
        return false;
    }
    
    // Get or create actor reputation entry
    FMcpActorReputation& ActorRep = ActorReputations.FindOrAdd(ActorId);
    ActorRep.ActorId = ActorId;
    
    // Get current reputation (default 0)
    float OldRep = 0.0f;
    if (float* CurrentRep = ActorRep.FactionReputations.Find(FactionId))
    {
        OldRep = *CurrentRep;
    }
    
    // Apply delta and clamp
    float NewRep = FMath::Clamp(OldRep + Delta, MinRep, MaxRep);
    ActorRep.FactionReputations.Add(FactionId, NewRep);
    
    UE_LOG(LogMcpFaction, Log, TEXT("Modified reputation: Actor='%s', Faction='%s', %.2f -> %.2f (Delta=%.2f)"),
        *ActorId, *FactionId, OldRep, NewRep, Delta);
    
    // Broadcast reputation change
    OnReputationChanged.Broadcast(ActorId, FactionId, NewRep);
    
    // Check for threshold crossings
    CheckReputationThresholds(ActorId, FactionId, OldRep, NewRep);
    
    return true;
}

bool UMcpFactionSubsystem::GetReputation(const FString& ActorId, const FString& FactionId, float& OutReputation) const
{
    OutReputation = 0.0f;
    
    if (ActorId.IsEmpty() || FactionId.IsEmpty())
    {
        return false;
    }
    
    const FMcpActorReputation* ActorRep = ActorReputations.Find(ActorId);
    if (!ActorRep)
    {
        // No reputation record, default is 0
        return true;
    }
    
    const float* Rep = ActorRep->FactionReputations.Find(FactionId);
    if (Rep)
    {
        OutReputation = *Rep;
    }
    
    return true;
}

bool UMcpFactionSubsystem::AddReputationThreshold(const FString& ActorId, const FString& FactionId, float ThresholdValue, const FString& Direction, const FString& EventId)
{
    if (ActorId.IsEmpty() || FactionId.IsEmpty() || EventId.IsEmpty())
    {
        UE_LOG(LogMcpFaction, Warning, TEXT("AddReputationThreshold: ActorId, FactionId, and EventId cannot be empty"));
        return false;
    }
    
    // Validate direction
    if (Direction != TEXT("above") && Direction != TEXT("below") && Direction != TEXT("crossing"))
    {
        UE_LOG(LogMcpFaction, Warning, TEXT("AddReputationThreshold: Invalid direction '%s'. Use 'above', 'below', or 'crossing'"), *Direction);
        return false;
    }
    
    // Get or create actor reputation entry
    FMcpActorReputation& ActorRep = ActorReputations.FindOrAdd(ActorId);
    ActorRep.ActorId = ActorId;
    
    // Get or create threshold array for this faction
    TArray<FMcpReputationThreshold>& Thresholds = ActorRep.FactionThresholds.FindOrAdd(FactionId);
    
    // Check for duplicate
    for (const FMcpReputationThreshold& Existing : Thresholds)
    {
        if (Existing.EventId == EventId)
        {
            UE_LOG(LogMcpFaction, Warning, TEXT("AddReputationThreshold: Threshold with EventId '%s' already exists"), *EventId);
            return false;
        }
    }
    
    FMcpReputationThreshold NewThreshold;
    NewThreshold.Value = ThresholdValue;
    NewThreshold.Direction = Direction;
    NewThreshold.EventId = EventId;
    NewThreshold.bHasTriggered = false;
    
    Thresholds.Add(NewThreshold);
    
    UE_LOG(LogMcpFaction, Log, TEXT("Added reputation threshold: Actor='%s', Faction='%s', Value=%.2f, Direction='%s', EventId='%s'"),
        *ActorId, *FactionId, ThresholdValue, *Direction, *EventId);
    
    return true;
}

void UMcpFactionSubsystem::CheckReputationThresholds(const FString& ActorId, const FString& FactionId, float OldRep, float NewRep)
{
    FMcpActorReputation* ActorRep = ActorReputations.Find(ActorId);
    if (!ActorRep)
    {
        return;
    }
    
    TArray<FMcpReputationThreshold>* Thresholds = ActorRep->FactionThresholds.Find(FactionId);
    if (!Thresholds)
    {
        return;
    }
    
    for (FMcpReputationThreshold& Threshold : *Thresholds)
    {
        bool bShouldTrigger = false;
        
        if (Threshold.Direction == TEXT("above"))
        {
            // Trigger when crossing from below to above
            bShouldTrigger = (OldRep < Threshold.Value && NewRep >= Threshold.Value);
        }
        else if (Threshold.Direction == TEXT("below"))
        {
            // Trigger when crossing from above to below
            bShouldTrigger = (OldRep > Threshold.Value && NewRep <= Threshold.Value);
        }
        else if (Threshold.Direction == TEXT("crossing"))
        {
            // Trigger on any crossing
            bShouldTrigger = (OldRep < Threshold.Value && NewRep >= Threshold.Value) ||
                            (OldRep > Threshold.Value && NewRep <= Threshold.Value);
        }
        
        if (bShouldTrigger && !Threshold.bHasTriggered)
        {
            Threshold.bHasTriggered = true;
            
            UE_LOG(LogMcpFaction, Log, TEXT("Reputation threshold crossed: Actor='%s', Faction='%s', Threshold=%.2f, EventId='%s'"),
                *ActorId, *FactionId, Threshold.Value, *Threshold.EventId);
            
            OnReputationThresholdCrossed.Broadcast(ActorId, FactionId, Threshold.Value);
        }
        else if (!bShouldTrigger && Threshold.Direction == TEXT("crossing"))
        {
            // Reset trigger for crossing thresholds when moving away
            Threshold.bHasTriggered = false;
        }
    }
}

bool UMcpFactionSubsystem::CheckFactionRelationship(const FString& ActorIdA, const FString& ActorIdB, EMcpFactionRelationship& OutRelationship, bool& bIsFriendly, bool& bIsHostile) const
{
    OutRelationship = EMcpFactionRelationship::Neutral;
    bIsFriendly = false;
    bIsHostile = false;
    
    if (ActorIdA.IsEmpty() || ActorIdB.IsEmpty())
    {
        return false;
    }
    
    // Get factions for both actors
    const FString* FactionA = ActorFactions.Find(ActorIdA);
    const FString* FactionB = ActorFactions.Find(ActorIdB);
    
    if (!FactionA || !FactionB)
    {
        // One or both actors don't have a faction - neutral
        return true;
    }
    
    if (*FactionA == *FactionB)
    {
        // Same faction = Friendly
        OutRelationship = EMcpFactionRelationship::Friendly;
        bIsFriendly = true;
        return true;
    }
    
    // Get relationship between factions
    OutRelationship = GetRelationshipBetweenFactions(*FactionA, *FactionB);
    
    // Determine friendly/hostile flags
    bIsFriendly = (OutRelationship == EMcpFactionRelationship::Friendly || 
                   OutRelationship == EMcpFactionRelationship::Allied);
    bIsHostile = (OutRelationship == EMcpFactionRelationship::Hostile || 
                  OutRelationship == EMcpFactionRelationship::Enemy);
    
    return true;
}

EMcpFactionRelationship UMcpFactionSubsystem::GetRelationshipBetweenFactions(const FString& FactionA, const FString& FactionB) const
{
    if (FactionA.IsEmpty() || FactionB.IsEmpty())
    {
        return EMcpFactionRelationship::Neutral;
    }
    
    if (FactionA == FactionB)
    {
        return EMcpFactionRelationship::Friendly;
    }
    
    // First check for directional relationship (A -> B)
    FString DirectionalKey = FactionA + TEXT("_to_") + FactionB;
    if (const EMcpFactionRelationship* Relationship = Relationships.Find(DirectionalKey))
    {
        return *Relationship;
    }
    
    // Then check for bidirectional relationship
    FString BidirectionalKey = MakeRelationshipKey(FactionA, FactionB);
    if (const EMcpFactionRelationship* Relationship = Relationships.Find(BidirectionalKey))
    {
        return *Relationship;
    }
    
    // Default to neutral
    return EMcpFactionRelationship::Neutral;
}

TArray<FString> UMcpFactionSubsystem::GetAllFactionIds() const
{
    TArray<FString> Result;
    Factions.GetKeys(Result);
    return Result;
}
