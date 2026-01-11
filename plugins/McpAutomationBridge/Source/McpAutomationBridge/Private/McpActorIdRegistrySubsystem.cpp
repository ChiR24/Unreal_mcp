// Copyright Epic Games, Inc. All Rights Reserved.

#include "McpActorIdRegistrySubsystem.h"
#include "EngineUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpActorIdRegistry, Log, All);

void UMcpActorIdRegistrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }
    
    // Register for actor spawn events
    OnActorSpawnedHandle = World->AddOnActorSpawnedHandler(
        FOnActorSpawned::FDelegate::CreateUObject(this, &UMcpActorIdRegistrySubsystem::OnActorSpawned));
    
    // Register all existing actors with McpId tags
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        OnActorSpawned(*It);
    }
    
    UE_LOG(LogMcpActorIdRegistry, Log, TEXT("MCP Actor ID Registry initialized with %d actors"), Registry.Num());
}

void UMcpActorIdRegistrySubsystem::Deinitialize()
{
    UWorld* World = GetWorld();
    
    // Remove spawn handler
    if (World && OnActorSpawnedHandle.IsValid())
    {
        World->RemoveOnActorSpawnedHandler(OnActorSpawnedHandle);
        OnActorSpawnedHandle.Reset();
    }
    
    // Remove all OnDestroyed bindings
    for (auto& Pair : OnDestroyedHandles)
    {
        if (AActor* Actor = Pair.Key.Get())
        {
            Actor->OnDestroyed.Remove(Pair.Value);
        }
    }
    OnDestroyedHandles.Empty();
    Registry.Empty();
    
    UE_LOG(LogMcpActorIdRegistry, Log, TEXT("MCP Actor ID Registry deinitialized"));
    
    Super::Deinitialize();
}

bool UMcpActorIdRegistrySubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    // Create for all world types (Editor, PIE, Game)
    return true;
}

void UMcpActorIdRegistrySubsystem::RegisterActor(AActor* Actor, const FString& McpId)
{
    if (!Actor || McpId.IsEmpty())
    {
        return;
    }
    
    // Check for duplicate registration
    if (TWeakObjectPtr<AActor>* Existing = Registry.Find(McpId))
    {
        if (Existing->IsValid() && Existing->Get() != Actor)
        {
            UE_LOG(LogMcpActorIdRegistry, Warning, 
                TEXT("McpId '%s' already registered to actor '%s', overwriting with '%s'"),
                *McpId, *Existing->Get()->GetName(), *Actor->GetName());
        }
    }
    
    Registry.Add(McpId, Actor);
    
    // Bind OnDestroyed if not already bound
    TWeakObjectPtr<AActor> WeakActor(Actor);
    if (!OnDestroyedHandles.Contains(WeakActor))
    {
        FDelegateHandle Handle = Actor->OnDestroyed.AddUObject(
            this, &UMcpActorIdRegistrySubsystem::OnActorDestroyed);
        OnDestroyedHandles.Add(WeakActor, Handle);
    }
    
    UE_LOG(LogMcpActorIdRegistry, Verbose, TEXT("Registered actor '%s' with McpId '%s'"), 
        *Actor->GetName(), *McpId);
}

void UMcpActorIdRegistrySubsystem::UnregisterActor(const FString& McpId)
{
    if (TWeakObjectPtr<AActor>* Found = Registry.Find(McpId))
    {
        if (AActor* Actor = Found->Get())
        {
            TWeakObjectPtr<AActor> WeakActor(Actor);
            if (FDelegateHandle* Handle = OnDestroyedHandles.Find(WeakActor))
            {
                Actor->OnDestroyed.Remove(*Handle);
                OnDestroyedHandles.Remove(WeakActor);
            }
        }
        Registry.Remove(McpId);
        UE_LOG(LogMcpActorIdRegistry, Verbose, TEXT("Unregistered McpId '%s'"), *McpId);
    }
}

AActor* UMcpActorIdRegistrySubsystem::FindByMcpId(const FString& McpId) const
{
    if (const TWeakObjectPtr<AActor>* Found = Registry.Find(McpId))
    {
        return Found->Get();
    }
    return nullptr;
}

void UMcpActorIdRegistrySubsystem::GetAllMcpIds(TArray<FString>& OutIds) const
{
    Registry.GetKeys(OutIds);
}

FString UMcpActorIdRegistrySubsystem::GetMcpIdFromActor(const AActor* Actor)
{
    if (!Actor)
    {
        return FString();
    }
    
    static const FString McpIdPrefix = TEXT("McpId:");
    
    for (const FName& Tag : Actor->Tags)
    {
        FString TagStr = Tag.ToString();
        if (TagStr.StartsWith(McpIdPrefix))
        {
            return TagStr.RightChop(McpIdPrefix.Len());
        }
    }
    
    return FString();
}

void UMcpActorIdRegistrySubsystem::OnActorSpawned(AActor* Actor)
{
    if (!Actor)
    {
        return;
    }
    
    // IDEMPOTENT: Check if already registered
    TWeakObjectPtr<AActor> WeakActor(Actor);
    if (OnDestroyedHandles.Contains(WeakActor))
    {
        return; // Already bound, skip to avoid duplicate bindings
    }
    
    // Check for McpId tag
    FString McpId = GetMcpIdFromActor(Actor);
    if (!McpId.IsEmpty())
    {
        RegisterActor(Actor, McpId);
    }
}

void UMcpActorIdRegistrySubsystem::OnActorDestroyed(AActor* DestroyedActor)
{
    if (!DestroyedActor)
    {
        return;
    }
    
    // Find and remove from registry by scanning
    for (auto It = Registry.CreateIterator(); It; ++It)
    {
        if (It.Value().Get() == DestroyedActor)
        {
            FString RemovedId = It.Key();
            It.RemoveCurrent();
            UE_LOG(LogMcpActorIdRegistry, Verbose, TEXT("Actor destroyed, removed McpId '%s'"), *RemovedId);
            break;
        }
    }
    
    // Remove from handles map
    TWeakObjectPtr<AActor> WeakActor(DestroyedActor);
    OnDestroyedHandles.Remove(WeakActor);
}
