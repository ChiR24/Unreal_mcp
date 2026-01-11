// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "McpInteractableComponent.generated.h"

// Delegate for interaction executed
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMcpInteractionExecuted, AActor*, InteractingActor, const FString&, InteractionType);

// Delegate for focus changed
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMcpInteractionFocusChanged, bool, bIsFocused, AActor*, FocusingActor);

// Delegate for enabled state changed
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMcpInteractionEnabledChanged, bool, bIsEnabled);

/**
 * UMcpInteractableComponent
 * 
 * Replicated actor component for interaction systems (doors, buttons, NPCs, pickups).
 * 
 * Features:
 * - Network replication with OnRep callbacks
 * - Configurable interaction type, prompt, and range
 * - Enable/disable state with replication
 * - Focus tracking for highlighting systems
 * - Priority-based resolution for overlapping interactables
 * - Static query for nearby interactables
 * 
 * Use cases:
 * - Doors (press E to open)
 * - Buttons (press E to activate)
 * - NPCs (press E to talk)
 * - Pickups (press E to collect)
 * - Examine objects (press E to examine)
 */
UCLASS(ClassGroup=(MCP), meta=(BlueprintSpawnableComponent))
class MCPAUTOMATIONBRIDGE_API UMcpInteractableComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UMcpInteractableComponent();

    // Interaction type - "use", "pickup", "talk", "examine", etc.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Interaction")
    FString InteractionType;

    // Interaction prompt - "Press E to open", etc.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Interaction")
    FString InteractionPrompt;

    // How close actor must be to interact
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Interaction")
    float InteractionRange;

    // Priority for resolving multiple overlapping interactables (higher = preferred)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Interaction")
    int32 InteractionPriority;

    // Whether interaction is enabled
    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_IsEnabled, Category = "Interaction")
    bool bIsEnabled;

    // Is this the currently focused interactable
    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_IsFocused, Category = "Interaction|Focus")
    bool bIsFocused;

    // Actor ID currently focusing this (empty if none)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Interaction|Focus")
    FString FocusedByActorId;

    // Blueprint-bindable events
    UPROPERTY(BlueprintAssignable, Category = "Interaction|Events")
    FOnMcpInteractionExecuted OnInteractionExecuted;

    UPROPERTY(BlueprintAssignable, Category = "Interaction|Events")
    FOnMcpInteractionFocusChanged OnInteractionFocusChanged;

    UPROPERTY(BlueprintAssignable, Category = "Interaction|Events")
    FOnMcpInteractionEnabledChanged OnInteractionEnabledChanged;

    // Replication
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // OnRep callbacks
    UFUNCTION()
    void OnRep_IsEnabled();

    UFUNCTION()
    void OnRep_IsFocused();

    // Public API - Configuration
    UFUNCTION(BlueprintCallable, Category = "Interaction")
    void ConfigureInteraction(const FString& Type, const FString& Prompt, float Range, int32 Priority);

    // Public API - Enable/Disable
    UFUNCTION(BlueprintCallable, Category = "Interaction")
    void SetEnabled(bool bEnabled);

    // Public API - Range check
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Interaction")
    bool IsInRange(AActor* Actor) const;

    // Public API - Focus management (Server only)
    UFUNCTION(BlueprintCallable, Category = "Interaction")
    void SetFocused(bool bFocused, AActor* FocusingActor);

    // Public API - Execute interaction (Server only, broadcasts delegate)
    UFUNCTION(BlueprintCallable, Category = "Interaction")
    bool ExecuteInteraction(AActor* InteractingActor);

    // Public API - Getters
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Interaction")
    FString GetInteractionType() const { return InteractionType; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Interaction")
    FString GetInteractionPrompt() const { return InteractionPrompt; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Interaction")
    float GetInteractionRange() const { return InteractionRange; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Interaction")
    int32 GetInteractionPriority() const { return InteractionPriority; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Interaction")
    bool IsEnabled() const { return bIsEnabled; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Interaction")
    bool IsFocused() const { return bIsFocused; }

    // Static query for nearby interactables
    UFUNCTION(BlueprintCallable, Category = "Interaction", meta = (WorldContext = "WorldContextObject"))
    static TArray<UMcpInteractableComponent*> GetNearbyInteractables(UObject* WorldContextObject, FVector Location, float Radius);

private:
    // Cache for OnRep comparison (enabled state)
    bool bPreviousEnabled;

    // Cache for OnRep comparison (focus state)
    bool bPreviousFocused;

    // Cached focusing actor for OnRep
    TWeakObjectPtr<AActor> CachedFocusingActor;
};
