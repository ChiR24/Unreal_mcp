// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "McpActorIdRegistrySubsystem.generated.h"

/**
 * UMcpActorIdRegistrySubsystem
 * 
 * World subsystem that maintains a registry of actors with McpId tags for O(1) lookup.
 * Used by gameplay primitives to efficiently find actors by stable identifier.
 * 
 * Actors are identified by tags in the format "McpId:UniqueId" (e.g., "McpId:player_spawn_01").
 */
UCLASS()
class MCPAUTOMATIONBRIDGE_API UMcpActorIdRegistrySubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    /** Initialize the subsystem */
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    
    /** Cleanup on shutdown */
    virtual void Deinitialize() override;
    
    /** Check if this subsystem should be created for the given world */
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

    /**
     * Register an actor with a specific McpId.
     * Called automatically for actors with McpId: tags on spawn.
     * 
     * @param Actor The actor to register
     * @param McpId The unique identifier (without "McpId:" prefix)
     */
    void RegisterActor(AActor* Actor, const FString& McpId);
    
    /**
     * Unregister an actor by its McpId.
     * Called automatically when actor is destroyed.
     * 
     * @param McpId The unique identifier to unregister
     */
    void UnregisterActor(const FString& McpId);
    
    /**
     * Find an actor by its McpId. O(1) lookup.
     * 
     * @param McpId The unique identifier (without "McpId:" prefix)
     * @return The actor if found, nullptr otherwise
     */
    AActor* FindByMcpId(const FString& McpId) const;
    
    /**
     * Get all registered McpIds.
     * 
     * @param OutIds Array to fill with registered IDs
     */
    void GetAllMcpIds(TArray<FString>& OutIds) const;
    
    /**
     * Get the McpId for an actor, if it has one.
     * 
     * @param Actor The actor to query
     * @return The McpId if found, empty string otherwise
     */
    static FString GetMcpIdFromActor(const AActor* Actor);

protected:
    /** Called when any actor is spawned in the world */
    void OnActorSpawned(AActor* Actor);
    
    /** Called when a registered actor is destroyed */
    UFUNCTION()
    void OnActorDestroyed(AActor* DestroyedActor);

private:
    /** Map of McpId -> Actor for O(1) lookup */
    TMap<FString, TWeakObjectPtr<AActor>> Registry;
    
    /** Delegate handle for actor spawn notifications */
    FDelegateHandle OnActorSpawnedHandle;
    
    /** Set of actors we're tracking destruction for */
    TSet<TWeakObjectPtr<AActor>> TrackedActors;
};
