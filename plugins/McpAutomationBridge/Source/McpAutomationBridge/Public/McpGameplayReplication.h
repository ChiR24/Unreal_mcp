// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Components/ActorComponent.h"
#include "Engine/NetSerialization.h"
#include "Net/UnrealNetwork.h"
#include "McpGameplayReplication.generated.h"

// ============================================================================
// WorldTime Replicated State (simple struct, no FastArray needed)
// ============================================================================

USTRUCT(BlueprintType)
struct MCPAUTOMATIONBRIDGE_API FMcpWorldTimeState
{
	GENERATED_BODY()

	/** Current time in hours (0-24) */
	UPROPERTY(BlueprintReadOnly, Category = "MCP World Time")
	float CurrentTime = 12.0f;

	/** Current day number */
	UPROPERTY(BlueprintReadOnly, Category = "MCP World Time")
	int32 Day = 1;

	/** Time scale multiplier */
	UPROPERTY(BlueprintReadOnly, Category = "MCP World Time")
	float TimeScale = 1.0f;

	/** Whether time is paused */
	UPROPERTY(BlueprintReadOnly, Category = "MCP World Time")
	bool bIsPaused = false;
};

// ============================================================================
// Faction FastArraySerializer Structures
// ============================================================================

/**
 * Single faction item for FastArraySerializer replication.
 * Relationships are encoded as "other_faction_id:relationship_value" strings
 * to avoid nested TMap replication complexity.
 */
USTRUCT(BlueprintType)
struct MCPAUTOMATIONBRIDGE_API FMcpFactionReplicationItem : public FFastArraySerializerItem
{
	GENERATED_BODY()

	/** Unique faction identifier */
	UPROPERTY(BlueprintReadOnly, Category = "MCP Faction")
	FString FactionId;

	/** Display name for UI */
	UPROPERTY(BlueprintReadOnly, Category = "MCP Faction")
	FString DisplayName;

	/** Faction color for UI/minimap */
	UPROPERTY(BlueprintReadOnly, Category = "MCP Faction")
	FLinearColor Color = FLinearColor::White;

	/** 
	 * Relationships encoded as "other_faction_id:relationship_value" strings.
	 * Relationship values: -2=Enemy, -1=Hostile, 0=Neutral, 1=Friendly, 2=Allied
	 */
	UPROPERTY(BlueprintReadOnly, Category = "MCP Faction")
	TArray<FString> Relationships;

	bool operator==(const FMcpFactionReplicationItem& Other) const
	{
		return FactionId == Other.FactionId;
	}
};

/**
 * FastArraySerializer container for faction replication.
 * Uses delta serialization for bandwidth-efficient updates.
 */
USTRUCT(BlueprintType)
struct MCPAUTOMATIONBRIDGE_API FMcpFactionReplicationArray : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FMcpFactionReplicationItem> Items;

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FMcpFactionReplicationItem, FMcpFactionReplicationArray>(Items, DeltaParms, *this);
	}

	/** Find faction by ID, returns nullptr if not found */
	FMcpFactionReplicationItem* FindByFactionId(const FString& FactionId)
	{
		for (auto& Item : Items)
		{
			if (Item.FactionId == FactionId)
			{
				return &Item;
			}
		}
		return nullptr;
	}

	/** Add or update faction, marks dirty for replication */
	void AddOrUpdate(const FMcpFactionReplicationItem& NewItem)
	{
		for (int32 i = 0; i < Items.Num(); ++i)
		{
			if (Items[i].FactionId == NewItem.FactionId)
			{
				Items[i] = NewItem;
				MarkItemDirty(Items[i]);
				return;
			}
		}
		Items.Add(NewItem);
		MarkItemDirty(Items.Last());
	}

	/** Remove faction by ID */
	bool RemoveByFactionId(const FString& FactionId)
	{
		for (int32 i = Items.Num() - 1; i >= 0; --i)
		{
			if (Items[i].FactionId == FactionId)
			{
				Items.RemoveAt(i);
				MarkArrayDirty();
				return true;
			}
		}
		return false;
	}
};

template<>
struct TStructOpsTypeTraits<FMcpFactionReplicationArray> : public TStructOpsTypeTraitsBase2<FMcpFactionReplicationArray>
{
	enum { WithNetDeltaSerializer = true };
};

// ============================================================================
// Zone FastArraySerializer Structures
// ============================================================================

/**
 * Single zone item for FastArraySerializer replication.
 */
USTRUCT(BlueprintType)
struct MCPAUTOMATIONBRIDGE_API FMcpZoneReplicationItem : public FFastArraySerializerItem
{
	GENERATED_BODY()

	/** Unique zone identifier */
	UPROPERTY(BlueprintReadOnly, Category = "MCP Zone")
	FString ZoneId;

	/** Display name for UI */
	UPROPERTY(BlueprintReadOnly, Category = "MCP Zone")
	FString DisplayName;

	/** Zone bounding box (world space) */
	UPROPERTY(BlueprintReadOnly, Category = "MCP Zone")
	FBox Bounds;

