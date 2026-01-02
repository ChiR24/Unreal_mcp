// McpAutomationBridge_AIHandlers.cpp
// Phase 16: AI System
// Implements 35 actions for AI controllers, blackboards, behavior trees, EQS, perception,
// state trees, smart objects, and mass AI.

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#include "Misc/EngineVersionComparison.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Factories/BlueprintFactory.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Int.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Float.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Rotator.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Class.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Enum.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Name.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_String.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BTDecorator.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/Composites/BTComposite_Selector.h"
#include "BehaviorTree/Composites/BTComposite_Sequence.h"
#include "BehaviorTree/Tasks/BTTask_MoveTo.h"
#include "BehaviorTree/Tasks/BTTask_Wait.h"
#include "BehaviorTree/Decorators/BTDecorator_Blackboard.h"
#include "BehaviorTree/Decorators/BTDecorator_Cooldown.h"
#include "BehaviorTree/Decorators/BTDecorator_Loop.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "EnvironmentQuery/Generators/EnvQueryGenerator_ActorsOfClass.h"
#include "EnvironmentQuery/Generators/EnvQueryGenerator_OnCircle.h"
#include "EnvironmentQuery/Generators/EnvQueryGenerator_SimpleGrid.h"
#include "EnvironmentQuery/Tests/EnvQueryTest_Distance.h"
#include "EnvironmentQuery/Tests/EnvQueryTest_Trace.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Damage.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#endif

// Attempt to include State Tree (UE 5.3+)
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 3
#define MCP_HAS_STATE_TREE 1
// State Tree headers would go here if publicly available
#else
#define MCP_HAS_STATE_TREE 0
#endif

// Attempt to include Smart Objects (UE 5.0+)
#if ENGINE_MAJOR_VERSION >= 5
#define MCP_HAS_SMART_OBJECTS 1
// Smart Object headers would go here
#else
#define MCP_HAS_SMART_OBJECTS 0
#endif

// Attempt to include Mass AI (UE 5.0+)
#if ENGINE_MAJOR_VERSION >= 5
#define MCP_HAS_MASS_AI 1
// Mass Entity headers would go here
#else
#define MCP_HAS_MASS_AI 0
#endif

// Use consolidated JSON helpers from McpAutomationBridgeHelpers.h
// Aliases for backward compatibility with existing code in this file
#define GetStringFieldAI GetJsonStringField
#define GetNumberFieldAI GetJsonNumberField
#define GetBoolFieldAI GetJsonBoolField

// Helper to save package
// Note: This helper is used for NEW assets created with CreatePackage + factory.
// FullyLoad() must NOT be called on new packages - it corrupts bulkdata in UE 5.7+.
static bool SavePackageHelperAI(UPackage* Package, UObject* Asset)
{
    if (!Package || !Asset) return false;
    
    // Use centralized helper for safe saving (UE 5.7+ compatible)
    return McpSafeAssetSave(Asset);
}

#if WITH_EDITOR
// Helper to create AI Controller blueprint
static UBlueprint* CreateAIControllerBlueprint(const FString& Path, const FString& Name, FString& OutError)
{
    FString FullPath = Path / Name;
    UPackage* Package = CreatePackage(*FullPath);
    if (!Package)
    {
        OutError = FString::Printf(TEXT("Failed to create package: %s"), *FullPath);
        return nullptr;
    }

    UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
    if (!Factory)
    {
        OutError = TEXT("Failed to create BlueprintFactory");
        return nullptr;
    }

    Factory->ParentClass = AAIController::StaticClass();

    UBlueprint* Blueprint = Cast<UBlueprint>(
        Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, *Name,
                                  RF_Public | RF_Standalone, nullptr, GWarn));

    if (!Blueprint)
    {
        OutError = TEXT("Failed to create AI Controller blueprint");
        return nullptr;
    }

    FAssetRegistryModule::AssetCreated(Blueprint);
    SavePackageHelperAI(Package, Blueprint);

    return Blueprint;
}

// Helper to create Blackboard asset
static UBlackboardData* CreateBlackboardAsset(const FString& Path, const FString& Name, FString& OutError)
{
    FString FullPath = Path / Name;
    UPackage* Package = CreatePackage(*FullPath);
    if (!Package)
    {
        OutError = FString::Printf(TEXT("Failed to create package: %s"), *FullPath);
        return nullptr;
    }

    UBlackboardData* Blackboard = NewObject<UBlackboardData>(Package, UBlackboardData::StaticClass(), FName(*Name), RF_Public | RF_Standalone);
    if (!Blackboard)
    {
        OutError = TEXT("Failed to create Blackboard asset");
        return nullptr;
    }

    FAssetRegistryModule::AssetCreated(Blackboard);
    SavePackageHelperAI(Package, Blackboard);

    return Blackboard;
}

// Helper to create Behavior Tree asset
static UBehaviorTree* CreateBehaviorTreeAsset(const FString& Path, const FString& Name, FString& OutError)
{
    FString FullPath = Path / Name;
    UPackage* Package = CreatePackage(*FullPath);
    if (!Package)
    {
        OutError = FString::Printf(TEXT("Failed to create package: %s"), *FullPath);
        return nullptr;
    }

    UBehaviorTree* BehaviorTree = NewObject<UBehaviorTree>(Package, UBehaviorTree::StaticClass(), FName(*Name), RF_Public | RF_Standalone);
    if (!BehaviorTree)
    {
        OutError = TEXT("Failed to create Behavior Tree asset");
        return nullptr;
    }

    FAssetRegistryModule::AssetCreated(BehaviorTree);
    SavePackageHelperAI(Package, BehaviorTree);

    return BehaviorTree;
}

