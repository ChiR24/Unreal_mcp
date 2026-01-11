// Copyright Epic Games, Inc. All Rights Reserved.

#include "McpGameplayReplication.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpGameplayReplication, Log, All);

// ============================================================================
// UMcpGameplayStateComponent Implementation
// ============================================================================

UMcpGameplayStateComponent::UMcpGameplayStateComponent()
{
	// Enable replication by default
	SetIsReplicatedByDefault(true);
	
	// This component doesn't need to tick
	PrimaryComponentTick.bCanEverTick = false;
	
	// Initialize default world time state
	WorldTimeState.CurrentTime = 12.0f;
	WorldTimeState.Day = 1;
	WorldTimeState.TimeScale = 1.0f;
	WorldTimeState.bIsPaused = false;
}

void UMcpGameplayStateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Replicate all state to all clients
	DOREPLIFETIME(UMcpGameplayStateComponent, WorldTimeState);
	DOREPLIFETIME(UMcpGameplayStateComponent, Factions);
	DOREPLIFETIME(UMcpGameplayStateComponent, Zones);
}

void UMcpGameplayStateComponent::SetWorldTimeState(const FMcpWorldTimeState& NewState)
{
	// Only authority can update state
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(LogMcpGameplayReplication, Warning, TEXT("SetWorldTimeState called on non-authority client - ignoring"));
		return;
	}

	WorldTimeState = NewState;
	
	UE_LOG(LogMcpGameplayReplication, Verbose, 
		TEXT("WorldTimeState updated: Time=%.2f, Day=%d, Scale=%.2f, Paused=%s"),
		WorldTimeState.CurrentTime, WorldTimeState.Day, WorldTimeState.TimeScale,
		WorldTimeState.bIsPaused ? TEXT("true") : TEXT("false"));
}

void UMcpGameplayStateComponent::UpdateFaction(const FMcpFactionReplicationItem& FactionData)
{
	// Only authority can update state
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(LogMcpGameplayReplication, Warning, TEXT("UpdateFaction called on non-authority client - ignoring"));
		return;
	}

	Factions.AddOrUpdate(FactionData);
	
	UE_LOG(LogMcpGameplayReplication, Verbose, 
		TEXT("Faction updated: ID=%s, Name=%s, Relationships=%d"),
		*FactionData.FactionId, *FactionData.DisplayName, FactionData.Relationships.Num());
}

void UMcpGameplayStateComponent::RemoveFaction(const FString& FactionId)
{
	// Only authority can update state
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(LogMcpGameplayReplication, Warning, TEXT("RemoveFaction called on non-authority client - ignoring"));
		return;
	}

	if (Factions.RemoveByFactionId(FactionId))
	{
		UE_LOG(LogMcpGameplayReplication, Verbose, TEXT("Faction removed: ID=%s"), *FactionId);
	}
	else
	{
		UE_LOG(LogMcpGameplayReplication, Warning, TEXT("RemoveFaction: Faction '%s' not found"), *FactionId);
	}
}

void UMcpGameplayStateComponent::UpdateZone(const FMcpZoneReplicationItem& ZoneData)
{
	// Only authority can update state
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(LogMcpGameplayReplication, Warning, TEXT("UpdateZone called on non-authority client - ignoring"));
		return;
	}

	Zones.AddOrUpdate(ZoneData);
	
	UE_LOG(LogMcpGameplayReplication, Verbose, 
		TEXT("Zone updated: ID=%s, Name=%s, Properties=%d"),
		*ZoneData.ZoneId, *ZoneData.DisplayName, ZoneData.Properties.Num());
}

void UMcpGameplayStateComponent::RemoveZone(const FString& ZoneId)
{
	// Only authority can update state
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(LogMcpGameplayReplication, Warning, TEXT("RemoveZone called on non-authority client - ignoring"));
		return;
	}

	if (Zones.RemoveByZoneId(ZoneId))
	{
		UE_LOG(LogMcpGameplayReplication, Verbose, TEXT("Zone removed: ID=%s"), *ZoneId);
	}
	else
	{
		UE_LOG(LogMcpGameplayReplication, Warning, TEXT("RemoveZone: Zone '%s' not found"), *ZoneId);
	}
}

// ============================================================================
// AMcpGameState Implementation
// ============================================================================

