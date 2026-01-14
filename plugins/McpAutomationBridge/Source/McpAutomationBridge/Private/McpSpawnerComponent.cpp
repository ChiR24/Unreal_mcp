// Copyright Epic Games, Inc. All Rights Reserved.

#include "McpSpawnerComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpSpawner, Log, All);

UMcpSpawnerComponent::UMcpSpawnerComponent()
{
    // Enable replication by default
    SetIsReplicatedByDefault(true);
    
    // Enable ticking for spawn timer processing
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
    
    // Initialize defaults
    SpawnClass = nullptr;
    SpawnClassPath = TEXT("");
    MaxSpawnCount = 5;
    SpawnInterval = 5.0f;
    SpawnRadius = 200.0f;
    bIsEnabled = false;
    bSpawnOnStart = true;
    CurrentSpawnedCount = 0;
    SpawnConditions = TEXT("");
    SpawnTimer = 0.0f;
    CleanupTimer = 0.0f;
    bPendingInitialSpawn = false;
}

void UMcpSpawnerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    
    DOREPLIFETIME(UMcpSpawnerComponent, SpawnClassPath);
    DOREPLIFETIME(UMcpSpawnerComponent, MaxSpawnCount);
    DOREPLIFETIME(UMcpSpawnerComponent, SpawnInterval);
    DOREPLIFETIME(UMcpSpawnerComponent, SpawnRadius);
    DOREPLIFETIME(UMcpSpawnerComponent, bIsEnabled);
    DOREPLIFETIME(UMcpSpawnerComponent, bSpawnOnStart);
    DOREPLIFETIME(UMcpSpawnerComponent, CurrentSpawnedCount);
    DOREPLIFETIME(UMcpSpawnerComponent, SpawnConditions);
}

void UMcpSpawnerComponent::OnRep_SpawnClassPath()
{
    // Resolve class on client when path replicates
    ResolveSpawnClass();
    
    UE_LOG(LogMcpSpawner, Verbose, TEXT("Spawner class path replicated: '%s'"), *SpawnClassPath);
}

void UMcpSpawnerComponent::OnRep_IsEnabled()
{
    // Broadcast enabled state change
    OnEnabledChanged.Broadcast(bIsEnabled);
    
    UE_LOG(LogMcpSpawner, Verbose, TEXT("Spawner enabled state replicated: %s"), 
        bIsEnabled ? TEXT("true") : TEXT("false"));
}

void UMcpSpawnerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
    // Only process spawning on server
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority())
    {
        return;
    }
    
    // Periodic cleanup of invalid weak pointers
    CleanupTimer += DeltaTime;
    if (CleanupTimer >= CleanupInterval)
    {
        CleanupTimer = 0.0f;
        CleanupInvalidActors();
    }
    
    // Skip if spawning disabled
    if (!bIsEnabled)
    {
        return;
    }
    
    // Check if we have a valid spawn class
    if (!HasValidSpawnClass())
    {
        return;
    }
    
    // Handle initial spawn on enable
    if (bPendingInitialSpawn && bSpawnOnStart)
    {
        bPendingInitialSpawn = false;
        if (CanSpawn() && EvaluateSpawnConditions())
        {
            SpawnOne();
        }
    }
    
    // Process spawn timer
    SpawnTimer += DeltaTime;
    if (SpawnTimer >= SpawnInterval)
    {
        SpawnTimer = 0.0f;
        
        // Attempt to spawn if we can
        if (CanSpawn() && EvaluateSpawnConditions())
        {
            SpawnOne();
        }
    }
}

void UMcpSpawnerComponent::ConfigureSpawner(const FString& ClassPath, int32 MaxCount, float Interval, float Radius)
{
    // Only allow server to configure
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority())
    {
        UE_LOG(LogMcpSpawner, Warning, TEXT("ConfigureSpawner called on client - ignored"));
        return;
    }
    
    // Set spawn class path
    SpawnClassPath = ClassPath;
    ResolveSpawnClass();
    
    // Set parameters with validation
    MaxSpawnCount = FMath::Clamp(MaxCount, 1, 100);
    SpawnInterval = FMath::Max(0.1f, Interval);
    SpawnRadius = FMath::Max(0.0f, Radius);
    
    // Reset timer on reconfigure
    SpawnTimer = 0.0f;
    
    UE_LOG(LogMcpSpawner, Log, TEXT("Spawner configured: Class='%s', Max=%d, Interval=%.2fs, Radius=%.1f"), 
        *SpawnClassPath, MaxSpawnCount, SpawnInterval, SpawnRadius);
}