	/** 
	 * Zone properties as key-value pairs.
	 * Note: TMap replication requires special handling; for simple cases this works.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "MCP Zone")
	TMap<FString, FString> Properties;

	FMcpZoneReplicationItem()
		: Bounds(ForceInit)
	{
	}

	bool operator==(const FMcpZoneReplicationItem& Other) const
	{
		return ZoneId == Other.ZoneId;
	}
};

/**
 * FastArraySerializer container for zone replication.
 * Uses delta serialization for bandwidth-efficient updates.
 */
USTRUCT(BlueprintType)
struct MCPAUTOMATIONBRIDGE_API FMcpZoneReplicationArray : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FMcpZoneReplicationItem> Items;

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FMcpZoneReplicationItem, FMcpZoneReplicationArray>(Items, DeltaParms, *this);
	}

	/** Find zone by ID, returns nullptr if not found */
	FMcpZoneReplicationItem* FindByZoneId(const FString& ZoneId)
	{
		for (auto& Item : Items)
		{
			if (Item.ZoneId == ZoneId)
			{
				return &Item;
			}
		}
		return nullptr;
	}

	/** Add or update zone, marks dirty for replication */
	void AddOrUpdate(const FMcpZoneReplicationItem& NewItem)
	{
		for (int32 i = 0; i < Items.Num(); ++i)
		{
			if (Items[i].ZoneId == NewItem.ZoneId)
			{
				Items[i] = NewItem;
				MarkItemDirty(Items[i]);
				return;
			}
		}
		Items.Add(NewItem);
		MarkItemDirty(Items.Last());
	}

	/** Remove zone by ID */
	bool RemoveByZoneId(const FString& ZoneId)
	{
		for (int32 i = Items.Num() - 1; i >= 0; --i)
		{
			if (Items[i].ZoneId == ZoneId)
			{
				Items.RemoveAt(i);
				MarkArrayDirty();
				return true;
			}
		}
		return false;
	}
};

template<>
struct TStructOpsTypeTraits<FMcpZoneReplicationArray> : public TStructOpsTypeTraitsBase2<FMcpZoneReplicationArray>
{
	enum { WithNetDeltaSerializer = true };
};

// ============================================================================
// UMcpGameplayStateComponent - Replicated State Component
// ============================================================================

/**
 * UMcpGameplayStateComponent
 * 
 * Actor component that holds replicated gameplay state from MCP subsystems.
 * Attach to GameState for automatic replication to all clients.
 * 
 * Subsystems update this component's data, which then replicates:
 * - McpWorldTimeSubsystem -> WorldTimeState
 * - McpFactionSubsystem -> Factions
 * - McpZoneSubsystem -> Zones
 */
UCLASS(ClassGroup=(MCP), meta=(BlueprintSpawnableComponent))
class MCPAUTOMATIONBRIDGE_API UMcpGameplayStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMcpGameplayStateComponent();

	// ========================================================================
	// Replicated State
	// ========================================================================

	/** Replicated world time state (time of day, paused, etc.) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "MCP Gameplay State")
	FMcpWorldTimeState WorldTimeState;

	/** Replicated faction definitions with relationships (FastArraySerializer) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "MCP Gameplay State")
	FMcpFactionReplicationArray Factions;

	/** Replicated zone definitions with properties (FastArraySerializer) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "MCP Gameplay State")
	FMcpZoneReplicationArray Zones;

	// ========================================================================
	// Replication
	// ========================================================================

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ========================================================================
	// Authority Helpers (for subsystems to update state)
	// ========================================================================

	/** Update world time state (call from server/authority only) */
	UFUNCTION(BlueprintCallable, Category = "MCP Gameplay State", meta = (BlueprintProtected))
	void SetWorldTimeState(const FMcpWorldTimeState& NewState);

	/** Update a single faction (call from server/authority only) */
	UFUNCTION(BlueprintCallable, Category = "MCP Gameplay State", meta = (BlueprintProtected))
	void UpdateFaction(const FMcpFactionReplicationItem& FactionData);

	/** Remove a faction by ID (call from server/authority only) */
	UFUNCTION(BlueprintCallable, Category = "MCP Gameplay State", meta = (BlueprintProtected))
	void RemoveFaction(const FString& FactionId);

	/** Update a single zone (call from server/authority only) */
	UFUNCTION(BlueprintCallable, Category = "MCP Gameplay State", meta = (BlueprintProtected))
	void UpdateZone(const FMcpZoneReplicationItem& ZoneData);

	/** Remove a zone by ID (call from server/authority only) */
	UFUNCTION(BlueprintCallable, Category = "MCP Gameplay State", meta = (BlueprintProtected))
	void RemoveZone(const FString& ZoneId);
};

// ============================================================================
// AMcpGameState - GameState with MCP Component
// ============================================================================

/**
 * AMcpGameState
 * 
 * GameState subclass with UMcpGameplayStateComponent as default subobject.
 * Set this as your project's GameStateClass for automatic MCP state replication.
 * 
 * For projects using their own GameState, add UMcpGameplayStateComponent manually
 * and use GetMcpState() helper to access it.
 */
UCLASS()
class MCPAUTOMATIONBRIDGE_API AMcpGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AMcpGameState();

	/** MCP gameplay state component (replicated) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MCP")
	TObjectPtr<UMcpGameplayStateComponent> McpState;
};

// ============================================================================
// Global Helper Function
// ============================================================================

/**
 * Get the MCP gameplay state component from the current world's GameState.
 * 
 * @param World The world to query (typically GetWorld())
 * @return The MCP state component, or nullptr if not available
 * 
 * Usage:
 *   if (UMcpGameplayStateComponent* McpState = GetMcpState(GetWorld()))
 *   {
 *       float CurrentTime = McpState->WorldTimeState.CurrentTime;
 *   }
 */
MCPAUTOMATIONBRIDGE_API UMcpGameplayStateComponent* GetMcpState(UWorld* World);
