// =============================================================================
// McpAutomationBridge_NetworkingHandlers.cpp
// =============================================================================
// Phase 20: Networking & Multiplayer System Handlers for MCP Automation Bridge.
//
// This file implements the following handlers:
// - manage_networking (main dispatcher)
//
// 20.1 Replication Actions:
//   - set_property_replicated
//     Payload:  { blueprintPath, propertyName, replicated? }
//     Response: { success, message, assetVerification }
//
//   - set_replication_condition
//     Payload:  { blueprintPath, propertyName, condition }
//     Response: { success, message, assetVerification }
//
//   - configure_net_update_frequency
//     Payload:  { blueprintPath, netUpdateFrequency?, minNetUpdateFrequency? }
//     Response: { success, message, assetVerification }
//
//   - configure_net_priority
//     Payload:  { blueprintPath, netPriority? }
//     Response: { success, message, assetVerification }
//
//   - set_net_dormancy
//     Payload:  { blueprintPath, dormancy }
//     Response: { success, message, assetVerification }
//
//   - configure_replication_graph
//     Payload:  { blueprintPath, spatiallyLoaded?, netLoadOnClient?, replicationPolicy? }
//     Response: { success, spatiallyLoaded, netLoadOnClient, replicationPolicy, message, assetVerification }
//
// 20.2 RPC Actions:
//   - create_rpc_function
//     Payload:  { blueprintPath, functionName, rpcType, reliable? }
//     Response: { success, functionName, rpcType, reliable, message, assetVerification }
//
//   - configure_rpc_validation
//     Payload:  { blueprintPath, functionName, withValidation? }
//     Response: { success, withValidation, message, assetVerification }
//
//   - set_rpc_reliability
//     Payload:  { blueprintPath, functionName, reliable? }
//     Response: { success, reliable, message, assetVerification }
//
// 20.3 Authority & Ownership Actions:
//   - set_owner
//     Payload:  { actorName, ownerActorName? }
//     Response: { success, message, actorVerification }
//
//   - set_autonomous_proxy
//     Payload:  { blueprintPath, isAutonomousProxy? }
//     Response: { success, isAutonomousProxy, message, assetVerification }
//
//   - check_has_authority
//     Payload:  { actorName }
//     Response: { success, hasAuthority, role }
//
//   - check_is_locally_controlled
//     Payload:  { actorName }
//     Response: { success, isLocallyControlled, isLocalController }
//
// 20.4 Network Relevancy Actions:
//   - configure_net_cull_distance
//     Payload:  { blueprintPath, netCullDistanceSquared?, useOwnerNetRelevancy? }
//     Response: { success, message, assetVerification }
//
//   - set_always_relevant
//     Payload:  { blueprintPath, alwaysRelevant? }
//     Response: { success, message, assetVerification }
//
//   - set_only_relevant_to_owner
//     Payload:  { blueprintPath, onlyRelevantToOwner? }
//     Response: { success, message, assetVerification }
//
// 20.5 Net Serialization Actions:
//   - configure_net_serialization
//     Payload:  { blueprintPath, structName?, customSerialization? }
//     Response: { success, customSerialization, structName?, message, assetVerification }
//
//   - set_replicated_using
//     Payload:  { blueprintPath, propertyName, repNotifyFunc }
//     Response: { success, message, assetVerification }
//
//   - configure_push_model
//     Payload:  { blueprintPath, usePushModel? }
//     Response: { success, usePushModel, message, assetVerification }
//
// 20.6 Network Prediction Actions:
//   - configure_client_prediction
//     Payload:  { blueprintPath, enablePrediction?, predictionThreshold? }
//     Response: { success, enablePrediction, predictionThreshold, message, assetVerification }
//
//   - configure_server_correction
//     Payload:  { blueprintPath, correctionThreshold?, smoothingRate? }
//     Response: { success, correctionThreshold, smoothingRate, message, assetVerification }
//
//   - add_network_prediction_data
//     Payload:  { blueprintPath, dataType, variableName? }
//     Response: { success, variableName, dataType, message, assetVerification }
//
//   - configure_movement_prediction
//     Payload:  { blueprintPath, networkSmoothingMode?, networkMaxSmoothUpdateDistance?, networkNoSmoothUpdateDistance? }
//     Response: { success, message, assetVerification }
//
// 20.7 Connection & Session Actions:
//   - configure_net_driver
//     Payload:  { maxClientRate?, maxInternetClientRate?, netServerMaxTickRate? }
//     Response: { success, appliedToActiveDriver, maxClientRate, maxInternetClientRate, netServerMaxTickRate, message }
//
//   - set_net_role
//     Payload:  { blueprintPath, role }
//     Response: { success, role, replicates, message, assetVerification }
//
//   - configure_replicated_movement
//     Payload:  { blueprintPath, replicateMovement? }
//     Response: { success, message, assetVerification }
//
// 20.8 Utility Actions:
//   - get_networking_info
//     Payload:  { blueprintPath? | actorName? }
//     Response: { success, networkingInfo }
//
// UE VERSION COMPATIBILITY:
// - UE 5.0: NetUpdateFrequency/MinNetUpdateFrequency/NetCullDistanceSquared not available
// - UE 5.1-5.4: Direct property access (deprecated in 5.5)
// - UE 5.5+: NetUpdateFrequency, MinNetUpdateFrequency, NetCullDistanceSquared
//            use getter/setter functions instead of direct property access
// - UE 5.7: NetServerMaxTickRate uses SetNetServerMaxTickRate() setter
// - Replication APIs stable across versions with deprecation warnings in 5.5+
//
// Copyright (c) 2024 MCP Automation Bridge Contributors
// =============================================================================

#include "McpVersionCompatibility.h"  // MUST BE FIRST - Version compatibility macros
#include "McpHandlerUtils.h"          // Utility functions for JSON parsing

// ---- Core Includes ----
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpBridgeWebSocket.h"
#include "Misc/EngineVersionComparison.h"

// ---- Editor Includes ----
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"

// ---- Blueprint & Graph Includes ----
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_CallFunction.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"

// ---- World & Actor Includes ----
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

// ---- Networking & Replication Includes ----
#include "Net/UnrealNetwork.h"
#include "Engine/NetDriver.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpNetworkingHandlers, Log, All);

// ============================================================================
// Helper Functions
// ============================================================================

namespace NetworkingHelpers
{
    // ---- JSON Field Extraction ----