void UMcpSpawnerComponent::SetEnabled(bool bEnabled)
{
    // Only allow server to change enabled state
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority())
    {
        UE_LOG(LogMcpSpawner, Warning, TEXT("SetEnabled called on client - ignored"));
        return;
    }
    
    // Early out if no change
    if (bIsEnabled == bEnabled)
    {
        return;
    }
    
    bIsEnabled = bEnabled;
    
    // Reset timer when enabling
    if (bIsEnabled)
    {
        SpawnTimer = 0.0f;
        bPendingInitialSpawn = true;
    }
    
    // Broadcast event on server
    OnEnabledChanged.Broadcast(bIsEnabled);
    
    UE_LOG(LogMcpSpawner, Log, TEXT("Spawner %s"), bIsEnabled ? TEXT("enabled") : TEXT("disabled"));
}

void UMcpSpawnerComponent::SetSpawnConditions(const FString& ConditionsJson)
{
    // Only allow server to set conditions
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority())
    {
        UE_LOG(LogMcpSpawner, Warning, TEXT("SetSpawnConditions called on client - ignored"));
        return;
    }
    
    SpawnConditions = ConditionsJson;
    
    UE_LOG(LogMcpSpawner, Log, TEXT("Spawn conditions set: '%s'"), *SpawnConditions);
}

void UMcpSpawnerComponent::DespawnAll()
{
    // Only allow server to despawn
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority())
    {
        UE_LOG(LogMcpSpawner, Warning, TEXT("DespawnAll called on client - ignored"));
        return;
    }
    
    int32 DespawnCount = 0;
    
    for (TWeakObjectPtr<AActor>& WeakActor : SpawnedActors)
    {
        if (AActor* Actor = WeakActor.Get())
        {
            // Broadcast despawn event before destroying
            OnActorDespawned.Broadcast(Actor, SpawnedActors.Num() - DespawnCount - 1);
            
            Actor->Destroy();
            DespawnCount++;
        }
    }
    
    SpawnedActors.Empty();
    UpdateSpawnedCount();
    
    UE_LOG(LogMcpSpawner, Log, TEXT("Despawned %d actors"), DespawnCount);
}

AActor* UMcpSpawnerComponent::SpawnOne()
{
    // Only allow server to spawn
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority())
    {
        UE_LOG(LogMcpSpawner, Warning, TEXT("SpawnOne called on client - ignored"));
        return nullptr;
    }
    
    // Validate spawn class
    if (!HasValidSpawnClass())
    {
        UE_LOG(LogMcpSpawner, Warning, TEXT("SpawnOne failed: Invalid spawn class"));
        return nullptr;
    }
    
    // Check count limit
    CleanupInvalidActors();
    if (!CanSpawn())
    {
        UE_LOG(LogMcpSpawner, Verbose, TEXT("SpawnOne skipped: At max count (%d/%d)"), 
            CurrentSpawnedCount, MaxSpawnCount);
        return nullptr;
    }
    
    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogMcpSpawner, Warning, TEXT("SpawnOne failed: No world"));
        return nullptr;
    }
    
    // Configure spawn parameters
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = Owner;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    
    // Calculate spawn location with random offset
    FVector SpawnLoc = GetRandomSpawnLocation();
    
    // Spawn the actor
    AActor* NewActor = World->SpawnActor<AActor>(SpawnClass, SpawnLoc, FRotator::ZeroRotator, SpawnParams);
    
    if (!NewActor)
    {
        UE_LOG(LogMcpSpawner, Warning, TEXT("SpawnOne failed: SpawnActor returned null for class '%s'"), 
            *SpawnClassPath);
        return nullptr;
    }
    
    // Track the spawned actor
    SpawnedActors.Add(TWeakObjectPtr<AActor>(NewActor));
    UpdateSpawnedCount();
    
    // Broadcast spawn event
    OnActorSpawned.Broadcast(NewActor, CurrentSpawnedCount);
    
    UE_LOG(LogMcpSpawner, Log, TEXT("Spawned actor '%s' (%d/%d)"), 
        *NewActor->GetName(), CurrentSpawnedCount, MaxSpawnCount);
    
    return NewActor;
}

