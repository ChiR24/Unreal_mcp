// Copyright Epic Games, Inc. All Rights Reserved.

#include "McpInteractableComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"
#include "Math/UnrealMathUtility.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpInteractable, Log, All);

UMcpInteractableComponent::UMcpInteractableComponent()
{
    // Enable replication by default
    SetIsReplicatedByDefault(true);
    
    // No ticking needed - interaction is event-driven
    PrimaryComponentTick.bCanEverTick = false;
    
    // Initialize with default values
    InteractionType = TEXT("use");
    InteractionPrompt = TEXT("Press E to interact");
    InteractionRange = 200.0f;
    InteractionPriority = 0;
    bIsEnabled = true;
    bIsFocused = false;
    FocusedByActorId = TEXT("");
    
    // Initialize cache
    bPreviousEnabled = true;
    bPreviousFocused = false;
}

void UMcpInteractableComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    
    DOREPLIFETIME(UMcpInteractableComponent, InteractionType);
    DOREPLIFETIME(UMcpInteractableComponent, InteractionPrompt);
    DOREPLIFETIME(UMcpInteractableComponent, InteractionRange);
    DOREPLIFETIME(UMcpInteractableComponent, InteractionPriority);
    DOREPLIFETIME(UMcpInteractableComponent, bIsEnabled);
    DOREPLIFETIME(UMcpInteractableComponent, bIsFocused);
    DOREPLIFETIME(UMcpInteractableComponent, FocusedByActorId);
}

void UMcpInteractableComponent::OnRep_IsEnabled()
{
    // Check if state actually changed
    if (bPreviousEnabled != bIsEnabled)
    {
        bPreviousEnabled = bIsEnabled;
        
        // Broadcast enabled change event
        OnInteractionEnabledChanged.Broadcast(bIsEnabled);
        
        UE_LOG(LogMcpInteractable, Verbose, TEXT("Interactable enabled replicated: %s"), 
            bIsEnabled ? TEXT("true") : TEXT("false"));
    }
}

void UMcpInteractableComponent::OnRep_IsFocused()
{
    // Check if state actually changed
    if (bPreviousFocused != bIsFocused)
    {
        bPreviousFocused = bIsFocused;
        
        // Get focusing actor from cached reference
        AActor* FocusingActor = CachedFocusingActor.Get();
        
        // Broadcast focus change event
        OnInteractionFocusChanged.Broadcast(bIsFocused, FocusingActor);
        
        UE_LOG(LogMcpInteractable, Verbose, TEXT("Interactable focus replicated: %s (by: %s)"), 
            bIsFocused ? TEXT("true") : TEXT("false"),
            *FocusedByActorId);
    }
}

void UMcpInteractableComponent::ConfigureInteraction(const FString& Type, const FString& Prompt, float Range, int32 Priority)
{
    // Only allow server to configure
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority())
    {
        UE_LOG(LogMcpInteractable, Warning, TEXT("ConfigureInteraction called on client - ignored"));
        return;
    }
    
    InteractionType = Type;
    InteractionPrompt = Prompt;
    InteractionRange = FMath::Max(1.0f, Range); // Prevent zero/negative
    InteractionPriority = Priority;
    
    UE_LOG(LogMcpInteractable, Log, TEXT("Interactable configured: Type='%s', Prompt='%s', Range=%.1f, Priority=%d"), 
        *InteractionType, *InteractionPrompt, InteractionRange, InteractionPriority);
}

void UMcpInteractableComponent::SetEnabled(bool bEnabled)
{
    // Only allow server to change enabled state
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority())
    {
        UE_LOG(LogMcpInteractable, Warning, TEXT("SetEnabled called on client - ignored"));
        return;
    }
    
    // Early out if no change
    if (bIsEnabled == bEnabled)
    {
        return;
    }
    
    bPreviousEnabled = bIsEnabled;
    bIsEnabled = bEnabled;
    
    // Clear focus when disabled
    if (!bIsEnabled && bIsFocused)
    {
        SetFocused(false, nullptr);
    }
    
    // Broadcast on server
    OnInteractionEnabledChanged.Broadcast(bIsEnabled);
    
    UE_LOG(LogMcpInteractable, Log, TEXT("Interactable enabled: %s"), 
        bIsEnabled ? TEXT("true") : TEXT("false"));
}

bool UMcpInteractableComponent::IsInRange(AActor* Actor) const
{
    if (!Actor)
    {
        return false;
    }
    
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return false;
    }
    
    // Calculate distance between actor and owner
    float Distance = FVector::Dist(Actor->GetActorLocation(), Owner->GetActorLocation());
    
    return Distance <= InteractionRange;
}