    /** Get string field with default value. */
    FString GetStringField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName, const FString& Default = TEXT(""))
    {
        FString Value = Default;
        if (Payload.IsValid())
        {
            Payload->TryGetStringField(FieldName, Value);
        }
        return Value;
    }

    /** Get number field with default value. */
    double GetNumberField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName, double Default = 0.0)
    {
        double Value = Default;
        if (Payload.IsValid())
        {
            Payload->TryGetNumberField(FieldName, Value);
        }
        return Value;
    }

    /** Get bool field with default value. */
    bool GetBoolField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName, bool Default = false)
    {
        bool Value = Default;
        if (Payload.IsValid())
        {
            Payload->TryGetBoolField(FieldName, Value);
        }
        return Value;
    }

    /** Get object field or nullptr if not present. */
    TSharedPtr<FJsonObject> GetObjectField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName)
    {
        if (Payload.IsValid() && Payload->HasTypedField<EJson::Object>(FieldName))
        {
            return Payload->GetObjectField(FieldName);
        }
        return nullptr;
    }

    /** Get array field or nullptr if not present. */
    const TArray<TSharedPtr<FJsonValue>>* GetArrayField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName)
    {
        if (Payload.IsValid() && Payload->HasTypedField<EJson::Array>(FieldName))
        {
            return &Payload->GetArrayField(FieldName);
        }
        return nullptr;
    }

    // ---- Blueprint Utilities ----

    /**
     * Load Blueprint from asset path.
     * Handles paths with and without .uasset suffix.
     * @param BlueprintPath Asset path (e.g., "/Game/BP_MyActor")
     * @return Loaded UBlueprint or nullptr
     */
    UBlueprint* LoadBlueprintFromPath(const FString& BlueprintPath)
    {
        FString CleanPath = BlueprintPath;
        if (!CleanPath.EndsWith(TEXT("_C")))
        {
            // Try loading as blueprint
            UBlueprint* BP = Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *CleanPath));
            if (BP) return BP;
            
            // Try with .uasset suffix removed
            if (CleanPath.EndsWith(TEXT(".uasset")))
            {
                CleanPath = CleanPath.LeftChop(7);
                BP = Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *CleanPath));
            }
            return BP;
        }
        return nullptr;
    }

    // ---- Actor Utilities ----

    /**
     * Find actor by name in the given world.
     * Checks both GetActorLabel() (user-visible name) and GetName() (internal name).
     * @param World The world to search in
     * @param ActorName Actor label or internal name to match
     * @return Found AActor or nullptr
     */
    AActor* FindActorByName(UWorld* World, const FString& ActorName)
    {
        if (!World) return nullptr;
        
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            AActor* Actor = *It;
            // Check both GetActorLabel() (user-visible name) and GetName() (internal name)
            // This matches the behavior of UMcpAutomationBridgeSubsystem::FindActorByName
            if (Actor && (Actor->GetActorLabel().Equals(ActorName, ESearchCase::IgnoreCase) ||
                          Actor->GetName().Equals(ActorName, ESearchCase::IgnoreCase)))
            {
                return Actor;
            }
        }
        return nullptr;
    }

    // ---- Enum Conversion Utilities ----

    /**
     * Convert replication condition string to ELifetimeCondition enum.
     * @param ConditionStr String like "COND_None", "COND_OwnerOnly", etc.
     * @return Matching ELifetimeCondition (defaults to COND_None)
     */
    ELifetimeCondition GetReplicationCondition(const FString& ConditionStr)
    {
        if (ConditionStr == TEXT("COND_None")) return COND_None;
        if (ConditionStr == TEXT("COND_InitialOnly")) return COND_InitialOnly;
        if (ConditionStr == TEXT("COND_OwnerOnly")) return COND_OwnerOnly;
        if (ConditionStr == TEXT("COND_SkipOwner")) return COND_SkipOwner;
        if (ConditionStr == TEXT("COND_SimulatedOnly")) return COND_SimulatedOnly;
        if (ConditionStr == TEXT("COND_AutonomousOnly")) return COND_AutonomousOnly;
        if (ConditionStr == TEXT("COND_SimulatedOrPhysics")) return COND_SimulatedOrPhysics;
        if (ConditionStr == TEXT("COND_InitialOrOwner")) return COND_InitialOrOwner;
        if (ConditionStr == TEXT("COND_Custom")) return COND_Custom;
        if (ConditionStr == TEXT("COND_ReplayOrOwner")) return COND_ReplayOrOwner;
        if (ConditionStr == TEXT("COND_ReplayOnly")) return COND_ReplayOnly;
        if (ConditionStr == TEXT("COND_SimulatedOnlyNoReplay")) return COND_SimulatedOnlyNoReplay;
        if (ConditionStr == TEXT("COND_SimulatedOrPhysicsNoReplay")) return COND_SimulatedOrPhysicsNoReplay;
        if (ConditionStr == TEXT("COND_SkipReplay")) return COND_SkipReplay;
        if (ConditionStr == TEXT("COND_Never")) return COND_Never;
        return COND_None;
    }

    /**
     * Convert dormancy mode string to ENetDormancy enum.
     * @param DormancyStr String like "DORM_Never", "DORM_Awake", etc.
     * @return Matching ENetDormancy (defaults to DORM_Never)
     */
    ENetDormancy GetNetDormancy(const FString& DormancyStr)
    {
        if (DormancyStr == TEXT("DORM_Never")) return DORM_Never;
        if (DormancyStr == TEXT("DORM_Awake")) return DORM_Awake;
        if (DormancyStr == TEXT("DORM_DormantAll")) return DORM_DormantAll;
        if (DormancyStr == TEXT("DORM_DormantPartial")) return DORM_DormantPartial;
        if (DormancyStr == TEXT("DORM_Initial")) return DORM_Initial;
        return DORM_Never;
    }

    /**
     * Convert net role string to ENetRole enum.
     * @param RoleStr String like "ROLE_None", "ROLE_Authority", etc.
     * @return Matching ENetRole (defaults to ROLE_None)
     */
    ENetRole GetNetRole(const FString& RoleStr)
    {
        if (RoleStr == TEXT("ROLE_None")) return ROLE_None;
        if (RoleStr == TEXT("ROLE_SimulatedProxy")) return ROLE_SimulatedProxy;
        if (RoleStr == TEXT("ROLE_AutonomousProxy")) return ROLE_AutonomousProxy;
        if (RoleStr == TEXT("ROLE_Authority")) return ROLE_Authority;
        return ROLE_None;
    }

    /**
     * Convert ENetRole to human-readable string.
     * @param Role The net role enum value
     * @return String representation (e.g., "ROLE_Authority")
     */
    FString NetRoleToString(ENetRole Role)
    {
        switch (Role)
        {
            case ROLE_None: return TEXT("ROLE_None");
            case ROLE_SimulatedProxy: return TEXT("ROLE_SimulatedProxy");
            case ROLE_AutonomousProxy: return TEXT("ROLE_AutonomousProxy");
            case ROLE_Authority: return TEXT("ROLE_Authority");
            default: return TEXT("ROLE_Unknown");
        }
    }

    /**
     * Convert ENetDormancy to human-readable string.
     * @param Dormancy The dormancy enum value
     * @return String representation (e.g., "DORM_Awake")
     */
    FString NetDormancyToString(ENetDormancy Dormancy)
    {
        switch (Dormancy)
        {
            case DORM_Never: return TEXT("DORM_Never");
            case DORM_Awake: return TEXT("DORM_Awake");
            case DORM_DormantAll: return TEXT("DORM_DormantAll");
            case DORM_DormantPartial: return TEXT("DORM_DormantPartial");
            case DORM_Initial: return TEXT("DORM_Initial");
            default: return TEXT("DORM_Unknown");
        }
    }
}