int32 UMcpSpawnerComponent::GetSpawnedCount() const
{
    // Return cached count for efficiency (updated on spawn/despawn)
    return CurrentSpawnedCount;
}

TArray<AActor*> UMcpSpawnerComponent::GetSpawnedActors() const
{
    TArray<AActor*> Result;
    Result.Reserve(SpawnedActors.Num());
    
    for (const TWeakObjectPtr<AActor>& WeakActor : SpawnedActors)
    {
        if (AActor* Actor = WeakActor.Get())
        {
            Result.Add(Actor);
        }
    }
    
    return Result;
}

bool UMcpSpawnerComponent::CanSpawn() const
{
    // Can spawn if we haven't reached max count
    return CurrentSpawnedCount < MaxSpawnCount;
}

bool UMcpSpawnerComponent::HasValidSpawnClass() const
{
    return SpawnClass != nullptr;
}

void UMcpSpawnerComponent::ResolveSpawnClass()
{
    if (SpawnClassPath.IsEmpty())
    {
        SpawnClass = nullptr;
        return;
    }
    
    // Attempt to load the class from path
    // Support both Blueprint class paths (ending in _C) and native class paths
    FString ClassPathToLoad = SpawnClassPath;
    if (!ClassPathToLoad.EndsWith(TEXT("_C")) && ClassPathToLoad.Contains(TEXT("/Game/")))
    {
        // Likely a Blueprint - append _C if needed
        ClassPathToLoad += TEXT("_C");
    }
    
    UClass* LoadedClass = LoadClass<AActor>(nullptr, *ClassPathToLoad);
    
    if (!LoadedClass)
    {
        // Try without _C suffix
        LoadedClass = LoadClass<AActor>(nullptr, *SpawnClassPath);
    }
    
    if (LoadedClass)
    {
        SpawnClass = LoadedClass;
        UE_LOG(LogMcpSpawner, Log, TEXT("Resolved spawn class: '%s' -> %s"), 
            *SpawnClassPath, *LoadedClass->GetName());
    }
    else
    {
        SpawnClass = nullptr;
        UE_LOG(LogMcpSpawner, Warning, TEXT("Failed to resolve spawn class: '%s'"), *SpawnClassPath);
    }
}

void UMcpSpawnerComponent::CleanupInvalidActors()
{
    int32 RemovedCount = 0;
    
    for (int32 i = SpawnedActors.Num() - 1; i >= 0; --i)
    {
        if (!SpawnedActors[i].IsValid())
        {
            SpawnedActors.RemoveAt(i);
            RemovedCount++;
        }
    }
    
    if (RemovedCount > 0)
    {
        UpdateSpawnedCount();
        UE_LOG(LogMcpSpawner, Verbose, TEXT("Cleaned up %d invalid actor references"), RemovedCount);
    }
}

void UMcpSpawnerComponent::UpdateSpawnedCount()
{
    int32 ValidCount = 0;
    
    for (const TWeakObjectPtr<AActor>& WeakActor : SpawnedActors)
    {
        if (WeakActor.IsValid())
        {
            ValidCount++;
        }
    }
    
    CurrentSpawnedCount = ValidCount;
}

bool UMcpSpawnerComponent::EvaluateSpawnConditions() const
{
    // If no conditions set, always allow spawning
    if (SpawnConditions.IsEmpty())
    {
        return true;
    }
    
    // Future: Parse and evaluate JSON predicate conditions
    // For now, any non-empty condition string is treated as "always true"
    // This allows the MCP to set conditions that can be checked externally
    // Note: Condition/rules system integration is a future enhancement
    return true;
}

FVector UMcpSpawnerComponent::GetRandomSpawnLocation() const
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return FVector::ZeroVector;
    }
    
    FVector SpawnLoc = Owner->GetActorLocation();
    
    if (SpawnRadius > 0.0f)
    {
        // Add random horizontal offset within radius
        FVector RandomOffset = FMath::VRand();
        RandomOffset.Z = 0.0f; // Keep horizontal
        RandomOffset.Normalize();
        RandomOffset *= FMath::FRandRange(0.0f, SpawnRadius);
        
        SpawnLoc += RandomOffset;
        // Keep same Z as owner
        SpawnLoc.Z = Owner->GetActorLocation().Z;
    }
    
    return SpawnLoc;
}
