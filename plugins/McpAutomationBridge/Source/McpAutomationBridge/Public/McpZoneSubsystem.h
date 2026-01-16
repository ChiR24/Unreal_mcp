// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "McpZoneSubsystem.generated.h"

// Zone event types
UENUM(BlueprintType)
enum class EMcpZoneEventType : uint8
{
    Enter UMETA(DisplayName = "Enter"),
    Exit  UMETA(DisplayName = "Exit")
};

// Zone event struct
USTRUCT(BlueprintType)
struct FMcpZoneEvent
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString EventId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EMcpZoneEventType EventType = EMcpZoneEventType::Enter;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ConditionId;  // Optional condition to check
};

// Zone definition struct
USTRUCT(BlueprintType)
struct FMcpZoneDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ZoneId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TWeakObjectPtr<AActor> VolumeActor;  // Optional trigger volume

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FString, FString> Properties;  // Key-value properties

    UPROPERTY()
    TArray<FMcpZoneEvent> Events;
};

// Delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnMcpZoneEvent, const FString&, ZoneId, const FString&, EventId, AActor*, Actor);

/**
 * UMcpZoneSubsystem
 * 
 * World subsystem for managing named zones with:
 * - Zone definitions with properties
 * - Volume-based zone detection (optional)
 * - Enter/Exit events
 * - Actor zone queries
 */
UCLASS()
class MCPAUTOMATIONBRIDGE_API UMcpZoneSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

    // Zone Registry
    UPROPERTY()
    TMap<FString, FMcpZoneDefinition> Zones;

    // Events
    UPROPERTY(BlueprintAssignable)
    FOnMcpZoneEvent OnZoneEnter;

    UPROPERTY(BlueprintAssignable)
    FOnMcpZoneEvent OnZoneExit;

    // API
    UFUNCTION(BlueprintCallable, Category = "MCP Zone")
    bool CreateZone(const FString& ZoneId, const FString& DisplayName, AActor* VolumeActor = nullptr);

    UFUNCTION(BlueprintCallable, Category = "MCP Zone")
    bool SetZoneProperty(const FString& ZoneId, const FString& PropertyKey, const FString& PropertyValue);

    UFUNCTION(BlueprintCallable, Category = "MCP Zone")
    bool GetZoneProperty(const FString& ZoneId, const FString& PropertyKey, FString& OutValue) const;

    UFUNCTION(BlueprintCallable, Category = "MCP Zone")
    bool GetActorZone(AActor* Actor, FString& OutZoneId, FString& OutZoneName) const;

    UFUNCTION(BlueprintCallable, Category = "MCP Zone")
    bool AddZoneEnterEvent(const FString& ZoneId, const FString& EventId, const FString& ConditionId = TEXT(""));

    UFUNCTION(BlueprintCallable, Category = "MCP Zone")
    bool AddZoneExitEvent(const FString& ZoneId, const FString& EventId, const FString& ConditionId = TEXT(""));

    UFUNCTION(BlueprintCallable, Category = "MCP Zone")
    bool FindZone(const FString& ZoneId, FMcpZoneDefinition& OutZone);
    
    // Internal non-UFUNCTION pointer version for C++ use
    FMcpZoneDefinition* FindZoneInternal(const FString& ZoneId);

    UFUNCTION(BlueprintCallable, Category = "MCP Zone")
    TArray<FString> GetAllZoneIds() const;

    // Called by volumes or manually to trigger zone events
    void NotifyActorEnteredZone(const FString& ZoneId, AActor* Actor);
    void NotifyActorExitedZone(const FString& ZoneId, AActor* Actor);
};