AMcpGameState::AMcpGameState()
{
	// Create the MCP state component as a default subobject
	McpState = CreateDefaultSubobject<UMcpGameplayStateComponent>(TEXT("McpState"));
	
	// GameState is always replicated
	bReplicates = true;
	bAlwaysRelevant = true;
	
	UE_LOG(LogMcpGameplayReplication, Log, TEXT("AMcpGameState created with McpState component"));
}

// ============================================================================
// Global Helper Function Implementation
// ============================================================================

UMcpGameplayStateComponent* GetMcpState(UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}

	AGameStateBase* GameState = World->GetGameState();
	if (!GameState)
	{
		return nullptr;
	}

	// Try our custom GameState first (most common case when using AMcpGameState)
	if (AMcpGameState* McpGameState = Cast<AMcpGameState>(GameState))
	{
		return McpGameState->McpState;
	}

	// Fallback: look for the component on any GameState
	// This supports projects that add UMcpGameplayStateComponent to their own GameState
	return GameState->FindComponentByClass<UMcpGameplayStateComponent>();
}

// ============================================================================
// Utility Functions for Subsystem Integration
// ============================================================================

namespace McpReplicationHelpers
{
	/**
	 * Encode a faction relationship value as a string.
	 * Format: "faction_id:value"
	 * Values: -2=Enemy, -1=Hostile, 0=Neutral, 1=Friendly, 2=Allied
	 */
	FString EncodeRelationship(const FString& OtherFactionId, int32 RelationshipValue)
	{
		return FString::Printf(TEXT("%s:%d"), *OtherFactionId, RelationshipValue);
	}

	/**
	 * Decode a relationship string back to faction ID and value.
	 * Returns true if successful.
	 */
	bool DecodeRelationship(const FString& EncodedRelationship, FString& OutFactionId, int32& OutValue)
	{
		int32 ColonIndex;
		if (!EncodedRelationship.FindChar(TEXT(':'), ColonIndex))
		{
			return false;
		}

		OutFactionId = EncodedRelationship.Left(ColonIndex);
		FString ValueStr = EncodedRelationship.Mid(ColonIndex + 1);
		OutValue = FCString::Atoi(*ValueStr);
		return true;
	}

	/**
	 * Get MCP state component with authority check.
	 * Returns nullptr if not on server/authority.
	 */
	UMcpGameplayStateComponent* GetMcpStateAuthority(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}

		// Check if we're the server/authority
		if (World->GetNetMode() == NM_Client)
		{
			return nullptr;
		}

		return GetMcpState(World);
	}

	/**
	 * Safely get MCP state, optionally creating GameState if needed.
	 * Useful during game initialization.
	 */
	UMcpGameplayStateComponent* GetOrWaitForMcpState(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}

		// Try to get existing state
		if (UMcpGameplayStateComponent* State = GetMcpState(World))
		{
			return State;
		}

		// GameState might not be spawned yet during early initialization
		// Callers should handle nullptr and try again later
		UE_LOG(LogMcpGameplayReplication, Verbose, 
			TEXT("GetOrWaitForMcpState: GameState not yet available in world '%s'"),
			*World->GetName());

		return nullptr;
	}

	/**
	 * Convert relationship enum to int value for replication.
	 */
	int32 RelationshipEnumToInt(int32 EnumValue)
	{
		// EMcpFactionRelationship: Neutral=0, Friendly=1, Allied=2, Hostile=3, Enemy=4
		// Replication values: -2=Enemy, -1=Hostile, 0=Neutral, 1=Friendly, 2=Allied
		switch (EnumValue)
		{
			case 0: return 0;   // Neutral -> 0
			case 1: return 1;   // Friendly -> 1
			case 2: return 2;   // Allied -> 2
			case 3: return -1;  // Hostile -> -1
			case 4: return -2;  // Enemy -> -2
			default: return 0;
		}
	}

	/**
	 * Convert replication int value back to relationship enum.
	 */
	int32 IntToRelationshipEnum(int32 IntValue)
	{
		// Replication values: -2=Enemy, -1=Hostile, 0=Neutral, 1=Friendly, 2=Allied
		// EMcpFactionRelationship: Neutral=0, Friendly=1, Allied=2, Hostile=3, Enemy=4
		switch (IntValue)
		{
			case 0: return 0;   // Neutral
			case 1: return 1;   // Friendly
			case 2: return 2;   // Allied
			case -1: return 3;  // Hostile
			case -2: return 4;  // Enemy
			default: return 0;
		}
	}

} // namespace McpReplicationHelpers
