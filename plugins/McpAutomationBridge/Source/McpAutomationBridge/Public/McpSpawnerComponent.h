// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "McpSpawnerComponent.generated.h"

// Delegate for spawn notifications
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMcpActorSpawned, AActor*, SpawnedActor, int32, CurrentCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMcpActorDespawned, AActor*, DespawnedActor, int32, CurrentCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMcpSpawnerEnabledChanged, bool, bEnabled);

/**
 * UMcpSpawnerComponent
 * 
 * Replicated actor component for spawning and managing entities.
 * 
 * Features:
 * - Network replication with OnRep callbacks
 * - Configurable spawn class, count limits, intervals
 * - Automatic spawning with radius-based random placement
 * - Weak pointer tracking for spawned actors
 * - Blueprint-bindable events for spawn/despawn
 * 
 * Use cases:
 * - Enemy spawners (respawning waves)
 * - Item/pickup dispensers
 * - Environmental hazard generators
 * - AI population management
 */
UCLASS(ClassGroup=(MCP), meta=(BlueprintSpawnableComponent))
class MCPAUTOMATIONBRIDGE_API UMcpSpawnerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UMcpSpawnerComponent();

    // ========== Replicated Properties ==========
    
    // Class to spawn - resolved from SpawnClassPath
    UPROPERTY(Transient, BlueprintReadOnly, Category = "Spawner")
    TSubclassOf<AActor> SpawnClass;

    // Replicated path for class lookup (clients resolve locally)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_SpawnClassPath, Category = "Spawner")
    FString SpawnClassPath;

    // Maximum simultaneous spawned actors
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Spawner", meta = (ClampMin = "1", ClampMax = "100"))
    int32 MaxSpawnCount = 5;

    // Seconds between spawn attempts
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Spawner", meta = (ClampMin = "0.1"))
    float SpawnInterval = 5.0f;

    // Random offset radius from spawner location
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Spawner", meta = (ClampMin = "0.0"))
    float SpawnRadius = 200.0f;

    // Whether spawning is enabled
    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_IsEnabled, Category = "Spawner")
    bool bIsEnabled = false;

    // Spawn immediately when enabled (vs waiting for first interval)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Spawner")
    bool bSpawnOnStart = true;

    // Current count of valid spawned actors (read-only, replicated for UI)
    UPROPERTY(BlueprintReadOnly, Replicated, Category = "Spawner")
    int32 CurrentSpawnedCount = 0;

    // JSON predicate for conditional spawning (future integration)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Spawner|Conditions")
    FString SpawnConditions;

    // ========== Delegates ==========
    
    UPROPERTY(BlueprintAssignable, Category = "Spawner|Events")
    FOnMcpActorSpawned OnActorSpawned;

    UPROPERTY(BlueprintAssignable, Category = "Spawner|Events")
    FOnMcpActorDespawned OnActorDespawned;

    UPROPERTY(BlueprintAssignable, Category = "Spawner|Events")
    FOnMcpSpawnerEnabledChanged OnEnabledChanged;

    // ========== Replication ==========
    
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION()
    void OnRep_SpawnClassPath();

    UFUNCTION()
    void OnRep_IsEnabled();

    // ========== Tick ==========
    
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========== Public API - Server-only mutations ==========
    
    /** Configure spawner with class path and parameters */
    UFUNCTION(BlueprintCallable, Category = "Spawner")
    void ConfigureSpawner(const FString& ClassPath, int32 MaxCount, float Interval, float Radius);

    /** Enable or disable spawning */
    UFUNCTION(BlueprintCallable, Category = "Spawner")
    void SetEnabled(bool bEnabled);

    /** Set spawn conditions JSON predicate */
    UFUNCTION(BlueprintCallable, Category = "Spawner")
    void SetSpawnConditions(const FString& ConditionsJson);

    /** Destroy all spawned actors */
    UFUNCTION(BlueprintCallable, Category = "Spawner")
    void DespawnAll();

    /** Manually trigger a single spawn attempt */
    UFUNCTION(BlueprintCallable, Category = "Spawner")
    AActor* SpawnOne();

    // ========== Query API ==========
    
    /** Get current count of valid spawned actors */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Spawner")
    int32 GetSpawnedCount() const;

    /** Get array of all valid spawned actors */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Spawner")
    TArray<AActor*> GetSpawnedActors() const;

    /** Check if spawner can spawn more actors */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Spawner")
    bool CanSpawn() const;

    /** Check if spawn class is valid */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Spawner")
    bool HasValidSpawnClass() const;

private:
    // ========== Server-only State ==========
    
    // Tracked spawned actors (cleaned periodically)
    TArray<TWeakObjectPtr<AActor>> SpawnedActors;

    // Time accumulator for spawn intervals
    float SpawnTimer = 0.0f;

    // Cleanup counter to periodically prune invalid weak pointers
    float CleanupTimer = 0.0f;
    static constexpr float CleanupInterval = 2.0f;

    // Flag to track if we need immediate spawn on enable
    bool bPendingInitialSpawn = false;

    // ========== Internal Helpers ==========
    
    /** Resolve SpawnClassPath to SpawnClass */
    void ResolveSpawnClass();

    /** Clean up invalid weak pointers from SpawnedActors */
    void CleanupInvalidActors();

    /** Update CurrentSpawnedCount after changes */
    void UpdateSpawnedCount();

    /** Check if spawn conditions are met (future: evaluate JSON predicate) */
    bool EvaluateSpawnConditions() const;

    /** Get random spawn location within radius */
    FVector GetRandomSpawnLocation() const;
};