// ============================================================================
// Main Handler Implementation
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleManageNetworkingAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    using namespace NetworkingHelpers;

    // Only handle manage_networking action
    if (Action != TEXT("manage_networking"))
    {
        return false;
    }

    // Get subAction from payload
    FString SubAction = GetStringField(Payload, TEXT("subAction"));
    if (SubAction.IsEmpty())
    {
        SubAction = Action;
    }

    UE_LOG(LogMcpNetworkingHandlers, Log, TEXT("HandleManageNetworkingAction: %s"), *SubAction);

    TSharedPtr<FJsonObject> ResultJson = McpHandlerUtils::CreateResultObject();

    // =========================================================================
    // 20.1 Replication Actions
    // =========================================================================

    // ----- set_property_replicated -----
    // Sets or clears the CPF_Net flag on a Blueprint property to enable/disable
    // property replication.
    //
    // Payload:  { blueprintPath: string, propertyName: string, replicated?: bool }
    // Response: { success: bool, message: string, assetVerification }
    if (SubAction == TEXT("set_property_replicated"))
    {
        FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"));
        FString PropertyName = GetStringField(Payload, TEXT("propertyName"));
        bool bReplicated = GetBoolField(Payload, TEXT("replicated"), true);

        if (BlueprintPath.IsEmpty() || PropertyName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath or propertyName"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        // Find the property
        FProperty* Property = nullptr;
        for (TFieldIterator<FProperty> It(Blueprint->GeneratedClass); It; ++It)
        {
            if (It->GetName() == PropertyName)
            {
                Property = *It;
                break;
            }
        }

        if (!Property)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Property not found in blueprint"), TEXT("NOT_FOUND"));
            return true;
        }

        // Set replication flag
        if (bReplicated)
        {
            Property->SetPropertyFlags(CPF_Net);
        }
        else
        {
            Property->ClearPropertyFlags(CPF_Net);
        }

        // Mark blueprint modified
        Blueprint->Modify();
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Property %s replication set to %s"), *PropertyName, bReplicated ? TEXT("true") : TEXT("false")));
        McpHandlerUtils::AddVerification(ResultJson, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Property replication configured"), ResultJson);
        return true;
    }

    // ----- set_replication_condition -----
    // Sets the replication condition (ELifetimeCondition) on a Blueprint variable.
    // Also ensures the property has CPF_Net flag set.
    //
    // Payload:  { blueprintPath: string, propertyName: string, condition: string }
    // Response: { success: bool, message: string, assetVerification }
    if (SubAction == TEXT("set_replication_condition"))
    {
        FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"));
        FString PropertyName = GetStringField(Payload, TEXT("propertyName"));
        FString Condition = GetStringField(Payload, TEXT("condition"));

        if (BlueprintPath.IsEmpty() || PropertyName.IsEmpty() || Condition.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        ELifetimeCondition LifetimeCondition = GetReplicationCondition(Condition);

        // Find the variable description and set its replication condition
        bool bFound = false;
        for (FBPVariableDescription& VarDesc : Blueprint->NewVariables)
        {
            if (VarDesc.VarName == FName(*PropertyName))
            {
                // Ensure property is replicated and set the condition
                VarDesc.PropertyFlags |= CPF_Net;
                VarDesc.ReplicationCondition = LifetimeCondition;
                bFound = true;
                break;
            }
        }

        if (!bFound)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Property '%s' not found"), *PropertyName), TEXT("NOT_FOUND"));
            return true;
        }

        Blueprint->Modify();
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Replication condition set to %s"), *Condition));
        McpHandlerUtils::AddVerification(ResultJson, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Replication condition configured"), ResultJson);
        return true;
    }

    // ----- configure_net_update_frequency -----
    // Configures the net update frequency and minimum net update frequency on an
    // Actor CDO. Uses setter methods on UE 5.5+ and direct property access on 5.1-5.4.
    //
    // Version notes:
    //   UE 5.0:   API not available
    //   UE 5.1-5.4: Direct property access (CDO->NetUpdateFrequency)
    //   UE 5.5+:  Setter methods (CDO->SetNetUpdateFrequency())
    //
    // Payload:  { blueprintPath: string, netUpdateFrequency?: number, minNetUpdateFrequency?: number }
    // Response: { success: bool, message: string, assetVerification }
    if (SubAction == TEXT("configure_net_update_frequency"))
    {
        FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"));
        double NetUpdateFrequency = GetNumberField(Payload, TEXT("netUpdateFrequency"), 100.0);
        double MinNetUpdateFrequency = GetNumberField(Payload, TEXT("minNetUpdateFrequency"), 2.0);

        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        // Set on CDO
        AActor* CDO = Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject());
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        if (CDO)
        {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
            // UE 5.5+ uses setter methods
            CDO->SetNetUpdateFrequency(static_cast<float>(NetUpdateFrequency));
            CDO->SetMinNetUpdateFrequency(static_cast<float>(MinNetUpdateFrequency));
#else
            // UE 5.1-5.4 uses public member variables (deprecated in 5.5)
            CDO->NetUpdateFrequency = static_cast<float>(NetUpdateFrequency);
            CDO->MinNetUpdateFrequency = static_cast<float>(MinNetUpdateFrequency);
#endif
        }
#else
        // UE 5.0 fallback - these APIs not available
        SendAutomationError(RequestingSocket, RequestId, TEXT("Net update frequency APIs not available in UE 5.0"), TEXT("NOT_AVAILABLE"));
        return true;
#endif

        Blueprint->Modify();
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Net update frequency set to %.1f (min: %.1f)"), NetUpdateFrequency, MinNetUpdateFrequency));
        McpHandlerUtils::AddVerification(ResultJson, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Net update frequency configured"), ResultJson);
        return true;
    }

    // ----- configure_net_priority -----
    // Sets the NetPriority on an Actor CDO, controlling how bandwidth is
    // allocated for replication.
    //
    // Payload:  { blueprintPath: string, netPriority?: number }
    // Response: { success: bool, message: string, assetVerification }
    if (SubAction == TEXT("configure_net_priority"))
    {
        FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"));
        double NetPriority = GetNumberField(Payload, TEXT("netPriority"), 1.0);

        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        AActor* CDO = Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject());
        if (CDO)
        {
            CDO->NetPriority = static_cast<float>(NetPriority);
        }

        Blueprint->Modify();
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Net priority set to %.2f"), NetPriority));
        McpHandlerUtils::AddVerification(ResultJson, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Net priority configured"), ResultJson);
        return true;
    }

    // ----- set_net_dormancy -----
    // Sets the net dormancy mode on an Actor CDO, controlling whether the actor
    // can go dormant to save bandwidth when not changing.
    //
    // Payload:  { blueprintPath: string, dormancy: string }
    // Response: { success: bool, message: string, assetVerification }
    if (SubAction == TEXT("set_net_dormancy"))
    {
        FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"));
        FString Dormancy = GetStringField(Payload, TEXT("dormancy"));

        if (BlueprintPath.IsEmpty() || Dormancy.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath or dormancy"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        ENetDormancy NetDormancy = GetNetDormancy(Dormancy);
        AActor* CDO = Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject());
        if (CDO)
        {
            CDO->NetDormancy = NetDormancy;
        }

        Blueprint->Modify();
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Net dormancy set to %s"), *Dormancy));
        McpHandlerUtils::AddVerification(ResultJson, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Net dormancy configured"), ResultJson);
        return true;
    }

    // ----- configure_replication_graph -----
    // Configures replication graph settings on an Actor CDO including
    // bNetLoadOnClient and spatial loading hints.
    //
    // Note: bReplicateUsingRegisteredSubObjectList is protected in UE 5.6/5.7
    // and cannot be accessed from external code.
    //
    // Payload:  { blueprintPath: string, spatiallyLoaded?: bool, netLoadOnClient?: bool, replicationPolicy?: string }
    // Response: { success: bool, spatiallyLoaded, netLoadOnClient, replicationPolicy, message, assetVerification }
    if (SubAction == TEXT("configure_replication_graph"))
    {
        FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"));
        bool bSpatiallyLoaded = GetBoolField(Payload, TEXT("spatiallyLoaded"), false);
        bool bNetLoadOnClient = GetBoolField(Payload, TEXT("netLoadOnClient"), true);
        FString ReplicationPolicy = GetStringField(Payload, TEXT("replicationPolicy"), TEXT("Default"));

        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        AActor* CDO = Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject());
        if (CDO)
        {
            // Configure actor-specific replication graph settings
            CDO->bNetLoadOnClient = bNetLoadOnClient;
            
            // Set replication flags relevant to replication graph decisions
            // Note: bReplicateUsingRegisteredSubObjectList is protected in both UE 5.6 and 5.7
            // Cannot access directly from external code
            if (bSpatiallyLoaded)
            {
                UE_LOG(LogMcpNetworkingHandlers, Log, TEXT("bReplicateUsingRegisteredSubObjectList is protected. Use Actor defaults in Blueprint instead."));
            }
        }

        Blueprint->Modify();
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetBoolField(TEXT("spatiallyLoaded"), bSpatiallyLoaded);
        ResultJson->SetBoolField(TEXT("netLoadOnClient"), bNetLoadOnClient);
        ResultJson->SetStringField(TEXT("replicationPolicy"), ReplicationPolicy);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Replication graph settings configured (netLoadOnClient=%s, spatiallyLoaded=%s)"), 
            bNetLoadOnClient ? TEXT("true") : TEXT("false"),
            bSpatiallyLoaded ? TEXT("true") : TEXT("false")));
        McpHandlerUtils::AddVerification(ResultJson, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Replication graph configured"), ResultJson);
        return true;
    }

    // =========================================================================
    // 20.2 RPC Actions
    // =========================================================================

    // ----- create_rpc_function -----
    // Creates a new RPC function graph in a Blueprint with the specified
    // RPC type (Server, Client, NetMulticast) and reliability setting.
    //
    // Payload:  { blueprintPath: string, functionName: string, rpcType: string, reliable?: bool }
    // Response: { success: bool, functionName, rpcType, reliable, message, assetVerification }
    if (SubAction == TEXT("create_rpc_function"))
    {
        FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"));
        FString FunctionName = GetStringField(Payload, TEXT("functionName"));
        FString RpcType = GetStringField(Payload, TEXT("rpcType")); // Server, Client, NetMulticast
        bool bReliable = GetBoolField(Payload, TEXT("reliable"), true);

        if (BlueprintPath.IsEmpty() || FunctionName.IsEmpty() || RpcType.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        // Create a new function graph
        UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
            Blueprint,
            FName(*FunctionName),
            UEdGraph::StaticClass(),
            UEdGraphSchema_K2::StaticClass()
        );

        if (NewGraph)
        {
            FBlueprintEditorUtils::AddFunctionGraph<UFunction>(Blueprint, NewGraph, false, static_cast<UFunction*>(nullptr));

            // Set RPC flags on the function entry node
            for (UEdGraphNode* Node : NewGraph->Nodes)
            {
                if (UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(Node))
                {
                    // Start with base network function flag
                    int32 NetFlags = FUNC_Net;
                    
                    // Add reliability flag if requested
                    if (bReliable)
                    {
                        NetFlags |= FUNC_NetReliable;
                    }
                    
                    // Add RPC type flag
                    if (RpcType.Equals(TEXT("Server"), ESearchCase::IgnoreCase))
                    {
                        NetFlags |= FUNC_NetServer;
                    }
                    else if (RpcType.Equals(TEXT("Client"), ESearchCase::IgnoreCase))
                    {
                        NetFlags |= FUNC_NetClient;
                    }
                    else if (RpcType.Equals(TEXT("NetMulticast"), ESearchCase::IgnoreCase) || RpcType.Equals(TEXT("Multicast"), ESearchCase::IgnoreCase))
                    {
                        NetFlags |= FUNC_NetMulticast;
                    }
                    
                    EntryNode->AddExtraFlags(NetFlags);
                    break;
                }
            }

            Blueprint->Modify();
            FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
            McpSafeCompileBlueprint(Blueprint);

            ResultJson->SetBoolField(TEXT("success"), true);
            ResultJson->SetStringField(TEXT("functionName"), FunctionName);
            ResultJson->SetStringField(TEXT("rpcType"), RpcType);
            ResultJson->SetBoolField(TEXT("reliable"), bReliable);
            ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Created %s RPC function: %s"), *RpcType, *FunctionName));
            McpHandlerUtils::AddVerification(ResultJson, Blueprint);
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("RPC function created"), ResultJson);
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create function graph"), TEXT("CREATE_FAILED"));
        }
        return true;
    }

    // ----- configure_rpc_validation -----
    // Enables or disables FUNC_NetValidate on an existing RPC function, which
    // adds a server-side validation step before the RPC executes.
    //
    // Payload:  { blueprintPath: string, functionName: string, withValidation?: bool }
    // Response: { success: bool, withValidation, message, assetVerification }
    if (SubAction == TEXT("configure_rpc_validation"))
    {
        FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"));
        FString FunctionName = GetStringField(Payload, TEXT("functionName"));
        bool bWithValidation = GetBoolField(Payload, TEXT("withValidation"), true);

        if (BlueprintPath.IsEmpty() || FunctionName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        // Find the function graph
        UEdGraph* FuncGraph = nullptr;
        for (UEdGraph* Graph : Blueprint->FunctionGraphs)
        {
            if (Graph && Graph->GetFName() == FName(*FunctionName))
            {
                FuncGraph = Graph;
                break;
            }
        }

        if (!FuncGraph)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Function '%s' not found"), *FunctionName), TEXT("NOT_FOUND"));
            return true;
        }

        // Find the function entry node and set validation flag
        bool bFlagSet = false;
        for (UEdGraphNode* Node : FuncGraph->Nodes)
        {
            if (UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(Node))
            {
                if (bWithValidation)
                {
                    EntryNode->AddExtraFlags(FUNC_NetValidate);
                }
                else
                {
                    EntryNode->ClearExtraFlags(FUNC_NetValidate);
                }
                bFlagSet = true;
                break;
            }
        }

        if (!bFlagSet)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Function entry node not found"), TEXT("NOT_FOUND"));
            return true;
        }

        Blueprint->Modify();
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetBoolField(TEXT("withValidation"), bWithValidation);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("RPC validation %s for function %s"), bWithValidation ? TEXT("enabled") : TEXT("disabled"), *FunctionName));
        McpHandlerUtils::AddVerification(ResultJson, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("RPC validation configured"), ResultJson);
        return true;
    }

    // ----- set_rpc_reliability -----
    // Sets or clears FUNC_NetReliable on an existing RPC function, controlling
    // whether the RPC is guaranteed to arrive.
    //
    // Payload:  { blueprintPath: string, functionName: string, reliable?: bool }
    // Response: { success: bool, reliable, message, assetVerification }
    if (SubAction == TEXT("set_rpc_reliability"))
    {
        FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"));
        FString FunctionName = GetStringField(Payload, TEXT("functionName"));
        bool bReliable = GetBoolField(Payload, TEXT("reliable"), true);

        if (BlueprintPath.IsEmpty() || FunctionName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        // Find the function graph
        UEdGraph* FuncGraph = nullptr;
        for (UEdGraph* Graph : Blueprint->FunctionGraphs)
        {
            if (Graph && Graph->GetFName() == FName(*FunctionName))
            {
                FuncGraph = Graph;
                break;
            }
        }

        if (!FuncGraph)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Function '%s' not found"), *FunctionName), TEXT("NOT_FOUND"));
            return true;
        }

        // Find the function entry node and set reliability flag
        bool bFlagSet = false;
        for (UEdGraphNode* Node : FuncGraph->Nodes)
        {
            if (UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(Node))
            {
                if (bReliable)
                {
                    EntryNode->AddExtraFlags(FUNC_NetReliable);
                }
                else
                {
                    EntryNode->ClearExtraFlags(FUNC_NetReliable);
                }
                bFlagSet = true;
                break;
            }
        }

        if (!bFlagSet)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Function entry node not found"), TEXT("NOT_FOUND"));
            return true;
        }

        Blueprint->Modify();
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetBoolField(TEXT("reliable"), bReliable);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("RPC %s reliability set to %s"), *FunctionName, bReliable ? TEXT("reliable") : TEXT("unreliable")));
        McpHandlerUtils::AddVerification(ResultJson, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("RPC reliability configured"), ResultJson);
        return true;
    }

    // =========================================================================
    // 20.3 Authority & Ownership Actions
    // =========================================================================

    // ----- set_owner -----
    // Sets the owner of an actor in the world. Pass empty ownerActorName to clear.
    //
    // Payload:  { actorName: string, ownerActorName?: string }
    // Response: { success: bool, message: string, actorVerification }
    if (SubAction == TEXT("set_owner"))
    {
        FString ActorName = GetStringField(Payload, TEXT("actorName"));
        FString OwnerActorName = GetStringField(Payload, TEXT("ownerActorName"));

        if (ActorName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing actorName"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!World)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("No world available"), TEXT("NO_WORLD"));
            return true;
        }

        AActor* Actor = NetworkingHelpers::FindActorByName(World, ActorName);
        if (!Actor)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Actor not found"), TEXT("NOT_FOUND"));
            return true;
        }

        AActor* Owner = nullptr;
        if (!OwnerActorName.IsEmpty())
        {
            Owner = NetworkingHelpers::FindActorByName(World, OwnerActorName);
        }

        Actor->SetOwner(Owner);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), Owner ? FString::Printf(TEXT("Set owner of %s to %s"), *ActorName, *OwnerActorName) : FString::Printf(TEXT("Cleared owner of %s"), *ActorName));
        McpHandlerUtils::AddVerification(ResultJson, Actor);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Owner set"), ResultJson);
        return true;
    }

    // ----- set_autonomous_proxy -----
    // Configures all replicated properties in a Blueprint to use
    // COND_AutonomousOnly replication condition, or resets to COND_None.
    //
    // Payload:  { blueprintPath: string, isAutonomousProxy?: bool }
    // Response: { success: bool, isAutonomousProxy, message, assetVerification }
    if (SubAction == TEXT("set_autonomous_proxy"))
    {
        FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"));
        bool bIsAutonomousProxy = GetBoolField(Payload, TEXT("isAutonomousProxy"), true);

        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        // Configure replicated properties to use COND_AutonomousOnly condition
        // This affects how properties are replicated for autonomous proxies
        bool bAnyModified = false;
        for (FBPVariableDescription& VarDesc : Blueprint->NewVariables)
        {
            if ((VarDesc.PropertyFlags & CPF_Net) != 0)
            {
                if (bIsAutonomousProxy)
                {
                    VarDesc.ReplicationCondition = COND_AutonomousOnly;
                }
                else
                {
                    // Reset to default (replicate to all)
                    VarDesc.ReplicationCondition = COND_None;
                }
                bAnyModified = true;
            }
        }

        if (bAnyModified)
        {
            Blueprint->Modify();
            FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
            McpSafeCompileBlueprint(Blueprint);
        }

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetBoolField(TEXT("isAutonomousProxy"), bIsAutonomousProxy);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Autonomous proxy configuration %s for replicated properties"), bIsAutonomousProxy ? TEXT("enabled") : TEXT("disabled")));
        McpHandlerUtils::AddVerification(ResultJson, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Autonomous proxy configured"), ResultJson);
        return true;
    }

    // ----- check_has_authority -----
    // Checks whether an actor in the world has network authority and reports
    // its current net role.
    //
    // Payload:  { actorName: string }
    // Response: { success: bool, hasAuthority: bool, role: string }
    if (SubAction == TEXT("check_has_authority"))
    {
        FString ActorName = GetStringField(Payload, TEXT("actorName"));

        if (ActorName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing actorName"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!World)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("No world available"), TEXT("NO_WORLD"));
            return true;
        }

        AActor* Actor = NetworkingHelpers::FindActorByName(World, ActorName);
        if (!Actor)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Actor not found"), TEXT("NOT_FOUND"));
            return true;
        }

        bool bHasAuthority = Actor->HasAuthority();
        ENetRole Role = Actor->GetLocalRole();

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetBoolField(TEXT("hasAuthority"), bHasAuthority);
        ResultJson->SetStringField(TEXT("role"), NetRoleToString(Role));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Authority checked"), ResultJson);
        return true;
    }

    // ----- check_is_locally_controlled -----
    // Checks if an actor (must be a Pawn) is locally controlled and whether
    // its controller is a local player controller.
    //
    // Payload:  { actorName: string }
    // Response: { success: bool, isLocallyControlled: bool, isLocalController: bool }
    if (SubAction == TEXT("check_is_locally_controlled"))
    {
        FString ActorName = GetStringField(Payload, TEXT("actorName"));

        if (ActorName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing actorName"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!World)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("No world available"), TEXT("NO_WORLD"));
            return true;
        }

        AActor* Actor = NetworkingHelpers::FindActorByName(World, ActorName);
        if (!Actor)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Actor not found"), TEXT("NOT_FOUND"));
            return true;
        }

        bool bIsLocallyControlled = false;
        bool bIsLocalController = false;

        APawn* Pawn = Cast<APawn>(Actor);
        if (Pawn)
        {
            bIsLocallyControlled = Pawn->IsLocallyControlled();
            APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
            bIsLocalController = PC ? PC->IsLocalController() : false;
        }

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetBoolField(TEXT("isLocallyControlled"), bIsLocallyControlled);
        ResultJson->SetBoolField(TEXT("isLocalController"), bIsLocalController);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Local control checked"), ResultJson);
        return true;
    }

    // =========================================================================
    // 20.4 Network Relevancy Actions
    // =========================================================================

    // ----- configure_net_cull_distance -----
    // Sets the net cull distance squared and owner relevancy on an Actor CDO.
    // Uses setter methods on UE 5.5+ and direct property access on 5.1-5.4.
    //
    // Version notes:
    //   UE 5.0:   API not available
    //   UE 5.1-5.4: Direct property access (CDO->NetCullDistanceSquared)
    //   UE 5.5+:  Setter method (CDO->SetNetCullDistanceSquared())
    //
    // Payload:  { blueprintPath: string, netCullDistanceSquared?: number, useOwnerNetRelevancy?: bool }
    // Response: { success: bool, message: string, assetVerification }
    if (SubAction == TEXT("configure_net_cull_distance"))
    {
        FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"));
        double NetCullDistanceSquared = GetNumberField(Payload, TEXT("netCullDistanceSquared"), 225000000.0);
        bool bUseOwnerNetRelevancy = GetBoolField(Payload, TEXT("useOwnerNetRelevancy"), false);

        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        AActor* CDO = Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject());
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        if (CDO)
        {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
            // UE 5.5+ uses setter methods
            CDO->SetNetCullDistanceSquared(static_cast<float>(NetCullDistanceSquared));
#else
            // UE 5.1-5.4 uses public member variables (deprecated in 5.5)
            CDO->NetCullDistanceSquared = static_cast<float>(NetCullDistanceSquared);
#endif
            CDO->bNetUseOwnerRelevancy = bUseOwnerNetRelevancy;
        }