void UMcpInteractableComponent::SetFocused(bool bFocused, AActor* FocusingActor)
{
    // Only allow server to change focus
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority())
    {
        UE_LOG(LogMcpInteractable, Warning, TEXT("SetFocused called on client - ignored"));
        return;
    }
    
    // Cannot focus if disabled
    if (bFocused && !bIsEnabled)
    {
        UE_LOG(LogMcpInteractable, Warning, TEXT("Cannot focus disabled interactable"));
        return;
    }
    
    // Early out if no change
    if (bIsFocused == bFocused)
    {
        // But update focusing actor if it changed while staying focused
        if (bFocused && FocusingActor)
        {
            FString NewActorId = FocusingActor->GetName();
            if (!FocusedByActorId.Equals(NewActorId))
            {
                FocusedByActorId = NewActorId;
                CachedFocusingActor = FocusingActor;
            }
        }
        return;
    }
    
    bPreviousFocused = bIsFocused;
    bIsFocused = bFocused;
    
    // Update focusing actor ID
    if (bFocused && FocusingActor)
    {
        FocusedByActorId = FocusingActor->GetName();
        CachedFocusingActor = FocusingActor;
    }
    else
    {
        FocusedByActorId = TEXT("");
        CachedFocusingActor = nullptr;
    }
    
    // Broadcast on server
    OnInteractionFocusChanged.Broadcast(bIsFocused, FocusingActor);
    
    UE_LOG(LogMcpInteractable, Log, TEXT("Interactable focus: %s (by: %s)"), 
        bIsFocused ? TEXT("true") : TEXT("false"),
        *FocusedByActorId);
}

bool UMcpInteractableComponent::ExecuteInteraction(AActor* InteractingActor)
{
    // Only allow server to execute interactions
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority())
    {
        UE_LOG(LogMcpInteractable, Warning, TEXT("ExecuteInteraction called on client - ignored"));
        return false;
    }
    
    // Cannot interact if disabled
    if (!bIsEnabled)
    {
        UE_LOG(LogMcpInteractable, Warning, TEXT("Cannot execute interaction - interactable is disabled"));
        return false;
    }
    
    // Validate interacting actor
    if (!InteractingActor)
    {
        UE_LOG(LogMcpInteractable, Warning, TEXT("Cannot execute interaction - no interacting actor"));
        return false;
    }
    
    // Check range
    if (!IsInRange(InteractingActor))
    {
        UE_LOG(LogMcpInteractable, Warning, TEXT("Cannot execute interaction - actor out of range"));
        return false;
    }
    
    // Broadcast interaction executed event
    OnInteractionExecuted.Broadcast(InteractingActor, InteractionType);
    
    UE_LOG(LogMcpInteractable, Log, TEXT("Interaction executed: Type='%s', Actor='%s'"), 
        *InteractionType, *InteractingActor->GetName());
    
    return true;
}

TArray<UMcpInteractableComponent*> UMcpInteractableComponent::GetNearbyInteractables(UObject* WorldContextObject, FVector Location, float Radius)
{
    TArray<UMcpInteractableComponent*> Result;
    
    if (!WorldContextObject)
    {
        UE_LOG(LogMcpInteractable, Warning, TEXT("GetNearbyInteractables: Invalid world context"));
        return Result;
    }
    
    UWorld* World = WorldContextObject->GetWorld();
    if (!World)
    {
        UE_LOG(LogMcpInteractable, Warning, TEXT("GetNearbyInteractables: Cannot get world"));
        return Result;
    }
    
    float RadiusSquared = Radius * Radius;
    
    // Iterate all actors with interactable components
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!Actor)
        {
            continue;
        }
        
        // Check distance first (cheaper than component lookup)
        float DistanceSquared = FVector::DistSquared(Actor->GetActorLocation(), Location);
        if (DistanceSquared > RadiusSquared)
        {
            continue;
        }
        
        // Find interactable component
        UMcpInteractableComponent* InteractableComp = Actor->FindComponentByClass<UMcpInteractableComponent>();
        if (InteractableComp && InteractableComp->bIsEnabled)
        {
            Result.Add(InteractableComp);
        }
    }
    
    // Sort by priority (highest first), then by distance (closest first)
    Result.Sort([Location](const UMcpInteractableComponent& A, const UMcpInteractableComponent& B)
    {
        // Higher priority first
        if (A.InteractionPriority != B.InteractionPriority)
        {
            return A.InteractionPriority > B.InteractionPriority;
        }
        
        // Closer distance first
        AActor* OwnerA = A.GetOwner();
        AActor* OwnerB = B.GetOwner();
        if (OwnerA && OwnerB)
        {
            float DistA = FVector::DistSquared(OwnerA->GetActorLocation(), Location);
            float DistB = FVector::DistSquared(OwnerB->GetActorLocation(), Location);
            return DistA < DistB;
        }
        
        return false;
    });
    
    UE_LOG(LogMcpInteractable, Verbose, TEXT("GetNearbyInteractables found %d components within %.1f units"), 
        Result.Num(), Radius);
    
    return Result;
}
