// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "McpFactionSubsystem.generated.h"

// Faction relationship types
UENUM(BlueprintType)
enum class EMcpFactionRelationship : uint8
{
    Neutral  UMETA(DisplayName = "Neutral"),
    Friendly UMETA(DisplayName = "Friendly"),
    Allied   UMETA(DisplayName = "Allied"),
    Hostile  UMETA(DisplayName = "Hostile"),
    Enemy    UMETA(DisplayName = "Enemy")
};

// Reputation threshold struct
USTRUCT(BlueprintType)
struct FMcpReputationThreshold
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Value = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Direction;  // "above", "below", "crossing"

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString EventId;

    UPROPERTY()
    bool bHasTriggered = false;
};

// Faction definition
USTRUCT(BlueprintType)
struct FMcpFactionDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString FactionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor Color = FLinearColor::White;
};

// Actor reputation entry
USTRUCT(BlueprintType)
struct FMcpActorReputation
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ActorId;  // McpId or stable actor identifier

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FString, float> FactionReputations;  // FactionId -> Reputation value

    // Internal: TMap with TArray value not supported by UPROPERTY, so no reflection macro
    TMap<FString, TArray<FMcpReputationThreshold>> FactionThresholds;
};

// Delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnMcpReputationChanged, const FString&, ActorId, const FString&, FactionId, float, NewReputation);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnMcpReputationThresholdCrossed, const FString&, ActorId, const FString&, FactionId, float, ThresholdValue);

/**
 * UMcpFactionSubsystem
 * 
 * World subsystem for managing:
 * - Faction definitions
 * - Faction-to-faction relationships
 * - Actor-to-faction assignments
 * - Per-actor reputation with each faction
 */
UCLASS()
class MCPAUTOMATIONBRIDGE_API UMcpFactionSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

    // Faction Registry
    UPROPERTY()
    TMap<FString, FMcpFactionDefinition> Factions;

    // Faction Relationships (FactionA_FactionB -> Relationship)
    UPROPERTY()
    TMap<FString, EMcpFactionRelationship> Relationships;

    // Actor Factions (ActorId -> FactionId)
    UPROPERTY()
    TMap<FString, FString> ActorFactions;

    // Actor Reputations
    UPROPERTY()
    TMap<FString, FMcpActorReputation> ActorReputations;

    // Events
    UPROPERTY(BlueprintAssignable)
    FOnMcpReputationChanged OnReputationChanged;

    UPROPERTY(BlueprintAssignable)
    FOnMcpReputationThresholdCrossed OnReputationThresholdCrossed;

    // API
    UFUNCTION(BlueprintCallable, Category = "MCP Faction")
    bool CreateFaction(const FString& FactionId, const FString& DisplayName, FLinearColor Color = FLinearColor::White);

    UFUNCTION(BlueprintCallable, Category = "MCP Faction")
    bool SetFactionRelationship(const FString& FactionA, const FString& FactionB, EMcpFactionRelationship Relationship, bool bBidirectional = true);

    UFUNCTION(BlueprintCallable, Category = "MCP Faction")
    bool AssignToFaction(const FString& ActorId, const FString& FactionId);

    UFUNCTION(BlueprintCallable, Category = "MCP Faction")
    bool GetFaction(const FString& ActorId, FString& OutFactionId, FMcpFactionDefinition& OutFaction) const;

    UFUNCTION(BlueprintCallable, Category = "MCP Faction")
    bool ModifyReputation(const FString& ActorId, const FString& FactionId, float Delta, float MinRep = -100.0f, float MaxRep = 100.0f);

    UFUNCTION(BlueprintCallable, Category = "MCP Faction")
    bool GetReputation(const FString& ActorId, const FString& FactionId, float& OutReputation) const;

    UFUNCTION(BlueprintCallable, Category = "MCP Faction")
    bool AddReputationThreshold(const FString& ActorId, const FString& FactionId, float ThresholdValue, const FString& Direction, const FString& EventId);

    UFUNCTION(BlueprintCallable, Category = "MCP Faction")
    bool CheckFactionRelationship(const FString& ActorIdA, const FString& ActorIdB, EMcpFactionRelationship& OutRelationship, bool& bIsFriendly, bool& bIsHostile) const;

    UFUNCTION(BlueprintCallable, Category = "MCP Faction")
    EMcpFactionRelationship GetRelationshipBetweenFactions(const FString& FactionA, const FString& FactionB) const;

    UFUNCTION(BlueprintCallable, Category = "MCP Faction")
    TArray<FString> GetAllFactionIds() const;

private:
    FString MakeRelationshipKey(const FString& FactionA, const FString& FactionB) const;
    void CheckReputationThresholds(const FString& ActorId, const FString& FactionId, float OldRep, float NewRep);
};