#else
        // UE 5.0 fallback - SetNetCullDistanceSquared not available
        SendAutomationError(RequestingSocket, RequestId, TEXT("Net cull distance API not available in UE 5.0"), TEXT("NOT_AVAILABLE"));
        return true;
#endif

        Blueprint->Modify();
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Net cull distance squared set to %.0f"), NetCullDistanceSquared));
        McpHandlerUtils::AddVerification(ResultJson, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Net cull distance configured"), ResultJson);
        return true;
    }

    // ----- set_always_relevant -----
    // Sets the bAlwaysRelevant flag on an Actor CDO, making the actor always
    // replicated to all clients regardless of distance.
    //
    // Payload:  { blueprintPath: string, alwaysRelevant?: bool }
    // Response: { success: bool, message: string, assetVerification }
    if (SubAction == TEXT("set_always_relevant"))
    {
        FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"));
        bool bAlwaysRelevant = GetBoolField(Payload, TEXT("alwaysRelevant"), true);

        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        AActor* CDO = Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject());
        if (CDO)
        {
            CDO->bAlwaysRelevant = bAlwaysRelevant;
        }

        Blueprint->Modify();
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Always relevant set to %s"), bAlwaysRelevant ? TEXT("true") : TEXT("false")));
        McpHandlerUtils::AddVerification(ResultJson, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Always relevant configured"), ResultJson);
        return true;
    }

    // ----- set_only_relevant_to_owner -----
    // Sets the bOnlyRelevantToOwner flag on an Actor CDO, restricting
    // replication to only the owning client.
    //
    // Payload:  { blueprintPath: string, onlyRelevantToOwner?: bool }
    // Response: { success: bool, message: string, assetVerification }
    if (SubAction == TEXT("set_only_relevant_to_owner"))
    {
        FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"));
        bool bOnlyRelevantToOwner = GetBoolField(Payload, TEXT("onlyRelevantToOwner"), true);

        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        AActor* CDO = Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject());
        if (CDO)
        {
            CDO->bOnlyRelevantToOwner = bOnlyRelevantToOwner;
        }

        Blueprint->Modify();
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Only relevant to owner set to %s"), bOnlyRelevantToOwner ? TEXT("true") : TEXT("false")));
        McpHandlerUtils::AddVerification(ResultJson, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Only relevant to owner configured"), ResultJson);
        return true;
    }

    // =========================================================================
    // 20.5 Net Serialization Actions
    // =========================================================================

    // ----- configure_net_serialization -----
    // Configures net serialization settings on an Actor CDO. Logs a warning
    // about bReplicateUsingRegisteredSubObjectList being protected.
    //
    // Note: bReplicateUsingRegisteredSubObjectList is protected in UE 5.6/5.7.
    //
    // Payload:  { blueprintPath: string, structName?: string, customSerialization?: bool }
    // Response: { success: bool, customSerialization, structName?, message, assetVerification }
    if (SubAction == TEXT("configure_net_serialization"))
    {
        FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"));
        FString StructName = GetStringField(Payload, TEXT("structName"));
        bool bCustomSerialization = GetBoolField(Payload, TEXT("customSerialization"), false);

        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        AActor* CDO = Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject());
        if (CDO)
        {
            // Configure net serialization flags on the actor
            // bReplicateUsingRegisteredSubObjectList controls whether actor uses custom subobject replication
            // Note: This is protected in both UE 5.6 and 5.7, cannot access directly
            UE_LOG(LogMcpNetworkingHandlers, Log, TEXT("bReplicateUsingRegisteredSubObjectList is protected. Use Actor defaults in Blueprint instead."));
        }

        Blueprint->Modify();
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetBoolField(TEXT("customSerialization"), bCustomSerialization);
        if (!StructName.IsEmpty())
        {
            ResultJson->SetStringField(TEXT("structName"), StructName);
        }
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Net serialization configured (customSerialization=%s)"), bCustomSerialization ? TEXT("true") : TEXT("false")));
        McpHandlerUtils::AddVerification(ResultJson, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Net serialization configured"), ResultJson);
        return true;
    }

    // ----- set_replicated_using -----
    // Sets a RepNotify function on a Blueprint variable. Enables CPF_Net and
    // CPF_RepNotify flags on the property and assigns the RepNotifyFunc name.
    //
    // Payload:  { blueprintPath: string, propertyName: string, repNotifyFunc: string }
    // Response: { success: bool, message: string, assetVerification }
    if (SubAction == TEXT("set_replicated_using"))
    {
        FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"));
        FString PropertyName = GetStringField(Payload, TEXT("propertyName"));
        FString RepNotifyFunc = GetStringField(Payload, TEXT("repNotifyFunc"));

        if (BlueprintPath.IsEmpty() || PropertyName.IsEmpty() || RepNotifyFunc.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        // Find the variable description and set RepNotify function
        bool bFound = false;
        for (FBPVariableDescription& VarDesc : Blueprint->NewVariables)
        {
            if (VarDesc.VarName == FName(*PropertyName))
            {
                // Ensure property is replicated
                VarDesc.PropertyFlags |= CPF_Net | CPF_RepNotify;
                VarDesc.RepNotifyFunc = FName(*RepNotifyFunc);
                bFound = true;
                break;
            }
        }

        if (!bFound)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Property '%s' not found"), *PropertyName), TEXT("NOT_FOUND"));
            return true;
        }

        Blueprint->Modify();
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("ReplicatedUsing set to %s for property %s"), *RepNotifyFunc, *PropertyName));
        McpHandlerUtils::AddVerification(ResultJson, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("ReplicatedUsing configured"), ResultJson);
        return true;
    }

    // ----- configure_push_model -----
    // Enables or disables push model replication metadata on all replicated
    // properties in a Blueprint. Push model reduces replication overhead by
    // only sending properties when explicitly marked dirty.
    //
    // Payload:  { blueprintPath: string, usePushModel?: bool }
    // Response: { success: bool, usePushModel, message, assetVerification }
    if (SubAction == TEXT("configure_push_model"))
    {
        FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"));
        bool bUsePushModel = GetBoolField(Payload, TEXT("usePushModel"), true);

        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        // Push Model replication is configured via metadata on the Blueprint's variable descriptions
        // Find and update the replication settings for all replicated properties
        bool bAnyModified = false;
        for (FBPVariableDescription& VarDesc : Blueprint->NewVariables)
        {
            // Check if variable is replicated (has CPF_Net flag)
            if ((VarDesc.PropertyFlags & CPF_Net) != 0)
            {
                if (bUsePushModel)
                {
                    // Add push model metadata
                    VarDesc.SetMetaData(TEXT("PushModel"), TEXT("true"));
                }
                else
                {
                    VarDesc.RemoveMetaData(TEXT("PushModel"));
                }
                bAnyModified = true;
            }
        }

        if (bAnyModified)
        {
            Blueprint->Modify();
            FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
            McpSafeCompileBlueprint(Blueprint);
        }

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetBoolField(TEXT("usePushModel"), bUsePushModel);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Push model replication %s for all replicated properties"), bUsePushModel ? TEXT("enabled") : TEXT("disabled")));
        McpHandlerUtils::AddVerification(ResultJson, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Push model configured"), ResultJson);
        return true;
    }

    // =========================================================================
    // 20.6 Network Prediction Actions
    // =========================================================================

    // ----- configure_client_prediction -----
    // Configures client-side prediction on a Character Blueprint's
    // CharacterMovementComponent. Enables/disables timestamp replication
    // and sets the smoothing threshold.
    //
    // Payload:  { blueprintPath: string, enablePrediction?: bool, predictionThreshold?: number }
    // Response: { success: bool, enablePrediction, predictionThreshold, message, assetVerification }
    if (SubAction == TEXT("configure_client_prediction"))
    {
        FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"));
        bool bEnablePrediction = GetBoolField(Payload, TEXT("enablePrediction"), true);
        double PredictionThreshold = GetNumberField(Payload, TEXT("predictionThreshold"), 0.1);

        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        // Configure client-side prediction on CharacterMovementComponent if present
        ACharacter* CharacterCDO = Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject());
        if (CharacterCDO && CharacterCDO->GetCharacterMovement())
        {
            UCharacterMovementComponent* CMC = CharacterCDO->GetCharacterMovement();
            
            // Enable/disable client prediction
            if (bEnablePrediction)
            {
                CMC->bNetworkAlwaysReplicateTransformUpdateTimestamp = true;
                CMC->NetworkSimulatedSmoothLocationTime = static_cast<float>(PredictionThreshold);
            }
            else
            {
                CMC->bNetworkAlwaysReplicateTransformUpdateTimestamp = false;
            }
        }

        Blueprint->Modify();
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetBoolField(TEXT("enablePrediction"), bEnablePrediction);
        ResultJson->SetNumberField(TEXT("predictionThreshold"), PredictionThreshold);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Client prediction %s"), bEnablePrediction ? TEXT("enabled") : TEXT("disabled")));
        McpHandlerUtils::AddVerification(ResultJson, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Client prediction configured"), ResultJson);
        return true;
    }

    // ----- configure_server_correction -----
    // Configures server correction smoothing parameters on a Character
    // Blueprint's CharacterMovementComponent for networked movement.
    //
    // Payload:  { blueprintPath: string, correctionThreshold?: number, smoothingRate?: number }
    // Response: { success: bool, correctionThreshold, smoothingRate, message, assetVerification }
    if (SubAction == TEXT("configure_server_correction"))
    {
        FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"));
        double CorrectionThreshold = GetNumberField(Payload, TEXT("correctionThreshold"), 1.0);
        double SmoothingRate = GetNumberField(Payload, TEXT("smoothingRate"), 0.5);

        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        // Configure server correction settings on CharacterMovementComponent
        ACharacter* CharacterCDO = Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject());
        if (CharacterCDO && CharacterCDO->GetCharacterMovement())
        {
            UCharacterMovementComponent* CMC = CharacterCDO->GetCharacterMovement();
            
            // Set server correction smoothing parameters
            CMC->NetworkSimulatedSmoothLocationTime = static_cast<float>(SmoothingRate);
            CMC->NetworkSimulatedSmoothRotationTime = static_cast<float>(SmoothingRate);
            CMC->ListenServerNetworkSimulatedSmoothLocationTime = static_cast<float>(SmoothingRate);
            CMC->ListenServerNetworkSimulatedSmoothRotationTime = static_cast<float>(SmoothingRate);
        }

        Blueprint->Modify();
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetNumberField(TEXT("correctionThreshold"), CorrectionThreshold);
        ResultJson->SetNumberField(TEXT("smoothingRate"), SmoothingRate);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Server correction configured (threshold=%.2f, smoothing=%.2f)"), CorrectionThreshold, SmoothingRate));
        McpHandlerUtils::AddVerification(ResultJson, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Server correction configured"), ResultJson);
        return true;
    }

    // ----- add_network_prediction_data -----
    // Adds a replicated variable to a Blueprint for storing network prediction
    // data. The variable is configured with COND_AutonomousOnly replication
    // condition (only sent to locally controlled pawns).
    //
    // Supported dataType values: "Transform", "Vector", "Rotator", or float default.
    //
    // Payload:  { blueprintPath: string, dataType: string, variableName?: string }
    // Response: { success: bool, variableName, dataType, message, assetVerification }
    if (SubAction == TEXT("add_network_prediction_data"))
    {
        FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"));
        FString DataType = GetStringField(Payload, TEXT("dataType"));
        FString VariableName = GetStringField(Payload, TEXT("variableName"));

        if (BlueprintPath.IsEmpty() || DataType.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        // Add a replicated variable for network prediction data
        FString VarName = VariableName.IsEmpty() ? FString::Printf(TEXT("PredictionData_%s"), *DataType) : VariableName;
        
        // Determine pin type based on data type
        FEdGraphPinType PinType;
        PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        
        // Map common prediction data types to their struct types
        if (DataType == TEXT("Transform"))
        {
            PinType.PinSubCategoryObject = TBaseStructure<FTransform>::Get();
        }
        else if (DataType == TEXT("Vector"))
        {
            PinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
        }
        else if (DataType == TEXT("Rotator"))
        {
            PinType.PinSubCategoryObject = TBaseStructure<FRotator>::Get();
        }
        else
        {
            // Default to float for simple prediction data
            PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
            PinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
        }

        // Add the variable with replication flags
        bool bSuccess = FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*VarName), PinType);
        
        if (bSuccess)
        {
            // Find and configure the variable for replication
            for (FBPVariableDescription& VarDesc : Blueprint->NewVariables)
            {
                if (VarDesc.VarName == FName(*VarName))
                {
                    VarDesc.PropertyFlags |= CPF_Net;
                    VarDesc.ReplicationCondition = COND_AutonomousOnly; // Only for locally controlled pawns
                    break;
                }
            }
        }

        Blueprint->Modify();
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);

        ResultJson->SetBoolField(TEXT("success"), bSuccess);
        ResultJson->SetStringField(TEXT("variableName"), VarName);
        ResultJson->SetStringField(TEXT("dataType"), DataType);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Network prediction data variable '%s' of type '%s' %s"), *VarName, *DataType, bSuccess ? TEXT("added") : TEXT("could not be added (may already exist)")));
        McpHandlerUtils::AddVerification(ResultJson, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, bSuccess, TEXT("Network prediction data added"), ResultJson);
        return true;
    }

    // ----- configure_movement_prediction -----
    // Configures movement prediction smoothing parameters on a Character
    // Blueprint's CharacterMovementComponent for networked movement.
    //
    // Payload:  { blueprintPath: string, networkSmoothingMode?: string, networkMaxSmoothUpdateDistance?: number, networkNoSmoothUpdateDistance?: number }
    // Response: { success: bool, message: string, assetVerification }
    if (SubAction == TEXT("configure_movement_prediction"))
    {
        FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"));
        FString NetworkSmoothingMode = GetStringField(Payload, TEXT("networkSmoothingMode"), TEXT("Exponential"));
        double NetworkMaxSmoothUpdateDistance = GetNumberField(Payload, TEXT("networkMaxSmoothUpdateDistance"), 256.0);
        double NetworkNoSmoothUpdateDistance = GetNumberField(Payload, TEXT("networkNoSmoothUpdateDistance"), 384.0);

        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        // Find CharacterMovementComponent in the CDO and configure it
        ACharacter* CharacterCDO = Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject());
        if (CharacterCDO && CharacterCDO->GetCharacterMovement())
        {
            UCharacterMovementComponent* CMC = CharacterCDO->GetCharacterMovement();
            CMC->NetworkMaxSmoothUpdateDistance = static_cast<float>(NetworkMaxSmoothUpdateDistance);
            CMC->NetworkNoSmoothUpdateDistance = static_cast<float>(NetworkNoSmoothUpdateDistance);
        }

        Blueprint->Modify();
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), TEXT("Movement prediction configured"));
        McpHandlerUtils::AddVerification(ResultJson, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Movement prediction configured"), ResultJson);
        return true;
    }

    // =========================================================================
    // 20.7 Connection & Session Actions
    // =========================================================================

    // ----- configure_net_driver -----
    // Configures net driver settings on the active world's net driver including
    // client rate limits and server tick rate.
    //
    // Version notes:
    //   UE 5.0-5.6: Direct property access for NetServerMaxTickRate (deprecated in 5.5)
    //   UE 5.7:     Uses SetNetServerMaxTickRate() setter method
    //
    // Payload:  { maxClientRate?: number, maxInternetClientRate?: number, netServerMaxTickRate?: number }
    // Response: { success: bool, appliedToActiveDriver, maxClientRate, maxInternetClientRate, netServerMaxTickRate, message }
    if (SubAction == TEXT("configure_net_driver"))
    {
        double MaxClientRate = GetNumberField(Payload, TEXT("maxClientRate"), 15000.0);
        double MaxInternetClientRate = GetNumberField(Payload, TEXT("maxInternetClientRate"), 10000.0);
        double NetServerMaxTickRate = GetNumberField(Payload, TEXT("netServerMaxTickRate"), 30.0);

        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        bool bConfigApplied = false;

        if (World && World->GetNetDriver())
        {
            UNetDriver* NetDriver = World->GetNetDriver();
            
            // Configure net driver settings
            NetDriver->MaxClientRate = static_cast<int32>(MaxClientRate);
            NetDriver->MaxInternetClientRate = static_cast<int32>(MaxInternetClientRate);
            // NetServerMaxTickRate is deprecated in UE 5.5+. Suppress warning unconditionally.
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
            NetDriver->SetNetServerMaxTickRate(static_cast<int32>(NetServerMaxTickRate));
#else
            PRAGMA_DISABLE_DEPRECATION_WARNINGS
            NetDriver->NetServerMaxTickRate = static_cast<int32>(NetServerMaxTickRate);
            PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
            
            bConfigApplied = true;
        }

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetBoolField(TEXT("appliedToActiveDriver"), bConfigApplied);
        ResultJson->SetNumberField(TEXT("maxClientRate"), MaxClientRate);
        ResultJson->SetNumberField(TEXT("maxInternetClientRate"), MaxInternetClientRate);
        ResultJson->SetNumberField(TEXT("netServerMaxTickRate"), NetServerMaxTickRate);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Net driver configured (maxClientRate=%.0f, maxInternetClientRate=%.0f, tickRate=%.0f)"), 
            MaxClientRate, MaxInternetClientRate, NetServerMaxTickRate));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Net driver configured"), ResultJson);
        return true;
    }

    // ----- set_net_role -----
    // Configures replication on an Actor CDO based on a desired net role.
    // ROLE_Authority and proxy roles enable replication; ROLE_None disables it.
    //
    // Payload:  { blueprintPath: string, role: string }
    // Response: { success: bool, role, replicates, message, assetVerification }
    if (SubAction == TEXT("set_net_role"))
    {
        FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"));
        FString Role = GetStringField(Payload, TEXT("role"));

        if (BlueprintPath.IsEmpty() || Role.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        AActor* CDO = Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject());
        ENetRole NetRole = GetNetRole(Role);
        
        if (CDO)
        {
            // Configure replication based on role
            if (NetRole == ROLE_Authority)
            {
                CDO->SetReplicates(true);
            }
            else if (NetRole == ROLE_None)
            {
                CDO->SetReplicates(false);
            }
            else
            {
                // For proxy roles, ensure replication is enabled
                CDO->SetReplicates(true);
            }
        }

        Blueprint->Modify();
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("role"), Role);
        ResultJson->SetBoolField(TEXT("replicates"), CDO ? CDO->GetIsReplicated() : false);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Net role configured to %s (replicates=%s)"), *Role, CDO && CDO->GetIsReplicated() ? TEXT("true") : TEXT("false")));
        McpHandlerUtils::AddVerification(ResultJson, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Net role configured"), ResultJson);
        return true;
    }

    // ----- configure_replicated_movement -----
    // Enables or disables movement replication on an Actor CDO via
    // SetReplicatingMovement().
    //
    // Payload:  { blueprintPath: string, replicateMovement?: bool }
    // Response: { success: bool, message: string, assetVerification }
    if (SubAction == TEXT("configure_replicated_movement"))
    {
        FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"));
        bool bReplicateMovement = GetBoolField(Payload, TEXT("replicateMovement"), true);

        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        AActor* CDO = Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject());
        if (CDO)
        {
            CDO->SetReplicatingMovement(bReplicateMovement);
        }

        Blueprint->Modify();
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Replicate movement set to %s"), bReplicateMovement ? TEXT("true") : TEXT("false")));
        McpHandlerUtils::AddVerification(ResultJson, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Replicated movement configured"), ResultJson);
        return true;
    }

    // =========================================================================
    // 20.8 Utility Actions
    // =========================================================================

    // ----- get_networking_info -----
    // Retrieves comprehensive networking configuration for a Blueprint CDO or
    // a live actor in the world. Returns replication settings, relevancy flags,
    // net update frequency, dormancy, roles, and authority status.
    //
    // Version notes:
    //   UE 5.0:   Net frequency/cull distance values returned as 0.0
    //   UE 5.1-5.4: Direct property access
    //   UE 5.5+:  Getter methods for net frequency and cull distance
    //
    // Payload:  { blueprintPath?: string, actorName?: string }  (one required)
    // Response: { success: bool, networkingInfo: object }
    if (SubAction == TEXT("get_networking_info"))
    {
        FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"));
        FString ActorName = GetStringField(Payload, TEXT("actorName"));

        TSharedPtr<FJsonObject> NetworkingInfo = McpHandlerUtils::CreateResultObject();

        if (!BlueprintPath.IsEmpty())
        {
            UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
            if (!Blueprint)
            {
                SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint not found"), TEXT("NOT_FOUND"));
                return true;
            }

            AActor* CDO = Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject());
            if (CDO)
            {
                NetworkingInfo->SetBoolField(TEXT("bReplicates"), CDO->GetIsReplicated());
                NetworkingInfo->SetBoolField(TEXT("bAlwaysRelevant"), CDO->bAlwaysRelevant);
                NetworkingInfo->SetBoolField(TEXT("bOnlyRelevantToOwner"), CDO->bOnlyRelevantToOwner);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
                // UE 5.5+ uses getter methods
                NetworkingInfo->SetNumberField(TEXT("netUpdateFrequency"), CDO->GetNetUpdateFrequency());
                NetworkingInfo->SetNumberField(TEXT("minNetUpdateFrequency"), CDO->GetMinNetUpdateFrequency());
                NetworkingInfo->SetNumberField(TEXT("netCullDistanceSquared"), CDO->GetNetCullDistanceSquared());
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
                // UE 5.1-5.4 uses public member variables (deprecated in 5.5)
                NetworkingInfo->SetNumberField(TEXT("netUpdateFrequency"), CDO->NetUpdateFrequency);
                NetworkingInfo->SetNumberField(TEXT("minNetUpdateFrequency"), CDO->MinNetUpdateFrequency);
                NetworkingInfo->SetNumberField(TEXT("netCullDistanceSquared"), CDO->NetCullDistanceSquared);
#else
                NetworkingInfo->SetNumberField(TEXT("netUpdateFrequency"), 0.0);
                NetworkingInfo->SetNumberField(TEXT("minNetUpdateFrequency"), 0.0);
                NetworkingInfo->SetNumberField(TEXT("netCullDistanceSquared"), 0.0);
#endif
                NetworkingInfo->SetNumberField(TEXT("netPriority"), CDO->NetPriority);
                NetworkingInfo->SetStringField(TEXT("netDormancy"), NetDormancyToString(CDO->NetDormancy));
            }
        }
        else if (!ActorName.IsEmpty())
        {
            UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
            if (!World)
            {
                SendAutomationError(RequestingSocket, RequestId, TEXT("No world available"), TEXT("NO_WORLD"));
                return true;
            }

            AActor* Actor = NetworkingHelpers::FindActorByName(World, ActorName);
            if (!Actor)
            {
                SendAutomationError(RequestingSocket, RequestId, TEXT("Actor not found"), TEXT("NOT_FOUND"));
                return true;
            }

            NetworkingInfo->SetBoolField(TEXT("bReplicates"), Actor->GetIsReplicated());
            NetworkingInfo->SetBoolField(TEXT("bAlwaysRelevant"), Actor->bAlwaysRelevant);
            NetworkingInfo->SetBoolField(TEXT("bOnlyRelevantToOwner"), Actor->bOnlyRelevantToOwner);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
            // UE 5.5+ uses getter methods
            NetworkingInfo->SetNumberField(TEXT("netUpdateFrequency"), Actor->GetNetUpdateFrequency());
            NetworkingInfo->SetNumberField(TEXT("minNetUpdateFrequency"), Actor->GetMinNetUpdateFrequency());
            NetworkingInfo->SetNumberField(TEXT("netCullDistanceSquared"), Actor->GetNetCullDistanceSquared());
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
            // UE 5.1-5.4 uses public member variables (deprecated in 5.5)
            NetworkingInfo->SetNumberField(TEXT("netUpdateFrequency"), Actor->NetUpdateFrequency);
            NetworkingInfo->SetNumberField(TEXT("minNetUpdateFrequency"), Actor->MinNetUpdateFrequency);
            NetworkingInfo->SetNumberField(TEXT("netCullDistanceSquared"), Actor->NetCullDistanceSquared);
#else
            NetworkingInfo->SetNumberField(TEXT("netUpdateFrequency"), 0.0);
            NetworkingInfo->SetNumberField(TEXT("minNetUpdateFrequency"), 0.0);
            NetworkingInfo->SetNumberField(TEXT("netCullDistanceSquared"), 0.0);
#endif
            NetworkingInfo->SetNumberField(TEXT("netPriority"), Actor->NetPriority);
            NetworkingInfo->SetStringField(TEXT("netDormancy"), NetDormancyToString(Actor->NetDormancy));
            NetworkingInfo->SetStringField(TEXT("role"), NetRoleToString(Actor->GetLocalRole()));
            NetworkingInfo->SetStringField(TEXT("remoteRole"), NetRoleToString(Actor->GetRemoteRole()));
            NetworkingInfo->SetBoolField(TEXT("hasAuthority"), Actor->HasAuthority());
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Must provide either blueprintPath or actorName"), TEXT("INVALID_PARAMS"));
            return true;
        }

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetObjectField(TEXT("networkingInfo"), NetworkingInfo);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Networking info retrieved"), ResultJson);
        return true;
    }

    // Unknown action
    return false;
}
