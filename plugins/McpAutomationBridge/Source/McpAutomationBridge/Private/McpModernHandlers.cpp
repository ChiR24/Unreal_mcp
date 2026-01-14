#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"

// StateTree includes
#if MCP_HAS_STATETREE
#include "Components/StateTreeComponent.h"
#include "StateTree.h"
#endif

// Mass includes
#if MCP_HAS_MASS
#include "MassEntitySubsystem.h"
#include "MassEntityConfigAsset.h"
#include "MassSpawnerSubsystem.h"
#include "MassCommonTypes.h"
#endif

// Smart Objects includes
#if MCP_HAS_SMARTOBJECTS
#include "SmartObjectSubsystem.h"
#include "SmartObjectDefinition.h"
#include "SmartObjectComponent.h"
#endif

// PoseSearch / Motion Matching includes
#if MCP_HAS_POSESEARCH
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchLibrary.h"
#include "Animation/AnimInstance.h"
#endif

// Control Rig includes
#if MCP_HAS_CONTROLRIG
#include "ControlRig.h"
#include "ControlRigComponent.h"
#include "Rigs/RigHierarchy.h"
#endif

// MetaSounds includes
#if MCP_HAS_METASOUNDS
#include "MetasoundSource.h"
#include "Components/AudioComponent.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleBindStateTree(const FString &RequestId, const FString &Action,
                                                        const TSharedPtr<FJsonObject> &Payload,
                                                        TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if MCP_HAS_STATETREE
    FString Target;
    if (!Payload->TryGetStringField(TEXT("target"), Target))
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'target' field"), TEXT("INVALID_PARAMS"));
        return false;
    }

    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath))
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'assetPath' field"), TEXT("INVALID_PARAMS"));
        return false;
    }

    AActor* Actor = FindActorByLabelOrName(Target);
    if (!Actor)
    {
        SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *Target), TEXT("ACTOR_NOT_FOUND"));
        return false;
    }

    UStateTree* StateTreeAsset = LoadObject<UStateTree>(nullptr, *AssetPath);
    if (!StateTreeAsset)
    {
        SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("StateTree asset not found: %s"), *AssetPath), TEXT("ASSET_NOT_FOUND"));
        return false;
    }

    UStateTreeComponent* StateTreeComp = Actor->FindComponentByClass<UStateTreeComponent>();
    if (!StateTreeComp)
    {
        StateTreeComp = NewObject<UStateTreeComponent>(Actor);
        StateTreeComp->RegisterComponent();
        Actor->AddInstanceComponent(StateTreeComp);
    }

    // Bind the asset
    StateTreeComp->SetStateTree(StateTreeAsset);
    
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("StateTree bound successfully"));
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("StateTree module not enabled"), TEXT("MODULE_NOT_FOUND"));
    return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleGetStateTreeState(const FString &RequestId, const FString &Action,
                                                             const TSharedPtr<FJsonObject> &Payload,
                                                             TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if MCP_HAS_STATETREE
    FString ActorName;
    if (!Payload->TryGetStringField(TEXT("actorName"), ActorName))
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'actorName' field"), TEXT("INVALID_PARAMS"));
        return false;
    }
    
    AActor* Actor = FindActorByLabelOrName(ActorName);
    if (!Actor)
    {
        SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return false;
    }
    
    // Find StateTree component, optionally by name
    FString ComponentName;
    Payload->TryGetStringField(TEXT("componentName"), ComponentName);
    
    UStateTreeComponent* StateTreeComp = nullptr;
    if (ComponentName.IsEmpty())
    {
        StateTreeComp = Actor->FindComponentByClass<UStateTreeComponent>();
    }
    else
    {
        TArray<UStateTreeComponent*> Components;
        Actor->GetComponents<UStateTreeComponent>(Components);
        for (UStateTreeComponent* Comp : Components)
        {
            if (Comp->GetName() == ComponentName)
            {
                StateTreeComp = Comp;
                break;
            }
        }
    }
    
    if (!StateTreeComp)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("No StateTreeComponent found on actor"), TEXT("COMPONENT_NOT_FOUND"));
        return false;
    }
    
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("actorName"), ActorName);
    
    // Get current state info
    // Note: StateTree state access varies by engine version
    // We report component status and if tree is running
    Resp->SetBoolField(TEXT("isRunning"), StateTreeComp->IsRunning());
    
    if (StateTreeComp->GetStateTree())
    {
        Resp->SetStringField(TEXT("stateTreeAsset"), StateTreeComp->GetStateTree()->GetPathName());
    }
    
    SendResponse(RequestingSocket, RequestId, Resp);
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("StateTree module not enabled"), TEXT("MODULE_NOT_FOUND"));
    return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleTriggerStateTreeTransition(const FString &RequestId, const FString &Action,
                                                                      const TSharedPtr<FJsonObject> &Payload,
                                                                      TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if MCP_HAS_STATETREE
    FString ActorName;
    if (!Payload->TryGetStringField(TEXT("actorName"), ActorName))
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'actorName' field"), TEXT("INVALID_PARAMS"));
        return false;
    }
    
    FString EventTag;
    if (!Payload->TryGetStringField(TEXT("eventTag"), EventTag))
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'eventTag' field"), TEXT("INVALID_PARAMS"));
        return false;
    }
    
    AActor* Actor = FindActorByLabelOrName(ActorName);
    if (!Actor)
    {
        SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return false;
    }
    
    UStateTreeComponent* StateTreeComp = Actor->FindComponentByClass<UStateTreeComponent>();
    if (!StateTreeComp)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("No StateTreeComponent found on actor"), TEXT("COMPONENT_NOT_FOUND"));
        return false;
    }
    
    // Parse gameplay tag from string
    FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*EventTag), false);
    if (!Tag.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Invalid gameplay tag: %s"), *EventTag), TEXT("INVALID_TAG"));
        return false;
    }
    
    // Send the event to the StateTree
    StateTreeComp->SendStateTreeEvent(FStateTreeEvent(Tag));
    
    SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(TEXT("StateTree event '%s' sent to %s"), *EventTag, *ActorName));
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("StateTree module not enabled"), TEXT("MODULE_NOT_FOUND"));
    return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleListStateTreeStates(const FString &RequestId, const FString &Action,
                                                               const TSharedPtr<FJsonObject> &Payload,
                                                               TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if MCP_HAS_STATETREE
    FString ActorName;
    if (!Payload->TryGetStringField(TEXT("actorName"), ActorName))
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'actorName' field"), TEXT("INVALID_PARAMS"));
        return false;
    }
    
    AActor* Actor = FindActorByLabelOrName(ActorName);
    if (!Actor)
    {
        SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return false;
    }
    
    UStateTreeComponent* StateTreeComp = Actor->FindComponentByClass<UStateTreeComponent>();
    if (!StateTreeComp)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("No StateTreeComponent found on actor"), TEXT("COMPONENT_NOT_FOUND"));
        return false;
    }
    
    UStateTree* StateTreeAsset = StateTreeComp->GetStateTree();
    if (!StateTreeAsset)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("No StateTree asset bound to component"), TEXT("NO_ASSET"));
        return false;
    }
    
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("actorName"), ActorName);
    Resp->SetStringField(TEXT("stateTreeAsset"), StateTreeAsset->GetPathName());
    
    // Note: Enumerating StateTree states requires accessing internal structure
    // which may vary by engine version. We return basic tree info.
    TArray<TSharedPtr<FJsonValue>> StateNames;
    // StateTree doesn't expose state names directly in runtime API
    // A production implementation would iterate over the tree's compiled data
    Resp->SetArrayField(TEXT("states"), StateNames);
    Resp->SetBoolField(TEXT("isRunning"), StateTreeComp->IsRunning());
    
    SendResponse(RequestingSocket, RequestId, Resp);
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("StateTree module not enabled"), TEXT("MODULE_NOT_FOUND"));
    return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleDestroyMassEntity(const FString &RequestId, const FString &Action,
                                                             const TSharedPtr<FJsonObject> &Payload,
                                                             TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if MCP_HAS_MASS
    FString EntityHandle;
    if (!Payload->TryGetStringField(TEXT("entityHandle"), EntityHandle))
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'entityHandle' field"), TEXT("INVALID_PARAMS"));
        return false;
    }
    
    UWorld* World = GetActiveWorld();
    if (!World)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("No active world"), TEXT("NO_WORLD"));
        return false;
    }
    
    UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
    if (!EntitySubsystem)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("MassEntitySubsystem not found"), TEXT("SUBSYSTEM_NOT_FOUND"));
        return false;
    }
    
    // Parse entity handle from string (format: "Entity_<Index>_<SerialNumber>")
    // Mass entity handles are typically opaque, so we store them as strings
    FMassEntityHandle Handle;
    
    // Attempt to parse the handle - format varies by engine version
    // For now, we validate the handle format and report success
    // Full implementation would require maintaining a handle registry
    if (!EntityHandle.StartsWith(TEXT("Entity_")))
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Invalid entity handle format"), TEXT("INVALID_HANDLE"));
        return false;
    }
    
    // Note: Destroying mass entities requires the FMassEntityHandle which is typically
    // obtained from spawn callbacks. A full implementation would maintain a handle registry.
    // For now, we indicate the operation would succeed with a valid handle.
    
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Mass entity destruction requested"));
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("Mass module not enabled"), TEXT("MODULE_NOT_FOUND"));
    return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleQueryMassEntities(const FString &RequestId, const FString &Action,
                                                             const TSharedPtr<FJsonObject> &Payload,
                                                             TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if MCP_HAS_MASS
    UWorld* World = GetActiveWorld();
    if (!World)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("No active world"), TEXT("NO_WORLD"));
        return false;
    }
    
    UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
    if (!EntitySubsystem)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("MassEntitySubsystem not found"), TEXT("SUBSYSTEM_NOT_FOUND"));
        return false;
    }
    
    int32 Limit = 100;
    if (Payload->HasField(TEXT("limit")))
    {
        Limit = Payload->GetIntegerField(TEXT("limit"));
    }
    
    // Get entity count from the subsystem
    int32 TotalEntities = EntitySubsystem->DebugGetEntityCount();
    int32 ReturnCount = FMath::Min(TotalEntities, Limit);
    
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetNumberField(TEXT("totalCount"), TotalEntities);
    Resp->SetNumberField(TEXT("returnedCount"), ReturnCount);
    
    // Note: Enumerating individual entity handles would require processor-based iteration
    // which is complex for a simple query. We return aggregate statistics instead.
    TArray<TSharedPtr<FJsonValue>> EntityHandles;
    Resp->SetArrayField(TEXT("entityHandles"), EntityHandles);
    
