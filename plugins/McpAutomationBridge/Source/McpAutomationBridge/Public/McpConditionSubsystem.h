// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Dom/JsonObject.h"
#include "McpConditionSubsystem.generated.h"

// Condition listener entry
USTRUCT()
struct FMcpConditionListener
{
    GENERATED_BODY()

    UPROPERTY()
    FString ListenerId;

    UPROPERTY()
    FString ConditionId;

    UPROPERTY()
    bool bOneShot = false;

    UPROPERTY()
    bool bHasTriggered = false;
};

// Condition definition - stores JSON AST
USTRUCT()
struct FMcpConditionDefinition
{
    GENERATED_BODY()

    UPROPERTY()
    FString ConditionId;

    UPROPERTY()
    FString PredicateJson;  // Serialized JSON predicate AST

    // Cached parsed predicate (not serialized)
    TSharedPtr<FJsonObject> ParsedPredicate;
};

// Delegate for condition events
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMcpConditionTriggered, const FString&, ConditionId, bool, bResult);

/**
 * UMcpConditionSubsystem
 * 
 * Server-only condition system using structured JSON predicates.
 * NO STRING EVAL - conditions are parsed JSON ASTs.
 * 
 * Predicate types:
 * - "all": All child conditions must be true
 * - "any": Any child condition must be true
 * - "not": Negates child condition
 * - "compare": Compares two values (eq, neq, gt, gte, lt, lte)
 * 
 * Operand types:
 * - "const": Constant value
 * - "value_tracker": Query McpValueTrackerComponent
 * - "world_time": Query McpWorldTimeSubsystem
 * - "faction_reputation": Query McpFactionSubsystem
 * - "zone_membership": Query McpZoneSubsystem
 */
UCLASS()
class MCPAUTOMATIONBRIDGE_API UMcpConditionSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

    // Condition Registry
    UPROPERTY()
    TMap<FString, FMcpConditionDefinition> Conditions;

    // Listeners
    UPROPERTY()
    TArray<FMcpConditionListener> Listeners;

    // Events
    UPROPERTY(BlueprintAssignable)
    FOnMcpConditionTriggered OnConditionTriggered;

    // API
    UFUNCTION(BlueprintCallable, Category = "MCP Condition")
    bool CreateCondition(const FString& ConditionId, const FString& PredicateJson);

    UFUNCTION(BlueprintCallable, Category = "MCP Condition")
    bool CreateCompoundCondition(const FString& ConditionId, const FString& Operator, const TArray<FString>& ConditionIds);

    UFUNCTION(BlueprintCallable, Category = "MCP Condition")
    bool EvaluateCondition(const FString& ConditionId, bool& OutResult);

    UFUNCTION(BlueprintCallable, Category = "MCP Condition")
    bool AddConditionListener(const FString& ConditionId, const FString& ListenerId, bool bOneShot = false);

    UFUNCTION(BlueprintCallable, Category = "MCP Condition")
    bool RemoveConditionListener(const FString& ListenerId);

    UFUNCTION(BlueprintCallable, Category = "MCP Condition")
    TArray<FString> GetAllConditionIds() const;

private:
    // Recursive predicate evaluation
    bool EvaluatePredicate(const TSharedPtr<FJsonObject>& Predicate);
    
    // Operand value resolution
    float ResolveOperandValue(const TSharedPtr<FJsonObject>& Operand);
    FString ResolveOperandString(const TSharedPtr<FJsonObject>& Operand);
    
    // Comparison helpers
    bool CompareValues(float Left, float Right, const FString& Operator);

    // Notify listeners when condition is evaluated true
    void NotifyListeners(const FString& ConditionId, bool bResult);
};