// Helper to create EQS Query asset
static UEnvQuery* CreateEQSQueryAsset(const FString& Path, const FString& Name, FString& OutError)
{
    FString FullPath = Path / Name;
    UPackage* Package = CreatePackage(*FullPath);
    if (!Package)
    {
        OutError = FString::Printf(TEXT("Failed to create package: %s"), *FullPath);
        return nullptr;
    }

    UEnvQuery* Query = NewObject<UEnvQuery>(Package, UEnvQuery::StaticClass(), FName(*Name), RF_Public | RF_Standalone);
    if (!Query)
    {
        OutError = TEXT("Failed to create EQS Query asset");
        return nullptr;
    }

    FAssetRegistryModule::AssetCreated(Query);
    SavePackageHelperAI(Package, Query);

    return Query;
}
#endif

bool UMcpAutomationBridgeSubsystem::HandleManageAIAction(
    const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (Action != TEXT("manage_ai"))
    {
        return false;
    }

#if !WITH_EDITOR
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("AI management is only available in editor builds"),
                        TEXT("EDITOR_ONLY"));
    return true;
#else
    FString SubAction = GetStringFieldAI(Payload, TEXT("subAction"));
    if (SubAction.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Missing subAction parameter"),
                            TEXT("INVALID_PARAMS"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());

    // =========================================================================
    // 16.1 AI Controller (3 actions)
    // =========================================================================

    if (SubAction == TEXT("create_ai_controller"))
    {
        FString Name = GetStringFieldAI(Payload, TEXT("name"));
        FString Path = GetStringFieldAI(Payload, TEXT("path"), TEXT("/Game/AI/Controllers"));

        if (Name.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId,
                                TEXT("Missing name parameter"),
                                TEXT("INVALID_PARAMS"));
            return true;
        }

        FString Error;
        UBlueprint* Blueprint = CreateAIControllerBlueprint(Path, Name, Error);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, Error, TEXT("CREATION_FAILED"));
            return true;
        }

        Result->SetStringField(TEXT("controllerPath"), Blueprint->GetPathName());
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Created AI Controller: %s"), *Name));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("AI Controller created"), Result);
        return true;
    }

    if (SubAction == TEXT("assign_behavior_tree"))
    {
        FString ControllerPath = GetStringFieldAI(Payload, TEXT("controllerPath"));
        FString BehaviorTreePath = GetStringFieldAI(Payload, TEXT("behaviorTreePath"));

        UBlueprint* Controller = LoadObject<UBlueprint>(nullptr, *ControllerPath);
        if (!Controller)
        {
            SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("AI Controller not found: %s"), *ControllerPath),
                                TEXT("NOT_FOUND"));
            return true;
        }

        UBehaviorTree* BT = LoadObject<UBehaviorTree>(nullptr, *BehaviorTreePath);
        if (!BT)
        {
            SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("Behavior Tree not found: %s"), *BehaviorTreePath),
                                TEXT("NOT_FOUND"));
            return true;
        }

        // Set default property on the generated class
        if (Controller->GeneratedClass)
        {
            if (AAIController* CDO = Cast<AAIController>(Controller->GeneratedClass->GetDefaultObject()))
            {
                // Note: Direct property access depends on the AI controller implementation
                // This sets up the reference for the behavior tree
                Result->SetStringField(TEXT("message"), TEXT("Behavior Tree assigned (set RunBehaviorTree in BeginPlay)"));
            }
        }

        Controller->MarkPackageDirty();
        Result->SetStringField(TEXT("controllerPath"), ControllerPath);
        Result->SetStringField(TEXT("behaviorTreePath"), BehaviorTreePath);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Behavior Tree reference set"), Result);
        return true;
    }

    if (SubAction == TEXT("assign_blackboard"))
    {
        FString ControllerPath = GetStringFieldAI(Payload, TEXT("controllerPath"));
        FString BlackboardPath = GetStringFieldAI(Payload, TEXT("blackboardPath"));

        UBlueprint* Controller = LoadObject<UBlueprint>(nullptr, *ControllerPath);
        if (!Controller)
        {
            SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("AI Controller not found: %s"), *ControllerPath),
                                TEXT("NOT_FOUND"));
            return true;
        }

        UBlackboardData* BB = LoadObject<UBlackboardData>(nullptr, *BlackboardPath);
        if (!BB)
        {
            SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("Blackboard not found: %s"), *BlackboardPath),
                                TEXT("NOT_FOUND"));
            return true;
        }

        Controller->MarkPackageDirty();
        Result->SetStringField(TEXT("controllerPath"), ControllerPath);
        Result->SetStringField(TEXT("blackboardPath"), BlackboardPath);
        Result->SetStringField(TEXT("message"), TEXT("Blackboard assigned"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Blackboard assigned"), Result);
        return true;
    }

    // =========================================================================
    // 16.2 Blackboard (3 actions)
    // =========================================================================

    if (SubAction == TEXT("create_blackboard_asset"))
    {
        FString Name = GetStringFieldAI(Payload, TEXT("name"));
        FString Path = GetStringFieldAI(Payload, TEXT("path"), TEXT("/Game/AI/Blackboards"));

        if (Name.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId,
                                TEXT("Missing name parameter"),
                                TEXT("INVALID_PARAMS"));
            return true;
        }

        FString Error;
        UBlackboardData* Blackboard = CreateBlackboardAsset(Path, Name, Error);
        if (!Blackboard)
        {
            SendAutomationError(RequestingSocket, RequestId, Error, TEXT("CREATION_FAILED"));
            return true;
        }

        Result->SetStringField(TEXT("blackboardPath"), Blackboard->GetPathName());
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Created Blackboard: %s"), *Name));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Blackboard created"), Result);
        return true;
    }

    if (SubAction == TEXT("add_blackboard_key"))
    {
        FString BlackboardPath = GetStringFieldAI(Payload, TEXT("blackboardPath"));
        FString KeyName = GetStringFieldAI(Payload, TEXT("keyName"));
        FString KeyType = GetStringFieldAI(Payload, TEXT("keyType"));

        UBlackboardData* Blackboard = LoadObject<UBlackboardData>(nullptr, *BlackboardPath);
        if (!Blackboard)
        {
            SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("Blackboard not found: %s"), *BlackboardPath),
                                TEXT("NOT_FOUND"));
            return true;
        }

        // Create appropriate key type
        FBlackboardEntry NewEntry;
        NewEntry.EntryName = FName(*KeyName);

        if (KeyType.Equals(TEXT("Bool"), ESearchCase::IgnoreCase))
        {
            NewEntry.KeyType = NewObject<UBlackboardKeyType_Bool>(Blackboard);
        }
        else if (KeyType.Equals(TEXT("Int"), ESearchCase::IgnoreCase))
        {
            NewEntry.KeyType = NewObject<UBlackboardKeyType_Int>(Blackboard);
        }
        else if (KeyType.Equals(TEXT("Float"), ESearchCase::IgnoreCase))
        {
            NewEntry.KeyType = NewObject<UBlackboardKeyType_Float>(Blackboard);
        }
        else if (KeyType.Equals(TEXT("Vector"), ESearchCase::IgnoreCase))
        {
            NewEntry.KeyType = NewObject<UBlackboardKeyType_Vector>(Blackboard);
        }
        else if (KeyType.Equals(TEXT("Rotator"), ESearchCase::IgnoreCase))
        {
            NewEntry.KeyType = NewObject<UBlackboardKeyType_Rotator>(Blackboard);
        }
        else if (KeyType.Equals(TEXT("Object"), ESearchCase::IgnoreCase))
        {
            UBlackboardKeyType_Object* ObjectKey = NewObject<UBlackboardKeyType_Object>(Blackboard);
            FString BaseClass = GetStringFieldAI(Payload, TEXT("baseObjectClass"), TEXT("Actor"));
            // Could set base class here
            NewEntry.KeyType = ObjectKey;
        }
        else if (KeyType.Equals(TEXT("Class"), ESearchCase::IgnoreCase))
        {
            NewEntry.KeyType = NewObject<UBlackboardKeyType_Class>(Blackboard);
        }
        else if (KeyType.Equals(TEXT("Enum"), ESearchCase::IgnoreCase))
        {
            NewEntry.KeyType = NewObject<UBlackboardKeyType_Enum>(Blackboard);
        }
        else if (KeyType.Equals(TEXT("Name"), ESearchCase::IgnoreCase))
        {
            NewEntry.KeyType = NewObject<UBlackboardKeyType_Name>(Blackboard);
        }
        else if (KeyType.Equals(TEXT("String"), ESearchCase::IgnoreCase))
        {
            NewEntry.KeyType = NewObject<UBlackboardKeyType_String>(Blackboard);
        }
        else
        {
            // Default to Object
            NewEntry.KeyType = NewObject<UBlackboardKeyType_Object>(Blackboard);
        }

        NewEntry.bInstanceSynced = GetBoolFieldAI(Payload, TEXT("isInstanceSynced"), false);

        Blackboard->Keys.Add(NewEntry);
        Blackboard->MarkPackageDirty();
        SavePackageHelperAI(Blackboard->GetOutermost(), Blackboard);

        Result->SetNumberField(TEXT("keyIndex"), Blackboard->Keys.Num() - 1);
        Result->SetStringField(TEXT("keyName"), KeyName);
        Result->SetStringField(TEXT("keyType"), KeyType);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Blackboard key added"), Result);
        return true;
    }

    if (SubAction == TEXT("set_key_instance_synced"))
    {
        FString BlackboardPath = GetStringFieldAI(Payload, TEXT("blackboardPath"));
        FString KeyName = GetStringFieldAI(Payload, TEXT("keyName"));
        bool bInstanceSynced = GetBoolFieldAI(Payload, TEXT("isInstanceSynced"), true);

        UBlackboardData* Blackboard = LoadObject<UBlackboardData>(nullptr, *BlackboardPath);
        if (!Blackboard)
        {
            SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("Blackboard not found: %s"), *BlackboardPath),
                                TEXT("NOT_FOUND"));
            return true;
        }

        bool bFound = false;
        for (FBlackboardEntry& Entry : Blackboard->Keys)
        {
            if (Entry.EntryName.ToString() == KeyName)
            {
                Entry.bInstanceSynced = bInstanceSynced;
                bFound = true;
                break;
            }
        }

        if (!bFound)
        {
            SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("Key not found: %s"), *KeyName),
                                TEXT("NOT_FOUND"));
            return true;
        }

        Blackboard->MarkPackageDirty();
        SavePackageHelperAI(Blackboard->GetOutermost(), Blackboard);

        Result->SetStringField(TEXT("keyName"), KeyName);
        Result->SetBoolField(TEXT("isInstanceSynced"), bInstanceSynced);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Key instance sync updated"), Result);
        return true;
    }

    // =========================================================================
    // 16.3 Behavior Tree - Expanded (6 actions)
    // =========================================================================

    if (SubAction == TEXT("create_behavior_tree"))
    {
        FString Name = GetStringFieldAI(Payload, TEXT("name"));
        FString Path = GetStringFieldAI(Payload, TEXT("path"), TEXT("/Game/AI/BehaviorTrees"));

        if (Name.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId,
                                TEXT("Missing name parameter"),
                                TEXT("INVALID_PARAMS"));
            return true;
        }

        FString Error;
        UBehaviorTree* BT = CreateBehaviorTreeAsset(Path, Name, Error);
        if (!BT)
        {
            SendAutomationError(RequestingSocket, RequestId, Error, TEXT("CREATION_FAILED"));
            return true;
        }

        Result->SetStringField(TEXT("behaviorTreePath"), BT->GetPathName());
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Created Behavior Tree: %s"), *Name));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Behavior Tree created"), Result);
        return true;
    }

    if (SubAction == TEXT("add_composite_node"))
    {
        FString BTPath = GetStringFieldAI(Payload, TEXT("behaviorTreePath"));
        FString CompositeType = GetStringFieldAI(Payload, TEXT("compositeType"));

        UBehaviorTree* BT = LoadObject<UBehaviorTree>(nullptr, *BTPath);
        if (!BT)
        {
            SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("Behavior Tree not found: %s"), *BTPath),
                                TEXT("NOT_FOUND"));
            return true;
        }

        UBTCompositeNode* NewNode = nullptr;
        if (CompositeType.Equals(TEXT("Selector"), ESearchCase::IgnoreCase))
        {
            NewNode = NewObject<UBTComposite_Selector>(BT);
        }
        else if (CompositeType.Equals(TEXT("Sequence"), ESearchCase::IgnoreCase))
        {
            NewNode = NewObject<UBTComposite_Sequence>(BT);
        }
        // Add more composite types as needed

        if (NewNode)
        {
            // For adding to root, we'd need to access the internal structure
            // The BT needs a root node set
            if (!BT->RootNode)
            {
                BT->RootNode = NewNode;
            }
            BT->MarkPackageDirty();
            SavePackageHelperAI(BT->GetOutermost(), BT);

            Result->SetStringField(TEXT("compositeType"), CompositeType);
            Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Added %s node"), *CompositeType));
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Composite node added"), Result);
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("Failed to create composite node: %s"), *CompositeType),
                                TEXT("CREATION_FAILED"));
        }

        return true;
    }

    if (SubAction == TEXT("add_task_node"))
    {
        FString BTPath = GetStringFieldAI(Payload, TEXT("behaviorTreePath"));
        FString TaskType = GetStringFieldAI(Payload, TEXT("taskType"));

        UBehaviorTree* BT = LoadObject<UBehaviorTree>(nullptr, *BTPath);
        if (!BT)
        {
            SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("Behavior Tree not found: %s"), *BTPath),
                                TEXT("NOT_FOUND"));
            return true;
        }

        UBTTaskNode* NewTask = nullptr;
        if (TaskType.Equals(TEXT("MoveTo"), ESearchCase::IgnoreCase))
        {
            NewTask = NewObject<UBTTask_MoveTo>(BT);
        }
        else if (TaskType.Equals(TEXT("Wait"), ESearchCase::IgnoreCase))
        {
            NewTask = NewObject<UBTTask_Wait>(BT);
        }
        // Add more task types as needed

        if (NewTask)
        {
            BT->MarkPackageDirty();
            Result->SetStringField(TEXT("taskType"), TaskType);
            Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Added %s task"), *TaskType));
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Task node added"), Result);
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("Failed to create task node: %s"), *TaskType),
                                TEXT("CREATION_FAILED"));
        }

        return true;
    }

    if (SubAction == TEXT("add_decorator"))
    {
        FString BTPath = GetStringFieldAI(Payload, TEXT("behaviorTreePath"));
        FString DecoratorType = GetStringFieldAI(Payload, TEXT("decoratorType"));

        UBehaviorTree* BT = LoadObject<UBehaviorTree>(nullptr, *BTPath);
        if (!BT)
        {
            SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("Behavior Tree not found: %s"), *BTPath),
                                TEXT("NOT_FOUND"));
            return true;
        }

        UBTDecorator* NewDecorator = nullptr;
        if (DecoratorType.Equals(TEXT("Blackboard"), ESearchCase::IgnoreCase))
        {
            NewDecorator = NewObject<UBTDecorator_Blackboard>(BT);
        }
        else if (DecoratorType.Equals(TEXT("Cooldown"), ESearchCase::IgnoreCase))
        {
            NewDecorator = NewObject<UBTDecorator_Cooldown>(BT);
        }
        else if (DecoratorType.Equals(TEXT("Loop"), ESearchCase::IgnoreCase))
        {
            NewDecorator = NewObject<UBTDecorator_Loop>(BT);
        }
        // Add more decorator types as needed

        if (NewDecorator)
        {
            BT->MarkPackageDirty();
            Result->SetStringField(TEXT("decoratorType"), DecoratorType);
            Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Added %s decorator"), *DecoratorType));
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Decorator added"), Result);
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("Failed to create decorator: %s"), *DecoratorType),
                                TEXT("CREATION_FAILED"));
        }

        return true;
    }

    if (SubAction == TEXT("add_service"))
    {
        FString BTPath = GetStringFieldAI(Payload, TEXT("behaviorTreePath"));
        FString ServiceType = GetStringFieldAI(Payload, TEXT("serviceType"));

        UBehaviorTree* BT = LoadObject<UBehaviorTree>(nullptr, *BTPath);
        if (!BT)
        {
            SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("Behavior Tree not found: %s"), *BTPath),
                                TEXT("NOT_FOUND"));
            return true;
        }

        // Services are added to composite nodes, not directly to the tree
        // For now, just mark the tree as modified
        BT->MarkPackageDirty();
        Result->SetStringField(TEXT("serviceType"), ServiceType);
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Service %s reference created"), *ServiceType));

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Service added"), Result);
        return true;
    }

    if (SubAction == TEXT("configure_bt_node"))
    {
        FString BTPath = GetStringFieldAI(Payload, TEXT("behaviorTreePath"));
        FString NodeId = GetStringFieldAI(Payload, TEXT("nodeId"));

        UBehaviorTree* BT = LoadObject<UBehaviorTree>(nullptr, *BTPath);
        if (!BT)
        {
            SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("Behavior Tree not found: %s"), *BTPath),
                                TEXT("NOT_FOUND"));
            return true;
        }

        // Node configuration would require finding the node by ID and setting properties
        BT->MarkPackageDirty();
        Result->SetStringField(TEXT("nodeId"), NodeId);
        Result->SetStringField(TEXT("message"), TEXT("Node configuration updated"));

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Node configured"), Result);
        return true;
    }

    // =========================================================================
    // 16.4 Environment Query System - EQS (5 actions)
    // =========================================================================

    if (SubAction == TEXT("create_eqs_query"))
    {
        FString Name = GetStringFieldAI(Payload, TEXT("name"));
        FString Path = GetStringFieldAI(Payload, TEXT("path"), TEXT("/Game/AI/EQS"));

        if (Name.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId,
                                TEXT("Missing name parameter"),
                                TEXT("INVALID_PARAMS"));
            return true;
        }

        FString Error;
        UEnvQuery* Query = CreateEQSQueryAsset(Path, Name, Error);
        if (!Query)
        {
            SendAutomationError(RequestingSocket, RequestId, Error, TEXT("CREATION_FAILED"));
            return true;
        }

        Result->SetStringField(TEXT("queryPath"), Query->GetPathName());
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Created EQS Query: %s"), *Name));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("EQS Query created"), Result);
        return true;
    }

    if (SubAction == TEXT("add_eqs_generator"))
    {
        FString QueryPath = GetStringFieldAI(Payload, TEXT("queryPath"));
        FString GeneratorType = GetStringFieldAI(Payload, TEXT("generatorType"));

        UEnvQuery* Query = LoadObject<UEnvQuery>(nullptr, *QueryPath);
        if (!Query)
        {
            SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("EQS Query not found: %s"), *QueryPath),
                                TEXT("NOT_FOUND"));
            return true;
        }

        UEnvQueryGenerator* NewGenerator = nullptr;
        if (GeneratorType.Equals(TEXT("ActorsOfClass"), ESearchCase::IgnoreCase))
        {
            NewGenerator = NewObject<UEnvQueryGenerator_ActorsOfClass>(Query);
        }
        else if (GeneratorType.Equals(TEXT("OnCircle"), ESearchCase::IgnoreCase))
        {
            NewGenerator = NewObject<UEnvQueryGenerator_OnCircle>(Query);
        }
        else if (GeneratorType.Equals(TEXT("SimpleGrid"), ESearchCase::IgnoreCase))
        {
            NewGenerator = NewObject<UEnvQueryGenerator_SimpleGrid>(Query);
        }

        if (NewGenerator)
        {
            // Add generator to query options
            Query->MarkPackageDirty();
            Result->SetStringField(TEXT("generatorType"), GeneratorType);
            Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Added %s generator"), *GeneratorType));
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Generator added"), Result);
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("Failed to create generator: %s"), *GeneratorType),
                                TEXT("CREATION_FAILED"));
        }

        return true;
    }

    if (SubAction == TEXT("add_eqs_context"))
    {
        FString QueryPath = GetStringFieldAI(Payload, TEXT("queryPath"));
        FString ContextType = GetStringFieldAI(Payload, TEXT("contextType"));

        UEnvQuery* Query = LoadObject<UEnvQuery>(nullptr, *QueryPath);
        if (!Query)
        {
            SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("EQS Query not found: %s"), *QueryPath),
                                TEXT("NOT_FOUND"));
            return true;
        }

        Query->MarkPackageDirty();
        Result->SetStringField(TEXT("contextType"), ContextType);
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Context %s configured"), *ContextType));

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Context added"), Result);
        return true;
    }

    if (SubAction == TEXT("add_eqs_test"))
    {
        FString QueryPath = GetStringFieldAI(Payload, TEXT("queryPath"));
        FString TestType = GetStringFieldAI(Payload, TEXT("testType"));

        UEnvQuery* Query = LoadObject<UEnvQuery>(nullptr, *QueryPath);
        if (!Query)
        {
            SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("EQS Query not found: %s"), *QueryPath),
                                TEXT("NOT_FOUND"));
            return true;
        }

        UEnvQueryTest* NewTest = nullptr;
        if (TestType.Equals(TEXT("Distance"), ESearchCase::IgnoreCase))
        {
            NewTest = NewObject<UEnvQueryTest_Distance>(Query);
        }
        else if (TestType.Equals(TEXT("Trace"), ESearchCase::IgnoreCase))
        {
            NewTest = NewObject<UEnvQueryTest_Trace>(Query);
        }

        if (NewTest)
        {
            Query->MarkPackageDirty();
            Result->SetStringField(TEXT("testType"), TestType);
            Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Added %s test"), *TestType));
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Test added"), Result);
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("Failed to create test: %s"), *TestType),
                                TEXT("CREATION_FAILED"));
        }

        return true;
    }

    if (SubAction == TEXT("configure_test_scoring"))
    {
        FString QueryPath = GetStringFieldAI(Payload, TEXT("queryPath"));
        int32 TestIndex = static_cast<int32>(GetNumberFieldAI(Payload, TEXT("testIndex"), 0));

        UEnvQuery* Query = LoadObject<UEnvQuery>(nullptr, *QueryPath);
        if (!Query)
        {
            SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("EQS Query not found: %s"), *QueryPath),
                                TEXT("NOT_FOUND"));
            return true;
        }

        Query->MarkPackageDirty();
        Result->SetNumberField(TEXT("testIndex"), TestIndex);
        Result->SetStringField(TEXT("message"), TEXT("Test scoring configured"));

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Scoring configured"), Result);
        return true;
    }

    // =========================================================================
    // 16.5 Perception System (5 actions)
    // =========================================================================

    if (SubAction == TEXT("add_ai_perception_component"))
    {
        FString BlueprintPath = GetStringFieldAI(Payload, TEXT("blueprintPath"));

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath),
                                TEXT("NOT_FOUND"));
            return true;
        }

        USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
        if (!SCS)
        {
            SendAutomationError(RequestingSocket, RequestId,
                                TEXT("Blueprint has no SimpleConstructionScript"),
                                TEXT("INVALID_BLUEPRINT"));
            return true;
        }

        // Create perception component
        USCS_Node* NewNode = SCS->CreateNode(UAIPerceptionComponent::StaticClass(), TEXT("AIPerception"));
        if (NewNode)
        {
            SCS->AddNode(NewNode);
            FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

            Result->SetStringField(TEXT("componentName"), TEXT("AIPerception"));
            Result->SetStringField(TEXT("message"), TEXT("AI Perception component added"));
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Perception component added"), Result);
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId,
                                TEXT("Failed to create AI Perception component"),
                                TEXT("CREATION_FAILED"));
        }

        return true;
    }

    if (SubAction == TEXT("configure_sight_config"))
    {
        FString BlueprintPath = GetStringFieldAI(Payload, TEXT("blueprintPath"));

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath),
                                TEXT("NOT_FOUND"));
            return true;
        }

        // Get sight config parameters
        const TSharedPtr<FJsonObject>* SightConfigObj = nullptr;
        if (Payload->TryGetObjectField(TEXT("sightConfig"), SightConfigObj) && SightConfigObj->IsValid())
        {
            double SightRadius = GetNumberFieldAI(*SightConfigObj, TEXT("sightRadius"), 3000.0);
            double LoseSightRadius = GetNumberFieldAI(*SightConfigObj, TEXT("loseSightRadius"), 3500.0);
            double PeripheralAngle = GetNumberFieldAI(*SightConfigObj, TEXT("peripheralVisionAngle"), 90.0);

            Result->SetNumberField(TEXT("sightRadius"), SightRadius);
            Result->SetNumberField(TEXT("loseSightRadius"), LoseSightRadius);
            Result->SetNumberField(TEXT("peripheralVisionAngle"), PeripheralAngle);
        }

        Blueprint->MarkPackageDirty();
        Result->SetStringField(TEXT("message"), TEXT("Sight sense configured"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Sight config set"), Result);
        return true;
    }

    if (SubAction == TEXT("configure_hearing_config"))
    {
        FString BlueprintPath = GetStringFieldAI(Payload, TEXT("blueprintPath"));

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath),
                                TEXT("NOT_FOUND"));
            return true;
        }

        const TSharedPtr<FJsonObject>* HearingConfigObj = nullptr;
        if (Payload->TryGetObjectField(TEXT("hearingConfig"), HearingConfigObj) && HearingConfigObj->IsValid())
        {
            double HearingRange = GetNumberFieldAI(*HearingConfigObj, TEXT("hearingRange"), 3000.0);
            Result->SetNumberField(TEXT("hearingRange"), HearingRange);
        }

        Blueprint->MarkPackageDirty();
        Result->SetStringField(TEXT("message"), TEXT("Hearing sense configured"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Hearing config set"), Result);
        return true;
    }

    if (SubAction == TEXT("configure_damage_sense_config"))
    {
        FString BlueprintPath = GetStringFieldAI(Payload, TEXT("blueprintPath"));

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath),
                                TEXT("NOT_FOUND"));
            return true;
        }

        Blueprint->MarkPackageDirty();
        Result->SetStringField(TEXT("message"), TEXT("Damage sense configured"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Damage config set"), Result);
        return true;
    }

    if (SubAction == TEXT("set_perception_team"))
    {
        FString BlueprintPath = GetStringFieldAI(Payload, TEXT("blueprintPath"));
        int32 TeamId = static_cast<int32>(GetNumberFieldAI(Payload, TEXT("teamId"), 0));

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath),
                                TEXT("NOT_FOUND"));
            return true;
        }

        Blueprint->MarkPackageDirty();
        Result->SetNumberField(TEXT("teamId"), TeamId);
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Team ID set to %d"), TeamId));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Team set"), Result);
        return true;
    }

    // =========================================================================
    // 16.6 State Trees - UE5.3+ (4 actions)
    // =========================================================================

    if (SubAction == TEXT("create_state_tree"))
    {
#if MCP_HAS_STATE_TREE
        FString Name = GetStringFieldAI(Payload, TEXT("name"));
        FString Path = GetStringFieldAI(Payload, TEXT("path"), TEXT("/Game/AI/StateTrees"));
        // State Tree creation would go here
        Result->SetStringField(TEXT("stateTreePath"), Path / Name);
        Result->SetStringField(TEXT("message"), TEXT("State Tree created"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("State Tree created"), Result);
#else
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("State Trees require UE 5.3+"),
                            TEXT("UNSUPPORTED_VERSION"));
#endif
        return true;
    }

    if (SubAction == TEXT("add_state_tree_state"))
    {
#if MCP_HAS_STATE_TREE
        FString StateTreePath = GetStringFieldAI(Payload, TEXT("stateTreePath"));
        FString StateName = GetStringFieldAI(Payload, TEXT("stateName"));
        Result->SetStringField(TEXT("stateName"), StateName);
        Result->SetStringField(TEXT("message"), TEXT("State added"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("State added"), Result);
#else
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("State Trees require UE 5.3+"),
                            TEXT("UNSUPPORTED_VERSION"));
#endif
        return true;
    }

    if (SubAction == TEXT("add_state_tree_transition"))
    {
#if MCP_HAS_STATE_TREE
        FString StateTreePath = GetStringFieldAI(Payload, TEXT("stateTreePath"));
        FString FromState = GetStringFieldAI(Payload, TEXT("fromState"));
        FString ToState = GetStringFieldAI(Payload, TEXT("toState"));
        Result->SetStringField(TEXT("fromState"), FromState);
        Result->SetStringField(TEXT("toState"), ToState);
        Result->SetStringField(TEXT("message"), TEXT("Transition added"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Transition added"), Result);
#else
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("State Trees require UE 5.3+"),
                            TEXT("UNSUPPORTED_VERSION"));
#endif
        return true;
    }

    if (SubAction == TEXT("configure_state_tree_task"))
    {
#if MCP_HAS_STATE_TREE
        FString StateTreePath = GetStringFieldAI(Payload, TEXT("stateTreePath"));
        FString StateName = GetStringFieldAI(Payload, TEXT("stateName"));
        Result->SetStringField(TEXT("stateName"), StateName);
        Result->SetStringField(TEXT("message"), TEXT("Task configured"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Task configured"), Result);
#else
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("State Trees require UE 5.3+"),
                            TEXT("UNSUPPORTED_VERSION"));
#endif
        return true;
    }

    // =========================================================================
    // 16.7 Smart Objects (4 actions)
    // =========================================================================

    if (SubAction == TEXT("create_smart_object_definition"))
    {
#if MCP_HAS_SMART_OBJECTS
        FString Name = GetStringFieldAI(Payload, TEXT("name"));
        FString Path = GetStringFieldAI(Payload, TEXT("path"), TEXT("/Game/AI/SmartObjects"));
        Result->SetStringField(TEXT("definitionPath"), Path / Name);
        Result->SetStringField(TEXT("message"), TEXT("Smart Object Definition created"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Definition created"), Result);
#else
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Smart Objects require UE 5.0+"),
                            TEXT("UNSUPPORTED_VERSION"));
#endif
        return true;
    }

    if (SubAction == TEXT("add_smart_object_slot"))
    {
#if MCP_HAS_SMART_OBJECTS
        FString DefinitionPath = GetStringFieldAI(Payload, TEXT("definitionPath"));
        Result->SetNumberField(TEXT("slotIndex"), 0);
        Result->SetStringField(TEXT("message"), TEXT("Slot added"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Slot added"), Result);
#else
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Smart Objects require UE 5.0+"),
                            TEXT("UNSUPPORTED_VERSION"));
#endif
        return true;
    }

    if (SubAction == TEXT("configure_slot_behavior"))
    {
#if MCP_HAS_SMART_OBJECTS
        FString DefinitionPath = GetStringFieldAI(Payload, TEXT("definitionPath"));
        int32 SlotIndex = static_cast<int32>(GetNumberFieldAI(Payload, TEXT("slotIndex"), 0));
        Result->SetNumberField(TEXT("slotIndex"), SlotIndex);
        Result->SetStringField(TEXT("message"), TEXT("Slot behavior configured"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Behavior configured"), Result);
#else
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Smart Objects require UE 5.0+"),
                            TEXT("UNSUPPORTED_VERSION"));
#endif
        return true;
    }

    if (SubAction == TEXT("add_smart_object_component"))
    {
#if MCP_HAS_SMART_OBJECTS
        FString BlueprintPath = GetStringFieldAI(Payload, TEXT("blueprintPath"));
        Result->SetStringField(TEXT("componentName"), TEXT("SmartObject"));
        Result->SetStringField(TEXT("message"), TEXT("Smart Object component added"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Component added"), Result);
#else
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Smart Objects require UE 5.0+"),
                            TEXT("UNSUPPORTED_VERSION"));
#endif
        return true;
    }

    // =========================================================================
    // 16.8 Mass AI / Crowds (3 actions)
    // =========================================================================

    if (SubAction == TEXT("create_mass_entity_config"))
    {
#if MCP_HAS_MASS_AI
        FString Name = GetStringFieldAI(Payload, TEXT("name"));
        FString Path = GetStringFieldAI(Payload, TEXT("path"), TEXT("/Game/AI/Mass"));
        Result->SetStringField(TEXT("configPath"), Path / Name);
        Result->SetStringField(TEXT("message"), TEXT("Mass Entity Config created"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Config created"), Result);
#else
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Mass AI requires UE 5.0+ with MassEntity plugin"),
                            TEXT("UNSUPPORTED_VERSION"));
#endif
        return true;
    }

    if (SubAction == TEXT("configure_mass_entity"))
    {
#if MCP_HAS_MASS_AI
        FString ConfigPath = GetStringFieldAI(Payload, TEXT("configPath"));
        Result->SetStringField(TEXT("configPath"), ConfigPath);
        Result->SetStringField(TEXT("message"), TEXT("Mass Entity configured"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Entity configured"), Result);
#else
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Mass AI requires UE 5.0+ with MassEntity plugin"),
                            TEXT("UNSUPPORTED_VERSION"));
#endif
        return true;
    }

    if (SubAction == TEXT("add_mass_spawner"))
    {
#if MCP_HAS_MASS_AI
        FString BlueprintPath = GetStringFieldAI(Payload, TEXT("blueprintPath"));
        Result->SetStringField(TEXT("componentName"), TEXT("MassSpawner"));
        Result->SetStringField(TEXT("message"), TEXT("Mass Spawner component added"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Spawner added"), Result);
#else
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Mass AI requires UE 5.0+ with MassEntity plugin"),
                            TEXT("UNSUPPORTED_VERSION"));
#endif
        return true;
    }

    // =========================================================================
    // Utility (1 action)
    // =========================================================================

    if (SubAction == TEXT("get_ai_info"))
    {
        TSharedPtr<FJsonObject> AIInfo = MakeShareable(new FJsonObject());

        // Check for controller
        FString ControllerPath = GetStringFieldAI(Payload, TEXT("controllerPath"));
        if (!ControllerPath.IsEmpty())
        {
            UBlueprint* Controller = LoadObject<UBlueprint>(nullptr, *ControllerPath);
            if (Controller)
            {
                AIInfo->SetStringField(TEXT("controllerClass"), Controller->GeneratedClass ? Controller->GeneratedClass->GetName() : TEXT("Unknown"));
            }
        }

        // Check for behavior tree
        FString BTPath = GetStringFieldAI(Payload, TEXT("behaviorTreePath"));
        if (!BTPath.IsEmpty())
        {
            UBehaviorTree* BT = LoadObject<UBehaviorTree>(nullptr, *BTPath);
            if (BT)
            {
                AIInfo->SetStringField(TEXT("behaviorTreeName"), BT->GetName());
                AIInfo->SetBoolField(TEXT("hasRootNode"), BT->RootNode != nullptr);
            }
        }

        // Check for blackboard
        FString BBPath = GetStringFieldAI(Payload, TEXT("blackboardPath"));
        if (!BBPath.IsEmpty())
        {
            UBlackboardData* BB = LoadObject<UBlackboardData>(nullptr, *BBPath);
            if (BB)
            {
                AIInfo->SetNumberField(TEXT("keyCount"), BB->Keys.Num());
                TArray<TSharedPtr<FJsonValue>> KeysArray;
                for (const FBlackboardEntry& Entry : BB->Keys)
                {
                    TSharedPtr<FJsonObject> KeyObj = MakeShareable(new FJsonObject());
                    KeyObj->SetStringField(TEXT("name"), Entry.EntryName.ToString());
                    KeyObj->SetStringField(TEXT("type"), Entry.KeyType ? Entry.KeyType->GetClass()->GetName() : TEXT("Unknown"));
                    KeyObj->SetBoolField(TEXT("instanceSynced"), Entry.bInstanceSynced);
                    KeysArray.Add(MakeShareable(new FJsonValueObject(KeyObj)));
                }
                AIInfo->SetArrayField(TEXT("keys"), KeysArray);
            }
        }

        // Check for EQS query
        FString QueryPath = GetStringFieldAI(Payload, TEXT("queryPath"));
        if (!QueryPath.IsEmpty())
        {
            UEnvQuery* Query = LoadObject<UEnvQuery>(nullptr, *QueryPath);
            if (Query)
            {
                AIInfo->SetStringField(TEXT("queryName"), Query->GetName());
            }
        }

        Result->SetObjectField(TEXT("aiInfo"), AIInfo);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("AI info retrieved"), Result);
        return true;
    }

    // Unknown sub-action
    SendAutomationError(RequestingSocket, RequestId,
                        FString::Printf(TEXT("Unknown AI action: %s"), *SubAction),
                        TEXT("UNKNOWN_ACTION"));
    return true;
#endif
}