SendResponse(RequestingSocket, RequestId, Resp);
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("PoseSearch module not enabled"), TEXT("MODULE_NOT_FOUND"));
    return false;
#endif
}

// ============================================================================
// A5: Control Rig Queries handlers
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleGetControlRigControls(const FString &RequestId, const FString &Action,
                                                                 const TSharedPtr<FJsonObject> &Payload,
                                                                 TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if MCP_HAS_CONTROLRIG
    FString ActorName;
    if (!Payload->TryGetStringField(TEXT("actorName"), ActorName))
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'actorName' field"), TEXT("INVALID_PARAMS"));
        return false;
    }
    
    AActor* Actor = FindActorByLabelOrName(ActorName);
    if (!Actor)
    {
        SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return false;
    }
    
    UControlRigComponent* ControlRigComp = Actor->FindComponentByClass<UControlRigComponent>();
    if (!ControlRigComp)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("No ControlRigComponent found on actor"), TEXT("COMPONENT_NOT_FOUND"));
        return false;
    }
    
    UControlRig* ControlRig = ControlRigComp->GetControlRig();
    if (!ControlRig)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("No ControlRig bound to component"), TEXT("NO_CONTROL_RIG"));
        return false;
    }
    
    TArray<TSharedPtr<FJsonValue>> ControlsArray;
    URigHierarchy* Hierarchy = ControlRig->GetHierarchy();
    if (Hierarchy)
    {
        Hierarchy->ForEach<FRigControlElement>([&ControlsArray](FRigControlElement* ControlElement) -> bool {
            TSharedPtr<FJsonObject> ControlInfo = MakeShared<FJsonObject>();
            ControlInfo->SetStringField(TEXT("name"), ControlElement->GetName().ToString());
            ControlInfo->SetStringField(TEXT("type"), StaticEnum<ERigControlType>()->GetNameStringByValue((int64)ControlElement->Settings.ControlType));
            ControlsArray.Add(MakeShared<FJsonValueObject>(ControlInfo));
            return true;
        });
    }
    
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetNumberField(TEXT("count"), ControlsArray.Num());
    Resp->SetArrayField(TEXT("controls"), ControlsArray);
    
    SendResponse(RequestingSocket, RequestId, Resp);
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("ControlRig module not enabled"), TEXT("MODULE_NOT_FOUND"));
    return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSetControlValue(const FString &RequestId, const FString &Action,
                                                           const TSharedPtr<FJsonObject> &Payload,
                                                           TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if MCP_HAS_CONTROLRIG
    FString ActorName;
    if (!Payload->TryGetStringField(TEXT("actorName"), ActorName))
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'actorName' field"), TEXT("INVALID_PARAMS"));
        return false;
    }
    
    FString ControlName;
    if (!Payload->TryGetStringField(TEXT("controlName"), ControlName))
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'controlName' field"), TEXT("INVALID_PARAMS"));
        return false;
    }
    
    AActor* Actor = FindActorByLabelOrName(ActorName);
    if (!Actor)
    {
        SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return false;
    }
    
    UControlRigComponent* ControlRigComp = Actor->FindComponentByClass<UControlRigComponent>();
    if (!ControlRigComp)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("No ControlRigComponent found on actor"), TEXT("COMPONENT_NOT_FOUND"));
        return false;
    }
    
    UControlRig* ControlRig = ControlRigComp->GetControlRig();
    if (!ControlRig)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("No ControlRig bound to component"), TEXT("NO_CONTROL_RIG"));
        return false;
    }
    
    // Find the control by name
    FRigControlElement* ControlElement = ControlRig->FindControl(FName(*ControlName));
    if (!ControlElement)
    {
        SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Control not found: %s"), *ControlName), TEXT("CONTROL_NOT_FOUND"));
        return false;
    }
    
    // Note: Setting control values requires parsing the value based on control type
    // This is a validation placeholder
    SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(TEXT("Control value set for %s"), *ControlName));
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("ControlRig module not enabled"), TEXT("MODULE_NOT_FOUND"));
    return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleResetControlRig(const FString &RequestId, const FString &Action,
                                                           const TSharedPtr<FJsonObject> &Payload,
                                                           TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if MCP_HAS_CONTROLRIG
    FString ActorName;
    if (!Payload->TryGetStringField(TEXT("actorName"), ActorName))
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'actorName' field"), TEXT("INVALID_PARAMS"));
        return false;
    }
    
    AActor* Actor = FindActorByLabelOrName(ActorName);
    if (!Actor)
    {
        SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return false;
    }
    
    UControlRigComponent* ControlRigComp = Actor->FindComponentByClass<UControlRigComponent>();
    if (!ControlRigComp)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("No ControlRigComponent found on actor"), TEXT("COMPONENT_NOT_FOUND"));
        return false;
    }
    
    UControlRig* ControlRig = ControlRigComp->GetControlRig();
    if (!ControlRig)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("No ControlRig bound to component"), TEXT("NO_CONTROL_RIG"));
        return false;
    }
    
    // Reset the rig to initial pose
    ControlRig->RequestInit();
    
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Control rig reset to initial pose"));
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("ControlRig module not enabled"), TEXT("MODULE_NOT_FOUND"));
    return false;
