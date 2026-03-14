// =============================================================================
// McpHandlerDeclarations.h
// =============================================================================
// Handler function declarations for MCP Automation Bridge.
//
// This file contains all handler function declarations organized by domain.
// Extracted from McpAutomationBridgeSubsystem.h for better organization.
//
// REFACTORING NOTES:
// - Handler declarations grouped by functional domain
// - Consistent naming convention: Handle[Domain][Action]
// - Each handler follows the same signature for consistency
//
// Copyright (c) 2024 MCP Automation Bridge Contributors
// =============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// Forward declarations
class FMcpBridgeWebSocket;

// =============================================================================
// Handler Signature Type
// =============================================================================

/**
 * Standard handler function signature.
 * All handlers follow this consistent pattern for maintainability.
 * 
 * @param RequestId Unique identifier for the request
 * @param Action The action string that was dispatched
 * @param Payload JSON payload containing action parameters
 * @param Socket WebSocket to send responses to
 * @return true if handler processed the request, false otherwise
 */
using FMcpHandlerFunc = TFunction<bool(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket
)>;

// =============================================================================
// Handler Interface - Abstract base for type-safe handlers
// =============================================================================

/**
 * Abstract handler interface for type-safe action handling.
 * Handlers can inherit from this to get compile-time type safety.
 */
class IMcpHandler
{
public:
    virtual ~IMcpHandler() = default;
    
    /**
     * Execute the handler with the given parameters.
     * @return true if the request was handled successfully
     */
    virtual bool Execute(
        const FString& RequestId,
        const FString& Action,
        const TSharedPtr<FJsonObject>& Payload,
        TSharedPtr<FMcpBridgeWebSocket> Socket
    ) = 0;
    
    /**
     * Get the action name this handler responds to.
     */
    virtual FString GetActionName() const = 0;
    
    /**
     * Get the domain this handler belongs to.
     */
    virtual FString GetDomain() const = 0;
};

// =============================================================================
// Handler Declaration Macros
// =============================================================================

/**
 * Macro for declaring a handler function with consistent signature.
 * Use this in the subsystem class declaration.
 */
#define MCP_DECLARE_HANDLER(HandlerName) \
    bool Handle##HandlerName( \
        const FString& RequestId, \
        const FString& Action, \
        const TSharedPtr<FJsonObject>& Payload, \
        TSharedPtr<FMcpBridgeWebSocket> Socket \
    )

/**
 * Macro for declaring a handler with custom signature (e.g., different params).
 */
#define MCP_DECLARE_HANDLER_CUSTOM(HandlerName, Signature) \
    bool Handle##HandlerName Signature

// =============================================================================
// Forward Handler Declarations - Organized by Domain
// =============================================================================

/**
 * Namespace containing all handler declarations organized by functional domain.
 * This provides a way to see the full handler API at a glance.
 */
namespace McpHandlers
{
    // =========================================================================
    // Core Handlers - Property & Object Manipulation
    // =========================================================================
    namespace Core
    {
        MCP_DECLARE_HANDLER(ExecuteEditorFunction);
        MCP_DECLARE_HANDLER(SetObjectProperty);
        MCP_DECLARE_HANDLER(GetObjectProperty);
    }

    // =========================================================================
    // Container Handlers - Array, Map, Set Operations
    // =========================================================================
    namespace Container
    {
        // Array operations
        MCP_DECLARE_HANDLER(ArrayAppend);
        MCP_DECLARE_HANDLER(ArrayRemove);
        MCP_DECLARE_HANDLER(ArrayInsert);
        MCP_DECLARE_HANDLER(ArrayGetElement);
        MCP_DECLARE_HANDLER(ArraySetElement);
        MCP_DECLARE_HANDLER(ArrayClear);

        // Map operations
        MCP_DECLARE_HANDLER(MapSetValue);
        MCP_DECLARE_HANDLER(MapGetValue);
        MCP_DECLARE_HANDLER(MapRemoveKey);
        MCP_DECLARE_HANDLER(MapHasKey);
        MCP_DECLARE_HANDLER(MapGetKeys);
        MCP_DECLARE_HANDLER(MapClear);

