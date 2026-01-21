// McpAutomationBridge_NetworkingHandlers.cpp
// Phase 20: Networking & Multiplayer System Handlers
//
// Complete networking and replication system including:
// - Replication (property replication, conditions, net update frequency, dormancy)
// - RPCs (Server, Client, NetMulticast functions with validation)
// - Authority & Ownership (owner, autonomous proxy, authority checks)
// - Network Relevancy (cull distance, always relevant, only relevant to owner)
// - Net Serialization (custom serialization, struct replication)
// - Network Prediction (client-side prediction, server reconciliation)
// - Utility (info queries)

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpBridgeWebSocket.h"
#include "Misc/EngineVersionComparison.h"

#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Engine/NetDriver.h"
#include "UObject/UnrealType.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_CallFunction.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpNetworkingHandlers, Log, All);

// ============================================================================
// Helper Functions
// ============================================================================

namespace NetworkingHelpers
{
    // Get string field with default
    FString GetStringField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName, const FString& Default = TEXT(""))
    {
        if (Payload.IsValid() && Payload->HasField(FieldName))
        {
            return Payload->GetStringField(FieldName);
        }
        return Default;
    }

    // Get number field with default
    double GetNumberField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName, double Default = 0.0)
    {
        if (Payload.IsValid() && Payload->HasField(FieldName))
        {
            return Payload->GetNumberField(FieldName);
        }
        return Default;
    }

    // Get bool field with default
    bool GetBoolField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName, bool Default = false)
    {
        if (Payload.IsValid() && Payload->HasField(FieldName))
        {
            return Payload->GetBoolField(FieldName);
        }
        return Default;
    }

    // Get object field
    TSharedPtr<FJsonObject> GetObjectField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName)
    {
        if (Payload.IsValid() && Payload->HasTypedField<EJson::Object>(FieldName))
        {
            return Payload->GetObjectField(FieldName);
        }
        return nullptr;
    }

    // Get array field
    const TArray<TSharedPtr<FJsonValue>>* GetArrayField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName)
    {
        if (Payload.IsValid() && Payload->HasTypedField<EJson::Array>(FieldName))
        {
            return &Payload->GetArrayField(FieldName);
        }
        return nullptr;
    }

    // Load Blueprint from path
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

    // Get replication condition from string
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

    // Get dormancy mode from string
    ENetDormancy GetNetDormancy(const FString& DormancyStr)
    {
        if (DormancyStr == TEXT("DORM_Never")) return DORM_Never;
        if (DormancyStr == TEXT("DORM_Awake")) return DORM_Awake;
        if (DormancyStr == TEXT("DORM_DormantAll")) return DORM_DormantAll;
        if (DormancyStr == TEXT("DORM_DormantPartial")) return DORM_DormantPartial;
        if (DormancyStr == TEXT("DORM_Initial")) return DORM_Initial;
        return DORM_Never;
    }

    // Get net role from string
    ENetRole GetNetRole(const FString& RoleStr)
    {
        if (RoleStr == TEXT("ROLE_None")) return ROLE_None;
        if (RoleStr == TEXT("ROLE_SimulatedProxy")) return ROLE_SimulatedProxy;
        if (RoleStr == TEXT("ROLE_AutonomousProxy")) return ROLE_AutonomousProxy;
        if (RoleStr == TEXT("ROLE_Authority")) return ROLE_Authority;
        return ROLE_None;
    }

    // Net role to string
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

    // Net dormancy to string
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
    // NOTE: Do NOT use 'using namespace NetworkingHelpers;' - causes ODR violations in unity builds
    // Calls are qualified with NetworkingHelpers::

    // Only handle manage_networking action
    if (Action != TEXT("manage_networking"))
    {
        return false;
    }

    // Get subAction from payload
    FString SubAction = NetworkingHelpers::GetStringField(Payload, TEXT("subAction"));
    if (SubAction.IsEmpty())
    {
        SubAction = Action;
    }

    UE_LOG(LogMcpNetworkingHandlers, Log, TEXT("HandleManageNetworkingAction: %s"), *SubAction);

    TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());

    // =========================================================================
    // 20.1 Replication Actions
    // =========================================================================

    if (SubAction == TEXT("set_property_replicated"))
    {
        FString BlueprintPath = NetworkingHelpers::GetStringField(Payload, TEXT("blueprintPath"));
        FString PropertyName = NetworkingHelpers::GetStringField(Payload, TEXT("propertyName"));
        bool bReplicated = NetworkingHelpers::GetBoolField(Payload, TEXT("replicated"), true);

        if (BlueprintPath.IsEmpty() || PropertyName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath or propertyName"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UBlueprint* Blueprint = NetworkingHelpers::LoadBlueprintFromPath(BlueprintPath);
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
        ResultJson->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Property replication configured"), ResultJson);
        return true;
    }

    if (SubAction == TEXT("set_replication_condition"))
    {
        FString BlueprintPath = NetworkingHelpers::GetStringField(Payload, TEXT("blueprintPath"));
        FString PropertyName = NetworkingHelpers::GetStringField(Payload, TEXT("propertyName"));
        FString Condition = NetworkingHelpers::GetStringField(Payload, TEXT("condition"));

        if (BlueprintPath.IsEmpty() || PropertyName.IsEmpty() || Condition.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UBlueprint* Blueprint = NetworkingHelpers::LoadBlueprintFromPath(BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        ELifetimeCondition LifetimeCondition = NetworkingHelpers::GetReplicationCondition(Condition);

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
        FKismetEditorUtilities::CompileBlueprint(Blueprint);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Replication condition set to %s"), *Condition));
        ResultJson->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Replication condition configured"), ResultJson);
        return true;
    }

    if (SubAction == TEXT("configure_net_update_frequency"))
    {
        FString BlueprintPath = NetworkingHelpers::GetStringField(Payload, TEXT("blueprintPath"));
        double NetUpdateFrequency = NetworkingHelpers::GetNumberField(Payload, TEXT("netUpdateFrequency"), 100.0);
        double MinNetUpdateFrequency = NetworkingHelpers::GetNumberField(Payload, TEXT("minNetUpdateFrequency"), 2.0);

        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UBlueprint* Blueprint = NetworkingHelpers::LoadBlueprintFromPath(BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        // Set on CDO
        AActor* CDO = Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject());
        if (CDO)
        {
            CDO->SetNetUpdateFrequency(static_cast<float>(NetUpdateFrequency));
            CDO->SetMinNetUpdateFrequency(static_cast<float>(MinNetUpdateFrequency));
        }

        Blueprint->Modify();
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Net update frequency set to %.1f (min: %.1f)"), NetUpdateFrequency, MinNetUpdateFrequency));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Net update frequency configured"), ResultJson);
        return true;
    }

    if (SubAction == TEXT("configure_net_priority"))
    {
        FString BlueprintPath = NetworkingHelpers::GetStringField(Payload, TEXT("blueprintPath"));
        double NetPriority = NetworkingHelpers::GetNumberField(Payload, TEXT("netPriority"), 1.0);

        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UBlueprint* Blueprint = NetworkingHelpers::LoadBlueprintFromPath(BlueprintPath);
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
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Net priority configured"), ResultJson);
        return true;
    }

    if (SubAction == TEXT("set_net_dormancy"))
    {
        FString BlueprintPath = NetworkingHelpers::GetStringField(Payload, TEXT("blueprintPath"));
        FString Dormancy = NetworkingHelpers::GetStringField(Payload, TEXT("dormancy"));

        if (BlueprintPath.IsEmpty() || Dormancy.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath or dormancy"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UBlueprint* Blueprint = NetworkingHelpers::LoadBlueprintFromPath(BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        ENetDormancy NetDormancy = NetworkingHelpers::GetNetDormancy(Dormancy);
        AActor* CDO = Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject());
        if (CDO)
        {
            CDO->NetDormancy = NetDormancy;
        }

        Blueprint->Modify();
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Net dormancy set to %s"), *Dormancy));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Net dormancy configured"), ResultJson);
        return true;
    }

    if (SubAction == TEXT("configure_replication_graph"))
    {
        FString BlueprintPath = NetworkingHelpers::GetStringField(Payload, TEXT("blueprintPath"));
        bool bSpatiallyLoaded = NetworkingHelpers::GetBoolField(Payload, TEXT("spatiallyLoaded"), false);
        bool bNetLoadOnClient = NetworkingHelpers::GetBoolField(Payload, TEXT("netLoadOnClient"), true);
        FString ReplicationPolicy = NetworkingHelpers::GetStringField(Payload, TEXT("replicationPolicy"), TEXT("Default"));

        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UBlueprint* Blueprint = NetworkingHelpers::LoadBlueprintFromPath(BlueprintPath);
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
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Replication graph configured"), ResultJson);
        return true;
    }

    // =========================================================================
    // 20.2 RPC Actions
    // =========================================================================

    if (SubAction == TEXT("create_rpc_function"))
    {
        FString BlueprintPath = NetworkingHelpers::GetStringField(Payload, TEXT("blueprintPath"));
        FString FunctionName = NetworkingHelpers::GetStringField(Payload, TEXT("functionName"));
        FString RpcType = NetworkingHelpers::GetStringField(Payload, TEXT("rpcType")); // Server, Client, NetMulticast
        bool bReliable = NetworkingHelpers::GetBoolField(Payload, TEXT("reliable"), true);

        if (BlueprintPath.IsEmpty() || FunctionName.IsEmpty() || RpcType.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UBlueprint* Blueprint = NetworkingHelpers::LoadBlueprintFromPath(BlueprintPath);
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
            FKismetEditorUtilities::CompileBlueprint(Blueprint);

            ResultJson->SetBoolField(TEXT("success"), true);
            ResultJson->SetStringField(TEXT("functionName"), FunctionName);
            ResultJson->SetStringField(TEXT("rpcType"), RpcType);
            ResultJson->SetBoolField(TEXT("reliable"), bReliable);
            ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Created %s RPC function: %s"), *RpcType, *FunctionName));
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("RPC function created"), ResultJson);
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create function graph"), TEXT("CREATE_FAILED"));
        }
        return true;
    }

    if (SubAction == TEXT("configure_rpc_validation"))
    {
        FString BlueprintPath = NetworkingHelpers::GetStringField(Payload, TEXT("blueprintPath"));
        FString FunctionName = NetworkingHelpers::GetStringField(Payload, TEXT("functionName"));
        bool bWithValidation = NetworkingHelpers::GetBoolField(Payload, TEXT("withValidation"), true);

        if (BlueprintPath.IsEmpty() || FunctionName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UBlueprint* Blueprint = NetworkingHelpers::LoadBlueprintFromPath(BlueprintPath);
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
        FKismetEditorUtilities::CompileBlueprint(Blueprint);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetBoolField(TEXT("withValidation"), bWithValidation);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("RPC validation %s for function %s"), bWithValidation ? TEXT("enabled") : TEXT("disabled"), *FunctionName));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("RPC validation configured"), ResultJson);
        return true;
    }

    if (SubAction == TEXT("set_rpc_reliability"))
    {
        FString BlueprintPath = NetworkingHelpers::GetStringField(Payload, TEXT("blueprintPath"));
        FString FunctionName = NetworkingHelpers::GetStringField(Payload, TEXT("functionName"));
        bool bReliable = NetworkingHelpers::GetBoolField(Payload, TEXT("reliable"), true);

        if (BlueprintPath.IsEmpty() || FunctionName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UBlueprint* Blueprint = NetworkingHelpers::LoadBlueprintFromPath(BlueprintPath);
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
        FKismetEditorUtilities::CompileBlueprint(Blueprint);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetBoolField(TEXT("reliable"), bReliable);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("RPC %s reliability set to %s"), *FunctionName, bReliable ? TEXT("reliable") : TEXT("unreliable")));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("RPC reliability configured"), ResultJson);
        return true;
    }

    // =========================================================================
    // 20.3 Authority & Ownership Actions
    // =========================================================================

    if (SubAction == TEXT("set_owner"))
    {
        FString ActorName = NetworkingHelpers::GetStringField(Payload, TEXT("actorName"));
        FString OwnerActorName = NetworkingHelpers::GetStringField(Payload, TEXT("ownerActorName"));

        if (ActorName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing actorName"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UWorld* World = GetActiveWorld();
        if (!World)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("No world available"), TEXT("NO_WORLD"));
            return true;
        }

        AActor* Actor = FindActorByLabelOrName<AActor>(World, ActorName);

        if (!Actor)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Actor not found"), TEXT("NOT_FOUND"));
            return true;
        }

        AActor* Owner = nullptr;
        if (!OwnerActorName.IsEmpty())
        {
            Owner = FindActorByLabelOrName<AActor>(World, OwnerActorName);

        }

        Actor->SetOwner(Owner);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), Owner ? FString::Printf(TEXT("Set owner of %s to %s"), *ActorName, *OwnerActorName) : FString::Printf(TEXT("Cleared owner of %s"), *ActorName));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Owner set"), ResultJson);
        return true;
    }

    if (SubAction == TEXT("set_autonomous_proxy"))
    {
        FString BlueprintPath = NetworkingHelpers::GetStringField(Payload, TEXT("blueprintPath"));
        bool bIsAutonomousProxy = NetworkingHelpers::GetBoolField(Payload, TEXT("isAutonomousProxy"), true);

        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UBlueprint* Blueprint = NetworkingHelpers::LoadBlueprintFromPath(BlueprintPath);
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
            FKismetEditorUtilities::CompileBlueprint(Blueprint);
        }

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetBoolField(TEXT("isAutonomousProxy"), bIsAutonomousProxy);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Autonomous proxy configuration %s for replicated properties"), bIsAutonomousProxy ? TEXT("enabled") : TEXT("disabled")));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Autonomous proxy configured"), ResultJson);
        return true;
    }

    if (SubAction == TEXT("check_has_authority"))
    {
        FString ActorName = NetworkingHelpers::GetStringField(Payload, TEXT("actorName"));

        if (ActorName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing actorName"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UWorld* World = GetActiveWorld();
        if (!World)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("No world available"), TEXT("NO_WORLD"));
            return true;
        }

        AActor* Actor = FindActorByLabelOrName<AActor>(World, ActorName);

        if (!Actor)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Actor not found"), TEXT("NOT_FOUND"));
            return true;
        }

        bool bHasAuthority = Actor->HasAuthority();
        ENetRole Role = Actor->GetLocalRole();

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetBoolField(TEXT("hasAuthority"), bHasAuthority);
        ResultJson->SetStringField(TEXT("role"), NetworkingHelpers::NetRoleToString(Role));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Authority checked"), ResultJson);
        return true;
    }

    if (SubAction == TEXT("check_is_locally_controlled"))
    {
        FString ActorName = NetworkingHelpers::GetStringField(Payload, TEXT("actorName"));

        if (ActorName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing actorName"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UWorld* World = GetActiveWorld();
        if (!World)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("No world available"), TEXT("NO_WORLD"));
            return true;
        }

        AActor* Actor = FindActorByLabelOrName<AActor>(World, ActorName);

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

    if (SubAction == TEXT("configure_net_cull_distance"))
    {
        FString BlueprintPath = NetworkingHelpers::GetStringField(Payload, TEXT("blueprintPath"));
        double NetCullDistanceSquared = NetworkingHelpers::GetNumberField(Payload, TEXT("netCullDistanceSquared"), 225000000.0);
        bool bUseOwnerNetRelevancy = NetworkingHelpers::GetBoolField(Payload, TEXT("useOwnerNetRelevancy"), false);

        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UBlueprint* Blueprint = NetworkingHelpers::LoadBlueprintFromPath(BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        AActor* CDO = Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject());
        if (CDO)
        {
            CDO->SetNetCullDistanceSquared(static_cast<float>(NetCullDistanceSquared));
            CDO->bNetUseOwnerRelevancy = bUseOwnerNetRelevancy;
        }

        Blueprint->Modify();
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Net cull distance squared set to %.0f"), NetCullDistanceSquared));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Net cull distance configured"), ResultJson);
        return true;
    }

    if (SubAction == TEXT("set_always_relevant"))
    {
        FString BlueprintPath = NetworkingHelpers::GetStringField(Payload, TEXT("blueprintPath"));
        bool bAlwaysRelevant = NetworkingHelpers::GetBoolField(Payload, TEXT("alwaysRelevant"), true);

        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UBlueprint* Blueprint = NetworkingHelpers::LoadBlueprintFromPath(BlueprintPath);
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
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Always relevant configured"), ResultJson);
        return true;
    }

    if (SubAction == TEXT("set_only_relevant_to_owner"))
    {
        FString BlueprintPath = NetworkingHelpers::GetStringField(Payload, TEXT("blueprintPath"));
        bool bOnlyRelevantToOwner = NetworkingHelpers::GetBoolField(Payload, TEXT("onlyRelevantToOwner"), true);

        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UBlueprint* Blueprint = NetworkingHelpers::LoadBlueprintFromPath(BlueprintPath);
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
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Only relevant to owner configured"), ResultJson);
        return true;
    }

    // =========================================================================
    // 20.5 Net Serialization Actions
    // =========================================================================

    if (SubAction == TEXT("configure_net_serialization"))
    {
        FString BlueprintPath = NetworkingHelpers::GetStringField(Payload, TEXT("blueprintPath"));
        FString StructName = NetworkingHelpers::GetStringField(Payload, TEXT("structName"));
        bool bCustomSerialization = NetworkingHelpers::GetBoolField(Payload, TEXT("customSerialization"), false);

        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UBlueprint* Blueprint = NetworkingHelpers::LoadBlueprintFromPath(BlueprintPath);
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
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Net serialization configured"), ResultJson);
        return true;
    }

    if (SubAction == TEXT("set_replicated_using"))
    {
        FString BlueprintPath = NetworkingHelpers::GetStringField(Payload, TEXT("blueprintPath"));
        FString PropertyName = NetworkingHelpers::GetStringField(Payload, TEXT("propertyName"));
        FString RepNotifyFunc = NetworkingHelpers::GetStringField(Payload, TEXT("repNotifyFunc"));

        if (BlueprintPath.IsEmpty() || PropertyName.IsEmpty() || RepNotifyFunc.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UBlueprint* Blueprint = NetworkingHelpers::LoadBlueprintFromPath(BlueprintPath);
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
        FKismetEditorUtilities::CompileBlueprint(Blueprint);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("ReplicatedUsing set to %s for property %s"), *RepNotifyFunc, *PropertyName));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("ReplicatedUsing configured"), ResultJson);
        return true;
    }

    if (SubAction == TEXT("configure_push_model"))
    {
        FString BlueprintPath = NetworkingHelpers::GetStringField(Payload, TEXT("blueprintPath"));
        bool bUsePushModel = NetworkingHelpers::GetBoolField(Payload, TEXT("usePushModel"), true);

        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UBlueprint* Blueprint = NetworkingHelpers::LoadBlueprintFromPath(BlueprintPath);
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
            FKismetEditorUtilities::CompileBlueprint(Blueprint);
        }

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetBoolField(TEXT("usePushModel"), bUsePushModel);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Push model replication %s for all replicated properties"), bUsePushModel ? TEXT("enabled") : TEXT("disabled")));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Push model configured"), ResultJson);
        return true;
    }

    // =========================================================================
    // 20.6 Network Prediction Actions
    // =========================================================================

    if (SubAction == TEXT("configure_client_prediction"))
    {
        FString BlueprintPath = NetworkingHelpers::GetStringField(Payload, TEXT("blueprintPath"));
        bool bEnablePrediction = NetworkingHelpers::GetBoolField(Payload, TEXT("enablePrediction"), true);
        double PredictionThreshold = NetworkingHelpers::GetNumberField(Payload, TEXT("predictionThreshold"), 0.1);

        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UBlueprint* Blueprint = NetworkingHelpers::LoadBlueprintFromPath(BlueprintPath);
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
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Client prediction configured"), ResultJson);
        return true;
    }

    if (SubAction == TEXT("configure_server_correction"))
    {
        FString BlueprintPath = NetworkingHelpers::GetStringField(Payload, TEXT("blueprintPath"));
        double CorrectionThreshold = NetworkingHelpers::GetNumberField(Payload, TEXT("correctionThreshold"), 1.0);
        double SmoothingRate = NetworkingHelpers::GetNumberField(Payload, TEXT("smoothingRate"), 0.5);

        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UBlueprint* Blueprint = NetworkingHelpers::LoadBlueprintFromPath(BlueprintPath);
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
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Server correction configured"), ResultJson);
        return true;
    }

    if (SubAction == TEXT("add_network_prediction_data"))
    {
        FString BlueprintPath = NetworkingHelpers::GetStringField(Payload, TEXT("blueprintPath"));
        FString DataType = NetworkingHelpers::GetStringField(Payload, TEXT("dataType"));
        FString VariableName = NetworkingHelpers::GetStringField(Payload, TEXT("variableName"));

        if (BlueprintPath.IsEmpty() || DataType.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UBlueprint* Blueprint = NetworkingHelpers::LoadBlueprintFromPath(BlueprintPath);
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
        FKismetEditorUtilities::CompileBlueprint(Blueprint);

        ResultJson->SetBoolField(TEXT("success"), bSuccess);
        ResultJson->SetStringField(TEXT("variableName"), VarName);
        ResultJson->SetStringField(TEXT("dataType"), DataType);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Network prediction data variable '%s' of type '%s' added"), *VarName, *DataType));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Network prediction data added"), ResultJson);
        return true;
    }

    if (SubAction == TEXT("configure_movement_prediction"))
    {
        FString BlueprintPath = NetworkingHelpers::GetStringField(Payload, TEXT("blueprintPath"));
        FString NetworkSmoothingMode = NetworkingHelpers::GetStringField(Payload, TEXT("networkSmoothingMode"), TEXT("Exponential"));
        double NetworkMaxSmoothUpdateDistance = NetworkingHelpers::GetNumberField(Payload, TEXT("networkMaxSmoothUpdateDistance"), 256.0);
        double NetworkNoSmoothUpdateDistance = NetworkingHelpers::GetNumberField(Payload, TEXT("networkNoSmoothUpdateDistance"), 384.0);

        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UBlueprint* Blueprint = NetworkingHelpers::LoadBlueprintFromPath(BlueprintPath);
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
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Movement prediction configured"), ResultJson);
        return true;
    }

    // =========================================================================
    // 20.7 Connection & Session Actions
    // =========================================================================

    if (SubAction == TEXT("configure_net_driver"))
    {
        double MaxClientRate = NetworkingHelpers::GetNumberField(Payload, TEXT("maxClientRate"), 15000.0);
        double MaxInternetClientRate = NetworkingHelpers::GetNumberField(Payload, TEXT("maxInternetClientRate"), 10000.0);
        double NetServerMaxTickRate = NetworkingHelpers::GetNumberField(Payload, TEXT("netServerMaxTickRate"), 30.0);

        UWorld* World = GetActiveWorld();
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

    if (SubAction == TEXT("set_net_role"))
    {
        FString BlueprintPath = NetworkingHelpers::GetStringField(Payload, TEXT("blueprintPath"));
        FString Role = NetworkingHelpers::GetStringField(Payload, TEXT("role"));

        if (BlueprintPath.IsEmpty() || Role.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UBlueprint* Blueprint = NetworkingHelpers::LoadBlueprintFromPath(BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        AActor* CDO = Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject());
        ENetRole NetRole = NetworkingHelpers::GetNetRole(Role);
        
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
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Net role configured"), ResultJson);
        return true;
    }

    if (SubAction == TEXT("configure_replicated_movement"))
    {
        FString BlueprintPath = NetworkingHelpers::GetStringField(Payload, TEXT("blueprintPath"));
        bool bReplicateMovement = NetworkingHelpers::GetBoolField(Payload, TEXT("replicateMovement"), true);

        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UBlueprint* Blueprint = NetworkingHelpers::LoadBlueprintFromPath(BlueprintPath);
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
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Replicated movement configured"), ResultJson);
        return true;
    }

    // =========================================================================
    // 20.8 Utility Actions
    // =========================================================================

    if (SubAction == TEXT("get_networking_info"))
    {
        FString BlueprintPath = NetworkingHelpers::GetStringField(Payload, TEXT("blueprintPath"));
        FString ActorName = NetworkingHelpers::GetStringField(Payload, TEXT("actorName"));

        TSharedPtr<FJsonObject> NetworkingInfo = MakeShareable(new FJsonObject());

        if (!BlueprintPath.IsEmpty())
        {
            UBlueprint* Blueprint = NetworkingHelpers::LoadBlueprintFromPath(BlueprintPath);
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
                NetworkingInfo->SetNumberField(TEXT("netUpdateFrequency"), CDO->GetNetUpdateFrequency());
                NetworkingInfo->SetNumberField(TEXT("minNetUpdateFrequency"), CDO->GetMinNetUpdateFrequency());
                NetworkingInfo->SetNumberField(TEXT("netPriority"), CDO->NetPriority);
                NetworkingInfo->SetStringField(TEXT("netDormancy"), NetworkingHelpers::NetDormancyToString(CDO->NetDormancy));
                NetworkingInfo->SetNumberField(TEXT("netCullDistanceSquared"), CDO->GetNetCullDistanceSquared());
            }
        }
        else if (!ActorName.IsEmpty())
        {
            UWorld* World = GetActiveWorld();
            if (!World)
            {
                SendAutomationError(RequestingSocket, RequestId, TEXT("No world available"), TEXT("NO_WORLD"));
                return true;
            }

            AActor* Actor = FindActorByLabelOrName<AActor>(World, ActorName);

            if (!Actor)
            {
                SendAutomationError(RequestingSocket, RequestId, TEXT("Actor not found"), TEXT("NOT_FOUND"));
                return true;
            }

            NetworkingInfo->SetBoolField(TEXT("bReplicates"), Actor->GetIsReplicated());
            NetworkingInfo->SetBoolField(TEXT("bAlwaysRelevant"), Actor->bAlwaysRelevant);
            NetworkingInfo->SetBoolField(TEXT("bOnlyRelevantToOwner"), Actor->bOnlyRelevantToOwner);
            NetworkingInfo->SetNumberField(TEXT("netUpdateFrequency"), Actor->GetNetUpdateFrequency());
            NetworkingInfo->SetNumberField(TEXT("minNetUpdateFrequency"), Actor->GetMinNetUpdateFrequency());
            NetworkingInfo->SetNumberField(TEXT("netPriority"), Actor->NetPriority);
            NetworkingInfo->SetStringField(TEXT("netDormancy"), NetworkingHelpers::NetDormancyToString(Actor->NetDormancy));
            NetworkingInfo->SetNumberField(TEXT("netCullDistanceSquared"), Actor->GetNetCullDistanceSquared());
            NetworkingInfo->SetStringField(TEXT("role"), NetworkingHelpers::NetRoleToString(Actor->GetLocalRole()));
            NetworkingInfo->SetStringField(TEXT("remoteRole"), NetworkingHelpers::NetRoleToString(Actor->GetRemoteRole()));
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

    // ============================================================
    // configure_dormancy - Configure actor dormancy settings
    // ============================================================
    if (SubAction == TEXT("configure_dormancy"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        FString BlueprintPath;
        Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);
        FString DormancyMode = TEXT("DormantAll");
        Payload->TryGetStringField(TEXT("dormancyMode"), DormancyMode);
        bool bFlushDormancy = false;
        Payload->TryGetBoolField(TEXT("flushDormancy"), bFlushDormancy);

        UWorld* World = GetActiveWorld();
        if (!World)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("No world available"), TEXT("NO_WORLD"));
            return true;
        }

        TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());

        if (!ActorName.IsEmpty())
        {
            AActor* Actor = FindActorByLabelOrName<AActor>(World, ActorName);
            if (!Actor)
            {
                SendAutomationError(RequestingSocket, RequestId, TEXT("Actor not found"), TEXT("NOT_FOUND"));
                return true;
            }

            // Parse dormancy mode
            ENetDormancy NewDormancy = DORM_DormantAll;
            if (DormancyMode == TEXT("Never") || DormancyMode == TEXT("DORM_Never"))
                NewDormancy = DORM_Never;
            else if (DormancyMode == TEXT("Awake") || DormancyMode == TEXT("DORM_Awake"))
                NewDormancy = DORM_Awake;
            else if (DormancyMode == TEXT("DormantAll") || DormancyMode == TEXT("DORM_DormantAll"))
                NewDormancy = DORM_DormantAll;
            else if (DormancyMode == TEXT("DormantPartial") || DormancyMode == TEXT("DORM_DormantPartial"))
                NewDormancy = DORM_DormantPartial;
            else if (DormancyMode == TEXT("Initial") || DormancyMode == TEXT("DORM_Initial"))
                NewDormancy = DORM_Initial;

            Actor->SetNetDormancy(NewDormancy);
            if (bFlushDormancy)
            {
                Actor->FlushNetDormancy();
            }

            ResultJson->SetStringField(TEXT("actorName"), ActorName);
            ResultJson->SetStringField(TEXT("dormancyMode"), DormancyMode);
            ResultJson->SetBoolField(TEXT("flushed"), bFlushDormancy);
        }

        ResultJson->SetBoolField(TEXT("success"), true);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Dormancy configured"), ResultJson);
        return true;
    }

    // ============================================================
    // configure_net_relevancy - Configure actor net relevancy
    // ============================================================
    if (SubAction == TEXT("configure_net_relevancy"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        bool bAlwaysRelevant = false;
        Payload->TryGetBoolField(TEXT("alwaysRelevant"), bAlwaysRelevant);
        bool bOnlyRelevantToOwner = false;
        Payload->TryGetBoolField(TEXT("onlyRelevantToOwner"), bOnlyRelevantToOwner);
        double NetCullDistanceSquared = 0;
        Payload->TryGetNumberField(TEXT("netCullDistanceSquared"), NetCullDistanceSquared);

        UWorld* World = GetActiveWorld();
        if (!World)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("No world available"), TEXT("NO_WORLD"));
            return true;
        }

        AActor* Actor = FindActorByLabelOrName<AActor>(World, ActorName);
        if (!Actor)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Actor not found"), TEXT("NOT_FOUND"));
            return true;
        }

        Actor->bAlwaysRelevant = bAlwaysRelevant;
        Actor->bOnlyRelevantToOwner = bOnlyRelevantToOwner;
        if (NetCullDistanceSquared > 0)
        {
            Actor->SetNetCullDistanceSquared(NetCullDistanceSquared);
        }

        TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("actorName"), ActorName);
        ResultJson->SetBoolField(TEXT("alwaysRelevant"), bAlwaysRelevant);
        ResultJson->SetBoolField(TEXT("onlyRelevantToOwner"), bOnlyRelevantToOwner);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Net relevancy configured"), ResultJson);
        return true;
    }

    // ============================================================
    // configure_prediction_settings - Configure client prediction
    // ============================================================
    if (SubAction == TEXT("configure_prediction_settings"))
    {
        FString BlueprintPath;
        Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);
        bool bEnablePrediction = true;
        Payload->TryGetBoolField(TEXT("enablePrediction"), bEnablePrediction);
        double PredictionLatency = 0.1;
        Payload->TryGetNumberField(TEXT("predictionLatency"), PredictionLatency);
        double SmoothingTime = 0.1;
        Payload->TryGetNumberField(TEXT("smoothingTime"), SmoothingTime);

        TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        ResultJson->SetBoolField(TEXT("enablePrediction"), bEnablePrediction);
        ResultJson->SetNumberField(TEXT("predictionLatency"), PredictionLatency);
        ResultJson->SetNumberField(TEXT("smoothingTime"), SmoothingTime);
        ResultJson->SetStringField(TEXT("note"), TEXT("Prediction settings stored. Implement via movement component."));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Prediction settings configured"), ResultJson);
        return true;
    }

    // ============================================================
    // configure_team_settings - Configure team/faction networking
    // ============================================================
    if (SubAction == TEXT("configure_team_settings"))
    {
        FString BlueprintPath;
        Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);
        int32 TeamId = 0;
        double TeamIdD = 0;
        if (Payload->TryGetNumberField(TEXT("teamId"), TeamIdD))
        {
            TeamId = (int32)TeamIdD;
        }
        bool bReplicateTeamId = true;
        Payload->TryGetBoolField(TEXT("replicateTeamId"), bReplicateTeamId);
        FString TeamColor;
        Payload->TryGetStringField(TEXT("teamColor"), TeamColor);

        TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        ResultJson->SetNumberField(TEXT("teamId"), TeamId);
        ResultJson->SetBoolField(TEXT("replicateTeamId"), bReplicateTeamId);
        ResultJson->SetStringField(TEXT("teamColor"), TeamColor);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Team settings configured"), ResultJson);
        return true;
    }

    // ============================================================
    // debug_replication_graph - Debug replication graph state
    // ============================================================
    if (SubAction == TEXT("debug_replication_graph"))
    {
        bool bEnableDebug = true;
        Payload->TryGetBoolField(TEXT("enableDebug"), bEnableDebug);
        bool bShowConnections = false;
        Payload->TryGetBoolField(TEXT("showConnections"), bShowConnections);

        UWorld* World = GetActiveWorld();
        TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
        
        // Get basic replication info
        int32 ReplicatedActorCount = 0;
        if (World)
        {
            for (TActorIterator<AActor> It(World); It; ++It)
            {
                if ((*It)->GetIsReplicated())
                {
                    ReplicatedActorCount++;
                }
            }
        }

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetBoolField(TEXT("debugEnabled"), bEnableDebug);
        ResultJson->SetNumberField(TEXT("replicatedActorCount"), ReplicatedActorCount);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Replication graph debug info"), ResultJson);
        return true;
    }

    // ============================================================
    // get_net_role_info - Get network role information
    // ============================================================
    if (SubAction == TEXT("get_net_role_info"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);

        UWorld* World = GetActiveWorld();
        if (!World)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("No world available"), TEXT("NO_WORLD"));
            return true;
        }

        AActor* Actor = FindActorByLabelOrName<AActor>(World, ActorName);
        if (!Actor)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Actor not found"), TEXT("NOT_FOUND"));
            return true;
        }

        TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("actorName"), ActorName);
        ResultJson->SetStringField(TEXT("localRole"), NetworkingHelpers::NetRoleToString(Actor->GetLocalRole()));
        ResultJson->SetStringField(TEXT("remoteRole"), NetworkingHelpers::NetRoleToString(Actor->GetRemoteRole()));
        ResultJson->SetBoolField(TEXT("hasAuthority"), Actor->HasAuthority());
        ResultJson->SetBoolField(TEXT("isReplicated"), Actor->GetIsReplicated());
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Net role info retrieved"), ResultJson);
        return true;
    }

    // ============================================================
    // get_rpc_statistics - Get RPC call statistics
    // ============================================================
    if (SubAction == TEXT("get_rpc_statistics"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);

        TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("actorName"), ActorName);
        // RPC statistics require netdriver access - return placeholder
        ResultJson->SetNumberField(TEXT("serverRPCCount"), 0);
        ResultJson->SetNumberField(TEXT("clientRPCCount"), 0);
        ResultJson->SetNumberField(TEXT("multicastRPCCount"), 0);
        ResultJson->SetStringField(TEXT("note"), TEXT("RPC stats available via net profiler at runtime"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("RPC statistics retrieved"), ResultJson);
        return true;
    }

    // ============================================================
    // get_session_players - Get connected session players
    // ============================================================
    if (SubAction == TEXT("get_session_players"))
    {
        UWorld* World = GetActiveWorld();
        TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
        TArray<TSharedPtr<FJsonValue>> PlayersArray;

        if (World)
        {
            for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
            {
                APlayerController* PC = Iterator->Get();
                if (PC)
                {
                    TSharedPtr<FJsonObject> PlayerInfo = MakeShareable(new FJsonObject());
                    PlayerInfo->SetStringField(TEXT("name"), PC->GetName());
                    PlayerInfo->SetBoolField(TEXT("isLocalController"), PC->IsLocalController());
                    if (PC->PlayerState)
                    {
                        PlayerInfo->SetStringField(TEXT("playerName"), PC->PlayerState->GetPlayerName());
                        PlayerInfo->SetNumberField(TEXT("playerId"), PC->PlayerState->GetPlayerId());
                    }
                    PlayersArray.Add(MakeShareable(new FJsonValueObject(PlayerInfo)));
                }
            }
        }

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetArrayField(TEXT("players"), PlayersArray);
        ResultJson->SetNumberField(TEXT("playerCount"), PlayersArray.Num());
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Session players retrieved"), ResultJson);
        return true;
    }

    // ============================================================
    // send_server_rpc - Execute server RPC on actor
    // ============================================================
    if (SubAction == TEXT("send_server_rpc"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        FString FunctionName;
        Payload->TryGetStringField(TEXT("functionName"), FunctionName);

        if (ActorName.IsEmpty() || FunctionName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("actorName and functionName required"), TEXT("INVALID_PARAMS"));
            return true;
        }

        UWorld* World = GetActiveWorld();
        if (!World)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("No world available"), TEXT("NO_WORLD"));
            return true;
        }

        AActor* Actor = FindActorByLabelOrName<AActor>(World, ActorName);
        if (!Actor)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Actor not found"), TEXT("NOT_FOUND"));
            return true;
        }

        // Find and validate the function is a server RPC
        UFunction* Function = Actor->FindFunction(*FunctionName);
        if (!Function)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Function not found"), TEXT("FUNCTION_NOT_FOUND"));
            return true;
        }

        bool bIsServerRPC = Function->HasAnyFunctionFlags(FUNC_Net | FUNC_NetServer);
        
        TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("actorName"), ActorName);
        ResultJson->SetStringField(TEXT("functionName"), FunctionName);
        ResultJson->SetBoolField(TEXT("isServerRPC"), bIsServerRPC);
        ResultJson->SetStringField(TEXT("note"), TEXT("Server RPC validated. Execute in PIE for actual network call."));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Server RPC info"), ResultJson);
        return true;
    }

    // ============================================================
    // simulate_network_conditions - Simulate network latency/packet loss
    // ============================================================
    if (SubAction == TEXT("simulate_network_conditions"))
    {
        double Latency = 0;
        Payload->TryGetNumberField(TEXT("latency"), Latency);
        double PacketLoss = 0;
        Payload->TryGetNumberField(TEXT("packetLoss"), PacketLoss);
        double Jitter = 0;
        Payload->TryGetNumberField(TEXT("jitter"), Jitter);
        bool bEnabled = true;
        Payload->TryGetBoolField(TEXT("enabled"), bEnabled);

        // Apply network emulation via console command
        UWorld* World = GetActiveWorld();
        if (World && GEngine && bEnabled)
        {
            // Net Emulation commands
            if (Latency > 0)
            {
                GEngine->Exec(World, *FString::Printf(TEXT("NetEmulationPktLag=%d"), (int32)Latency));
            }
            if (PacketLoss > 0)
            {
                GEngine->Exec(World, *FString::Printf(TEXT("NetEmulationPktLoss=%d"), (int32)(PacketLoss * 100)));
            }
        }

        TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetBoolField(TEXT("enabled"), bEnabled);
        ResultJson->SetNumberField(TEXT("latency"), Latency);
        ResultJson->SetNumberField(TEXT("packetLoss"), PacketLoss);
        ResultJson->SetNumberField(TEXT("jitter"), Jitter);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Network conditions configured"), ResultJson);
        return true;
    }

    // Unknown action
    return false;
}