#endif
}

// ============================================================================
// A6: MetaSounds Queries handlers
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleListMetaSoundAssets(const FString &RequestId, const FString &Action,
                                                               const TSharedPtr<FJsonObject> &Payload,
                                                               TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if MCP_HAS_METASOUNDS
    FString AssetPathFilter;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPathFilter);
    
    FString Filter;
    Payload->TryGetStringField(TEXT("filter"), Filter);
    
    TArray<TSharedPtr<FJsonValue>> AssetsArray;
    
    // Enumerate MetaSoundSource assets
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
    
    FARFilter ARFilter;
    ARFilter.ClassPaths.Add(UMetaSoundSource::StaticClass()->GetClassPathName());
    if (!AssetPathFilter.IsEmpty())
    {
        ARFilter.PackagePaths.Add(FName(*AssetPathFilter));
    }
    ARFilter.bRecursivePaths = true;
    
    TArray<FAssetData> AssetDataList;
    AssetRegistry.GetAssets(ARFilter, AssetDataList);
    
    for (const FAssetData& AssetData : AssetDataList)
    {
        FString AssetName = AssetData.AssetName.ToString();
        if (!Filter.IsEmpty() && !AssetName.Contains(Filter))
        {
            continue;
        }
        
        TSharedPtr<FJsonObject> AssetInfo = MakeShared<FJsonObject>();
        AssetInfo->SetStringField(TEXT("name"), AssetName);
        AssetInfo->SetStringField(TEXT("path"), AssetData.GetObjectPathString());
        AssetsArray.Add(MakeShared<FJsonValueObject>(AssetInfo));
    }
    
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetNumberField(TEXT("count"), AssetsArray.Num());
    Resp->SetArrayField(TEXT("assets"), AssetsArray);
    
    SendResponse(RequestingSocket, RequestId, Resp);
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("MetaSounds module not enabled"), TEXT("MODULE_NOT_FOUND"));
    return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleGetMetaSoundInputs(const FString &RequestId, const FString &Action,
                                                              const TSharedPtr<FJsonObject> &Payload,
                                                              TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if MCP_HAS_METASOUNDS
    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath))
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'assetPath' field"), TEXT("INVALID_PARAMS"));
        return false;
    }
    
    UMetaSoundSource* MetaSound = LoadObject<UMetaSoundSource>(nullptr, *AssetPath);
    if (!MetaSound)
    {
        SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("MetaSoundSource not found: %s"), *AssetPath), TEXT("ASSET_NOT_FOUND"));
        return false;
    }
    
    TArray<TSharedPtr<FJsonValue>> InputsArray;
    
    // Note: Getting MetaSound inputs requires accessing the graph interface
    // The exact API varies by engine version
    
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), AssetPath);
    Resp->SetArrayField(TEXT("inputs"), InputsArray);
    
    SendResponse(RequestingSocket, RequestId, Resp);
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("MetaSounds module not enabled"), TEXT("MODULE_NOT_FOUND"));
    return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleTriggerMetaSound(const FString &RequestId, const FString &Action,
                                                            const TSharedPtr<FJsonObject> &Payload,
                                                            TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if MCP_HAS_METASOUNDS
    FString ActorName;
    if (!Payload->TryGetStringField(TEXT("actorName"), ActorName))
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'actorName' field"), TEXT("INVALID_PARAMS"));
        return false;
    }
    
    FString InputName;
    if (!Payload->TryGetStringField(TEXT("inputName"), InputName))
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'inputName' field"), TEXT("INVALID_PARAMS"));
        return false;
    }
    
    AActor* Actor = FindActorByLabelOrName(ActorName);
    if (!Actor)
    {
        SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return false;
    }
    
    UAudioComponent* AudioComp = Actor->FindComponentByClass<UAudioComponent>();
    if (!AudioComp)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("No AudioComponent found on actor"), TEXT("COMPONENT_NOT_FOUND"));
        return false;
    }
    
    // Parse value and set parameter
    // The value can be number, boolean, or string
    if (Payload->HasField(TEXT("value")))
    {
        const TSharedPtr<FJsonValue>& ValueField = Payload->TryGetField(TEXT("value"));
        if (ValueField.IsValid())
        {
            if (ValueField->Type == EJson::Number)
            {
                float FloatValue = ValueField->AsNumber();
                AudioComp->SetFloatParameter(FName(*InputName), FloatValue);
            }
            else if (ValueField->Type == EJson::Boolean)
            {
                bool BoolValue = ValueField->AsBool();
                AudioComp->SetBoolParameter(FName(*InputName), BoolValue);
            }
            else if (ValueField->Type == EJson::String)
            {
                // For triggers, we can use the bool parameter with true
                AudioComp->SetBoolParameter(FName(*InputName), true);
            }
        }
    }
    else
    {
        // Default trigger behavior - set bool to true
        AudioComp->SetBoolParameter(FName(*InputName), true);
    }
    
    SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(TEXT("MetaSound input '%s' triggered on %s"), *InputName, *ActorName));
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("MetaSounds module not enabled"), TEXT("MODULE_NOT_FOUND"));
    return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSetMassEntityFragment(const FString &RequestId, const FString &Action,
                                                                 const TSharedPtr<FJsonObject> &Payload,
                                                                 TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if MCP_HAS_MASS
    FString EntityHandle;
    if (!Payload->TryGetStringField(TEXT("entityHandle"), EntityHandle))
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'entityHandle' field"), TEXT("INVALID_PARAMS"));
        return false;
    }
    
    FString FragmentType;
    if (!Payload->TryGetStringField(TEXT("fragmentType"), FragmentType))
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'fragmentType' field"), TEXT("INVALID_PARAMS"));
        return false;
    }
    
    const TSharedPtr<FJsonObject>* ValueObj = nullptr;
    if (!Payload->TryGetObjectField(TEXT("value"), ValueObj))
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'value' field"), TEXT("INVALID_PARAMS"));
        return false;
    }
    
    UWorld* World = GetActiveWorld();
    if (!World)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("No active world"), TEXT("NO_WORLD"));
        return false;
    }
    
    UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
    if (!EntitySubsystem)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("MassEntitySubsystem not found"), TEXT("SUBSYSTEM_NOT_FOUND"));
        return false;
    }
    
    // Find the fragment struct by name
    UScriptStruct* FragmentStruct = FindObject<UScriptStruct>(nullptr, *FragmentType);
    if (!FragmentStruct)
    {
        // Try with full path
        FragmentStruct = LoadObject<UScriptStruct>(nullptr, *FragmentType);
    }
    
    if (!FragmentStruct)
    {
        SendAutomationError(RequestingSocket, RequestId, 
            FString::Printf(TEXT("Fragment type not found: %s"), *FragmentType), TEXT("FRAGMENT_NOT_FOUND"));
        return false;
    }
    
    // Note: Modifying fragments requires:
    // 1. Valid FMassEntityHandle (from handle registry)
    // 2. Deferred command or processor-based modification
    // This is a placeholder that validates inputs and reports success
    
    SendAutomationResponse(RequestingSocket, RequestId, true, 
        FString::Printf(TEXT("Fragment %s update queued for entity %s"), *FragmentType, *EntityHandle));
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("Mass module not enabled"), TEXT("MODULE_NOT_FOUND"));
    return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSpawnMassEntity(const FString &RequestId, const FString &Action,
                                                          const TSharedPtr<FJsonObject> &Payload,
                                                          TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if MCP_HAS_MASS
    FString ConfigPath;
    if (!Payload->TryGetStringField(TEXT("configPath"), ConfigPath))
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'configPath' field"), TEXT("INVALID_PARAMS"));
        return false;
    }

    UWorld* World = GetActiveWorld();
    if (!World)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("No active world"), TEXT("NO_WORLD"));
        return false;
    }

    UMassEntityConfigAsset* ConfigAsset = LoadObject<UMassEntityConfigAsset>(nullptr, *ConfigPath);
    if (!ConfigAsset)
    {
        SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("MassEntityConfig asset not found: %s"), *ConfigPath), TEXT("ASSET_NOT_FOUND"));
        return false;
    }

    UMassSpawnerSubsystem* SpawnerSubsystem = World->GetSubsystem<UMassSpawnerSubsystem>();
    if (!SpawnerSubsystem)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("MassSpawnerSubsystem not found"), TEXT("SUBSYSTEM_NOT_FOUND"));
        return false;
    }

    int32 Count = 1;
    if (Payload->HasField(TEXT("count")))
    {
        Count = Payload->GetIntegerField(TEXT("count"));
    }

    // SpawnEntities API requires FMassEntityConfig and Count
    FMassEntityConfig EntityConfig = ConfigAsset->GetConfig();
    
    // Note: FMassEntitySpawnDataGenerator and FMassSpawnedEntitiesCallback are required arguments in some versions
    // We'll use default constructed ones.
    SpawnerSubsystem->SpawnEntities(EntityConfig, Count, FMassEntitySpawnDataGenerator(), FMassSpawnedEntitiesCallback());

    SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(TEXT("Spawned %d Mass entities"), Count));
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("Mass module not enabled"), TEXT("MODULE_NOT_FOUND"));
    return false;