        // Set operations
        MCP_DECLARE_HANDLER(SetAdd);
        MCP_DECLARE_HANDLER(SetRemove);
        MCP_DECLARE_HANDLER(SetContains);
        MCP_DECLARE_HANDLER(SetClear);
    }

    // =========================================================================
    // Asset Handlers - Asset Management Operations
    // =========================================================================
    namespace Asset
    {
        MCP_DECLARE_HANDLER(AssetAction);
        MCP_DECLARE_HANDLER(GetAssetReferences);
        MCP_DECLARE_HANDLER(GetAssetDependencies);
    }

    // =========================================================================
    // Actor Handlers - Runtime Actor Control
    // =========================================================================
    namespace Actor
    {
        MCP_DECLARE_HANDLER(ControlActorAction);
        
        // Actor sub-handlers
        MCP_DECLARE_HANDLER_CUSTOM(ControlActorSpawn, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlActorSpawnBlueprint, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlActorDelete, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlActorApplyForce, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlActorSetTransform, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlActorGetTransform, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlActorSetVisibility, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlActorAddComponent, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlActorGetComponentProperty, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlActorSetComponentProperties, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlActorGetComponents, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlActorDuplicate, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlActorAttach, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlActorDetach, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlActorFindByTag, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlActorAddTag, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlActorRemoveTag, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlActorFindByName, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlActorDeleteByTag, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlActorSetBlueprintVariables, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlActorCreateSnapshot, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlActorRestoreSnapshot, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlActorExport, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlActorGetBoundingBox, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlActorGetMetadata, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlActorFindByClass, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlActorRemoveComponent, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlActorSetCollision, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlActorCallFunction, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlActorList, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlActorGet, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
    }

    // =========================================================================
    // Editor Handlers - Editor Control Operations
    // =========================================================================
    namespace Editor
    {
        MCP_DECLARE_HANDLER(ControlEditorAction);
        
        // Editor sub-handlers
        MCP_DECLARE_HANDLER_CUSTOM(ControlEditorPlay, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlEditorStop, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlEditorEject, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlEditorPossess, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlEditorFocusActor, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlEditorSetCamera, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlEditorSetViewMode, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlEditorOpenAsset, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlEditorScreenshot, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlEditorPause, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlEditorResume, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlEditorConsoleCommand, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlEditorStepFrame, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlEditorStartRecording, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlEditorStopRecording, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlEditorCreateBookmark, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlEditorJumpToBookmark, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlEditorSetPreferences, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlEditorSetViewportRealtime, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlEditorSimulateInput, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlEditorCloseAsset, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlEditorSaveAll, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlEditorUndo, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlEditorRedo, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlEditorSetEditorMode, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlEditorShowStats, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlEditorHideStats, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlEditorSetGameView, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlEditorSetImmersiveMode, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlEditorSetFixedDeltaTime, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ControlEditorOpenLevel, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
    }

    // =========================================================================
    // Level Handlers - Level Management Operations
    // =========================================================================
    namespace Level
    {
        MCP_DECLARE_HANDLER(LevelAction);
    }

    // =========================================================================
    // Blueprint Handlers - Blueprint Operations
    // =========================================================================
    namespace Blueprint
    {
        MCP_DECLARE_HANDLER(BlueprintAction);
    }

    // =========================================================================
    // Sequence Handlers - Sequencer Operations
    // =========================================================================
    namespace Sequence
    {
        MCP_DECLARE_HANDLER(SequenceAction);
        