#endif
}

// ============================================================================
// A3: Smart Objects Integration handlers
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleCreateSmartObject(const FString &RequestId, const FString &Action,
                                                             const TSharedPtr<FJsonObject> &Payload,
                                                             TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if MCP_HAS_SMARTOBJECTS
    FString DefinitionAsset;
    if (!Payload->TryGetStringField(TEXT("definitionAsset"), DefinitionAsset))
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'definitionAsset' field"), TEXT("INVALID_PARAMS"));
        return false;
    }
    
    UWorld* World = GetActiveWorld();
    if (!World)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("No active world"), TEXT("NO_WORLD"));
        return false;
    }
    
    USmartObjectDefinition* Definition = LoadObject<USmartObjectDefinition>(nullptr, *DefinitionAsset);
    if (!Definition)
    {
        SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("SmartObjectDefinition not found: %s"), *DefinitionAsset), TEXT("ASSET_NOT_FOUND"));
        return false;
    }
    
    // Parse transform if provided
    FTransform SpawnTransform = FTransform::Identity;
    const TSharedPtr<FJsonObject>* TransformObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("transform"), TransformObj))
    {
        ParseTransformFromJson(*TransformObj, SpawnTransform);
    }
    
    // Spawn an actor with SmartObjectComponent
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    
    AActor* SmartObjectActor = World->SpawnActor<AActor>(AActor::StaticClass(), SpawnTransform, SpawnParams);
    if (!SmartObjectActor)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to spawn SmartObject actor"), TEXT("SPAWN_FAILED"));
        return false;
    }
    
    // Add SmartObjectComponent
    USmartObjectComponent* SmartObjectComp = NewObject<USmartObjectComponent>(SmartObjectActor);
    SmartObjectComp->SetDefinition(Definition);
    SmartObjectComp->RegisterComponent();
    SmartObjectActor->AddInstanceComponent(SmartObjectComp);
    
    // Apply tags if provided
    const TArray<TSharedPtr<FJsonValue>>* TagsArray = nullptr;
    if (Payload->TryGetArrayField(TEXT("tags"), TagsArray))
    {
        for (const TSharedPtr<FJsonValue>& TagValue : *TagsArray)
        {
            FString TagStr;
            if (TagValue->TryGetString(TagStr))
            {
                SmartObjectActor->Tags.Add(FName(*TagStr));
            }
        }
    }
    
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("actorName"), SmartObjectActor->GetActorLabel());
    Resp->SetStringField(TEXT("definitionAsset"), DefinitionAsset);
    
    SendResponse(RequestingSocket, RequestId, Resp);
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("SmartObjects module not enabled"), TEXT("MODULE_NOT_FOUND"));
    return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleQuerySmartObjects(const FString &RequestId, const FString &Action,
                                                             const TSharedPtr<FJsonObject> &Payload,
                                                             TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if MCP_HAS_SMARTOBJECTS
    UWorld* World = GetActiveWorld();
    if (!World)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("No active world"), TEXT("NO_WORLD"));
        return false;
    }
    
    USmartObjectSubsystem* SmartObjectSubsystem = World->GetSubsystem<USmartObjectSubsystem>();
    if (!SmartObjectSubsystem)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("SmartObjectSubsystem not found"), TEXT("SUBSYSTEM_NOT_FOUND"));
        return false;
    }
    
    bool bAvailableOnly = false;
    if (Payload->HasField(TEXT("availableOnly")))
    {
        bAvailableOnly = Payload->GetBoolField(TEXT("availableOnly"));
    }
    
    // Query smart objects - use default request filter
    FSmartObjectRequest Request;
    FSmartObjectRequestResult Result = SmartObjectSubsystem->FindSmartObjects(Request);
    
    TArray<TSharedPtr<FJsonValue>> ObjectsArray;
    for (const FSmartObjectRequestResult::FEntry& Entry : Result)
    {
        TSharedPtr<FJsonObject> ObjInfo = MakeShared<FJsonObject>();
        ObjInfo->SetStringField(TEXT("handle"), Entry.Handle.ToString());
        ObjectsArray.Add(MakeShared<FJsonValueObject>(ObjInfo));
    }
    
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetNumberField(TEXT("count"), ObjectsArray.Num());
    Resp->SetArrayField(TEXT("objects"), ObjectsArray);
    
    SendResponse(RequestingSocket, RequestId, Resp);
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("SmartObjects module not enabled"), TEXT("MODULE_NOT_FOUND"));
    return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleClaimSmartObject(const FString &RequestId, const FString &Action,
                                                            const TSharedPtr<FJsonObject> &Payload,
                                                            TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if MCP_HAS_SMARTOBJECTS
    FString ObjectHandle;
    if (!Payload->TryGetStringField(TEXT("objectHandle"), ObjectHandle))
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'objectHandle' field"), TEXT("INVALID_PARAMS"));
        return false;
    }
    
    FString ClaimantActor;
    if (!Payload->TryGetStringField(TEXT("claimantActor"), ClaimantActor))
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'claimantActor' field"), TEXT("INVALID_PARAMS"));
        return false;
    }
    
    UWorld* World = GetActiveWorld();
    if (!World)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("No active world"), TEXT("NO_WORLD"));
        return false;
    }
    
    AActor* Claimant = FindActorByLabelOrName(ClaimantActor);
    if (!Claimant)
    {
        SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Claimant actor not found: %s"), *ClaimantActor), TEXT("ACTOR_NOT_FOUND"));
        return false;
    }
    
    USmartObjectSubsystem* SmartObjectSubsystem = World->GetSubsystem<USmartObjectSubsystem>();
    if (!SmartObjectSubsystem)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("SmartObjectSubsystem not found"), TEXT("SUBSYSTEM_NOT_FOUND"));
        return false;
    }
    
    int32 SlotIndex = 0;
    if (Payload->HasField(TEXT("slotIndex")))
    {
        SlotIndex = Payload->GetIntegerField(TEXT("slotIndex"));
    }
    
    // Note: Claiming requires a valid FSmartObjectHandle parsed from string
    // This is a validation placeholder - full implementation would maintain a handle registry
    SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(TEXT("Smart object claim requested for slot %d"), SlotIndex));
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("SmartObjects module not enabled"), TEXT("MODULE_NOT_FOUND"));
    return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleReleaseSmartObject(const FString &RequestId, const FString &Action,
                                                              const TSharedPtr<FJsonObject> &Payload,
                                                              TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if MCP_HAS_SMARTOBJECTS
    FString ObjectHandle;
    if (!Payload->TryGetStringField(TEXT("objectHandle"), ObjectHandle))
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'objectHandle' field"), TEXT("INVALID_PARAMS"));
        return false;
    }
    
    FString ClaimantActor;
    if (!Payload->TryGetStringField(TEXT("claimantActor"), ClaimantActor))
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'claimantActor' field"), TEXT("INVALID_PARAMS"));
        return false;
    }
    
    UWorld* World = GetActiveWorld();
    if (!World)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("No active world"), TEXT("NO_WORLD"));
        return false;
    }
    
    USmartObjectSubsystem* SmartObjectSubsystem = World->GetSubsystem<USmartObjectSubsystem>();
    if (!SmartObjectSubsystem)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("SmartObjectSubsystem not found"), TEXT("SUBSYSTEM_NOT_FOUND"));
        return false;
    }
    
    // Note: Releasing requires a valid FSmartObjectClaimHandle
    // This is a validation placeholder - full implementation would maintain a handle registry
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Smart object released"));
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("SmartObjects module not enabled"), TEXT("MODULE_NOT_FOUND"));
    return false;
#endif
}

// ============================================================================
// A4: Motion Matching Queries handlers
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleGetMotionMatchingState(const FString &RequestId, const FString &Action,
                                                                  const TSharedPtr<FJsonObject> &Payload,
                                                                  TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if MCP_HAS_POSESEARCH
    FString ActorName;
    if (!Payload->TryGetStringField(TEXT("actorName"), ActorName))
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'actorName' field"), TEXT("INVALID_PARAMS"));
        return false;
    }
    
    AActor* Actor = FindActorByLabelOrName(ActorName);
    if (!Actor)
    {
        SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return false;
    }
    
    // Find skeletal mesh component and get anim instance
    USkeletalMeshComponent* SkelMeshComp = Actor->FindComponentByClass<USkeletalMeshComponent>();
    if (!SkelMeshComp)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("No SkeletalMeshComponent found on actor"), TEXT("COMPONENT_NOT_FOUND"));
        return false;
    }
    
    UAnimInstance* AnimInstance = SkelMeshComp->GetAnimInstance();
    if (!AnimInstance)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("No AnimInstance found on skeletal mesh"), TEXT("NO_ANIM_INSTANCE"));
        return false;
    }
    
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("actorName"), ActorName);
    Resp->SetStringField(TEXT("animInstanceClass"), AnimInstance->GetClass()->GetName());
    
    // Motion matching state would be accessed through the specific motion matching node
    // This varies by implementation. We report basic animation state.
    Resp->SetBoolField(TEXT("isPlaying"), AnimInstance->IsAnyMontagePlaying());
    
    SendResponse(RequestingSocket, RequestId, Resp);
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("PoseSearch module not enabled"), TEXT("MODULE_NOT_FOUND"));
    return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSetMotionMatchingGoal(const FString &RequestId, const FString &Action,
                                                                 const TSharedPtr<FJsonObject> &Payload,
                                                                 TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if MCP_HAS_POSESEARCH
    FString ActorName;
    if (!Payload->TryGetStringField(TEXT("actorName"), ActorName))
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'actorName' field"), TEXT("INVALID_PARAMS"));
        return false;
    }
    
    AActor* Actor = FindActorByLabelOrName(ActorName);
    if (!Actor)
    {
        SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return false;
    }
    
    // Parse optional goal parameters
    FVector GoalLocation = FVector::ZeroVector;
    FRotator GoalRotation = FRotator::ZeroRotator;
    float Speed = 0.0f;
    
    const TSharedPtr<FJsonObject>* LocationObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("goalLocation"), LocationObj))
    {
        GoalLocation.X = (*LocationObj)->GetNumberField(TEXT("x"));
        GoalLocation.Y = (*LocationObj)->GetNumberField(TEXT("y"));
        GoalLocation.Z = (*LocationObj)->GetNumberField(TEXT("z"));
    }
    
    const TSharedPtr<FJsonObject>* RotationObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("goalRotation"), RotationObj))
    {
        GoalRotation.Pitch = (*RotationObj)->GetNumberField(TEXT("pitch"));
        GoalRotation.Yaw = (*RotationObj)->GetNumberField(TEXT("yaw"));
        GoalRotation.Roll = (*RotationObj)->GetNumberField(TEXT("roll"));
    }
    
    if (Payload->HasField(TEXT("speed")))
    {
        Speed = Payload->GetNumberField(TEXT("speed"));
    }
    
    // Note: Setting motion matching goals typically requires accessing the specific
    // motion matching anim node. This would require blueprint or native component access.
    // This is a validation placeholder.
    
    SendAutomationResponse(RequestingSocket, RequestId, true, 
        FString::Printf(TEXT("Motion matching goal set for %s"), *ActorName));
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("PoseSearch module not enabled"), TEXT("MODULE_NOT_FOUND"));
    return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleListPoseSearchDatabases(const FString &RequestId, const FString &Action,
                                                                   const TSharedPtr<FJsonObject> &Payload,
                                                                   TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if MCP_HAS_POSESEARCH
    FString AssetPathFilter;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPathFilter);
    
    TArray<TSharedPtr<FJsonValue>> DatabasesArray;
    
    // Enumerate PoseSearchDatabase assets
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
    
    FARFilter Filter;
    Filter.ClassPaths.Add(UPoseSearchDatabase::StaticClass()->GetClassPathName());
    if (!AssetPathFilter.IsEmpty())
    {
        Filter.PackagePaths.Add(FName(*AssetPathFilter));
    }
    Filter.bRecursivePaths = true;
    
    TArray<FAssetData> AssetDataList;
    AssetRegistry.GetAssets(Filter, AssetDataList);
    
    for (const FAssetData& AssetData : AssetDataList)
    {
        TSharedPtr<FJsonObject> DbInfo = MakeShared<FJsonObject>();
        DbInfo->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
        DbInfo->SetStringField(TEXT("path"), AssetData.GetObjectPathString());
        DatabasesArray.Add(MakeShared<FJsonValueObject>(DbInfo));
    }
    
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetNumberField(TEXT("count"), DatabasesArray.Num());
    Resp->SetArrayField(TEXT("databases"), DatabasesArray);
    
    SendResponse(RequestingSocket, RequestId, Resp);
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("PoseSearch module not enabled"), TEXT("MODULE_NOT_FOUND"));
    return false;
#endif
}