        // Sequence sub-handlers
        MCP_DECLARE_HANDLER_CUSTOM(SequenceCreate, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(SequenceOpen, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(SequenceSave, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(SequenceClose, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(SequenceAddTrack, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(SequenceRemoveTrack, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(SequenceAddKeyframe, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(SequenceSetTime, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(SequenceGetMetadata, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(SequenceRename, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(SequenceDelete, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(SequenceAddSection, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(SequenceSetTickResolution, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(SequenceSetViewRange, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(SequenceSetTrackMuted, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(SequenceSetTrackSolo, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(SequenceSetTrackLocked, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
    }

    // =========================================================================
    // Animation & Physics Handlers
    // =========================================================================
    namespace AnimationPhysics
    {
        MCP_DECLARE_HANDLER(AnimationPhysicsAction);
    }

    // =========================================================================
    // Effect Handlers - Niagara and VFX Operations
    // =========================================================================
    namespace Effect
    {
        MCP_DECLARE_HANDLER(EffectAction);
        MCP_DECLARE_HANDLER_CUSTOM(CreateNiagaraEffect, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket,
            const FString& EffectName,
            const FString& DefaultSystemPath
        ));
    }

    // =========================================================================
    // Audio Handlers
    // =========================================================================
    namespace Audio
    {
        MCP_DECLARE_HANDLER(AudioAction);
        MCP_DECLARE_HANDLER_CUSTOM(CreateDialogueVoice, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(CreateDialogueWave, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(SetDialogueContext, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(CreateReverbEffect, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(CreateSourceEffectChain, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(AddSourceEffect, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(CreateSubmixEffect, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
    }

    // =========================================================================
    // UI Handlers
    // =========================================================================
    namespace UI
    {
        MCP_DECLARE_HANDLER(UiAction);
    }

    // =========================================================================
    // Input Handlers
    // =========================================================================
    namespace Input
    {
        MCP_DECLARE_HANDLER(InputAction);
    }

    // =========================================================================
    // Lighting Handlers
    // =========================================================================
    namespace Lighting
    {
        MCP_DECLARE_HANDLER(LightingAction);
    }

    // =========================================================================
    // Performance Handlers
    // =========================================================================
    namespace Performance
    {
        MCP_DECLARE_HANDLER(PerformanceAction);
    }

    // =========================================================================
    // Environment Handlers - Landscape, Foliage, etc.
    // =========================================================================
    namespace Environment
    {
        MCP_DECLARE_HANDLER(BuildEnvironmentAction);
        MCP_DECLARE_HANDLER(ControlEnvironmentAction);
        MCP_DECLARE_HANDLER_CUSTOM(PaintFoliage, (
            const FString& RequestId,
            const FString& Action,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(GetFoliageInstances, (
            const FString& RequestId,
            const FString& Action,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(RemoveFoliage, (
            const FString& RequestId,
            const FString& Action,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(GenerateLODs, (
            const FString& RequestId,
            const FString& Action,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(BakeLightmap, (
            const FString& RequestId,
            const FString& Action,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(CreateProceduralTerrain, (
            const FString& RequestId,
            const FString& Action,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(CreateProceduralFoliage, (
            const FString& RequestId,
            const FString& Action,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
    }

    // =========================================================================
    // Interaction Handlers
    // =========================================================================
    namespace Interaction
    {
        MCP_DECLARE_HANDLER_CUSTOM(CreateInteractionComponent, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ConfigureInteractionTrace, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(ConfigureInteractionWidget, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(CreateDoorActor, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(CreateSwitchActor, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
        MCP_DECLARE_HANDLER_CUSTOM(CreateChestActor, (
            const FString& RequestId,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
    }

    // =========================================================================
    // Skeleton Handlers
    // =========================================================================
    namespace Skeleton
    {
        MCP_DECLARE_HANDLER_CUSTOM(SkeletonAction, (
            const FString& RequestId,
            const FString& Action,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
    }

    // =========================================================================
    // UI Authoring Handlers
    // =========================================================================
    namespace UIAuthoring
    {
        MCP_DECLARE_HANDLER_CUSTOM(UIAuthoringAction, (
            const FString& RequestId,
            const FString& Action,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
    }

    // =========================================================================
    // Material Authoring Handlers
    // =========================================================================
    namespace MaterialAuthoring
    {
        MCP_DECLARE_HANDLER_CUSTOM(MaterialAuthoringAction, (
            const FString& RequestId,
            const FString& Action,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
    }

    // =========================================================================
    // AI Handlers
    // =========================================================================
    namespace AI
    {
        MCP_DECLARE_HANDLER_CUSTOM(AIAction, (
            const FString& RequestId,
            const FString& Action,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket
        ));
    }

} // namespace McpHandlers

// Undefine macros to prevent pollution
#undef MCP_DECLARE_HANDLER
#undef MCP_DECLARE_HANDLER_CUSTOM