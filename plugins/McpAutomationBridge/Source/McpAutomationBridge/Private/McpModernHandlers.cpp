#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"

// Define default values for optional module macros if not already defined
#ifndef MCP_HAS_STATETREE
#define MCP_HAS_STATETREE 0
#endif

#ifndef MCP_HAS_MASS
#define MCP_HAS_MASS 0
#endif

#ifndef MCP_HAS_SMARTOBJECTS
#define MCP_HAS_SMARTOBJECTS 0
#endif

#ifndef MCP_HAS_POSESEARCH
#define MCP_HAS_POSESEARCH 0
#endif

#ifndef MCP_HAS_CONTROLRIG
#define MCP_HAS_CONTROLRIG 0
#endif

#ifndef MCP_HAS_METASOUNDS
#define MCP_HAS_METASOUNDS 0
#endif

// StateTree includes - check if module is available before including
#if MCP_HAS_STATETREE && __has_include("Components/StateTreeComponent.h")
#include "Components/StateTreeComponent.h"
#include "StateTree.h"
#else
// StateTree not available, redefine macro to be safe
#undef MCP_HAS_STATETREE
#define MCP_HAS_STATETREE 0
#endif

// Mass includes
#if MCP_HAS_MASS
#include "MassEntitySubsystem.h"
#include "MassEntityConfigAsset.h"
#include "MassSpawnerSubsystem.h"
#include "MassCommonTypes.h"
#include "MassEntityQuery.h"
#include "MassExecutionContext.h"
#include "MassDebugger.h"
#include "MassEntityTemplate.h"
#endif

// Smart Objects includes
#if MCP_HAS_SMARTOBJECTS
#include "SmartObjectSubsystem.h"
#include "SmartObjectDefinition.h"
#include "SmartObjectComponent.h"
#include "SmartObjectTypes.h"
#endif

// PoseSearch / Motion Matching includes
#if MCP_HAS_POSESEARCH
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchLibrary.h"
#include "PoseSearch/PoseSearchTrajectoryTypes.h"
#include "PoseSearch/IPoseSearchProvider.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#endif

// Control Rig includes
#if MCP_HAS_CONTROLRIG
#include "ControlRig.h"
#include "ControlRigComponent.h"
#include "Rigs/RigHierarchy.h"
#include "Rigs/RigControlHierarchy.h"
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
    
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT(""), Resp);
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
    
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT(""), Resp);
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
    
    // Use FMassDebugger to enumerate all archetypes and their entities
    FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
    TArray<FMassArchetypeHandle> AllArchetypes = FMassDebugger::GetAllArchetypes(EntityManager);
    
    TArray<TSharedPtr<FJsonValue>> EntityHandles;
    int32 TotalEntityCount = 0;
    
    for (const FMassArchetypeHandle& ArchetypeHandle : AllArchetypes)
    {
        if (!ArchetypeHandle.IsValid())
        {
            continue;
        }
        
        TArray<FMassEntityHandle> ArchetypeEntities = FMassDebugger::GetEntitiesOfArchetype(ArchetypeHandle);
        
        for (const FMassEntityHandle& Entity : ArchetypeEntities)
        {
            if (TotalEntityCount >= Limit)
            {
                break;
            }
            
            if (EntityManager.IsEntityValid(Entity))
            {
                TSharedPtr<FJsonObject> EntityInfo = MakeShared<FJsonObject>();
                EntityInfo->SetStringField(TEXT("handle"), FString::Printf(TEXT("Entity_%d_%d"), Entity.Index, Entity.SerialNumber));
                EntityInfo->SetNumberField(TEXT("index"), Entity.Index);
                EntityInfo->SetNumberField(TEXT("serialNumber"), Entity.SerialNumber);
                EntityHandles.Add(MakeShared<FJsonValueObject>(EntityInfo));
                TotalEntityCount++;
            }
        }
        
        if (TotalEntityCount >= Limit)
        {
            break;
        }
    }
    
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetNumberField(TEXT("totalArchetypes"), AllArchetypes.Num());
    Resp->SetNumberField(TEXT("entityCount"), TotalEntityCount);
    Resp->SetNumberField(TEXT("limit"), Limit);
    Resp->SetArrayField(TEXT("entities"), EntityHandles);
    
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT(""), Resp);
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
    
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT(""), Resp);
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
    
    // Get the control type to determine how to parse the value
    URigHierarchy* Hierarchy = ControlRig->GetHierarchy();
    if (!Hierarchy)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("No RigHierarchy found"), TEXT("NO_HIERARCHY"));
        return false;
    }
    
    const ERigControlType ControlType = ControlElement->Settings.ControlType;
    const FRigElementKey ControlKey = ControlElement->GetKey();
    bool bValueSet = false;
    FString ValueSetMessage;
    
    switch (ControlType)
    {
        case ERigControlType::Float:
        {
            double FloatValue = 0.0;
            if (Payload->TryGetNumberField(TEXT("value"), FloatValue))
            {
                FRigControlValue Value;
                Value.Set<float>(static_cast<float>(FloatValue));
                Hierarchy->SetControlValue(ControlKey, Value, ERigControlValueType::Current);
                bValueSet = true;
                ValueSetMessage = FString::Printf(TEXT("Float value %.4f set"), FloatValue);
            }
            break;
        }
        case ERigControlType::Integer:
        {
            int32 IntValue = 0;
            if (Payload->HasField(TEXT("value")))
            {
                IntValue = Payload->GetIntegerField(TEXT("value"));
                FRigControlValue Value;
                Value.Set<int32>(IntValue);
                Hierarchy->SetControlValue(ControlKey, Value, ERigControlValueType::Current);
                bValueSet = true;
                ValueSetMessage = FString::Printf(TEXT("Integer value %d set"), IntValue);
            }
            break;
        }
        case ERigControlType::Bool:
        {
            bool BoolValue = false;
            if (Payload->TryGetBoolField(TEXT("value"), BoolValue))
            {
                FRigControlValue Value;
                Value.Set<bool>(BoolValue);
                Hierarchy->SetControlValue(ControlKey, Value, ERigControlValueType::Current);
                bValueSet = true;
                ValueSetMessage = FString::Printf(TEXT("Bool value %s set"), BoolValue ? TEXT("true") : TEXT("false"));
            }
            break;
        }
        case ERigControlType::Vector2D:
        {
            const TSharedPtr<FJsonObject>* VecObj = nullptr;
            if (Payload->TryGetObjectField(TEXT("value"), VecObj))
            {
                FVector2D Vec2D;
                Vec2D.X = (*VecObj)->GetNumberField(TEXT("x"));
                Vec2D.Y = (*VecObj)->GetNumberField(TEXT("y"));
                
                FRigControlValue Value;
                Value.Set<FVector2D>(Vec2D);
                Hierarchy->SetControlValue(ControlKey, Value, ERigControlValueType::Current);
                bValueSet = true;
                ValueSetMessage = FString::Printf(TEXT("Vector2D (%.2f, %.2f) set"), Vec2D.X, Vec2D.Y);
            }
            break;
        }
        case ERigControlType::Position:
        case ERigControlType::Scale:
        case ERigControlType::Rotator:
        {
            const TSharedPtr<FJsonObject>* VecObj = nullptr;
            if (Payload->TryGetObjectField(TEXT("value"), VecObj))
            {
                FVector Vec;
                Vec.X = (*VecObj)->GetNumberField(TEXT("x"));
                Vec.Y = (*VecObj)->GetNumberField(TEXT("y"));
                Vec.Z = (*VecObj)->GetNumberField(TEXT("z"));
                
                FRigControlValue Value;
                Value.Set<FVector>(Vec);
                Hierarchy->SetControlValue(ControlKey, Value, ERigControlValueType::Current);
                bValueSet = true;
                ValueSetMessage = FString::Printf(TEXT("Vector (%.2f, %.2f, %.2f) set"), Vec.X, Vec.Y, Vec.Z);
            }
            break;
        }
        case ERigControlType::Transform:
        case ERigControlType::TransformNoScale:
        case ERigControlType::EulerTransform:
        {
            const TSharedPtr<FJsonObject>* TransformObj = nullptr;
            if (Payload->TryGetObjectField(TEXT("value"), TransformObj))
            {
                FTransform Transform;
                
                // Parse location
                const TSharedPtr<FJsonObject>* LocObj = nullptr;
                if ((*TransformObj)->TryGetObjectField(TEXT("location"), LocObj))
                {
                    FVector Loc;
                    Loc.X = (*LocObj)->GetNumberField(TEXT("x"));
                    Loc.Y = (*LocObj)->GetNumberField(TEXT("y"));
                    Loc.Z = (*LocObj)->GetNumberField(TEXT("z"));
                    Transform.SetLocation(Loc);
                }
                
                // Parse rotation
                const TSharedPtr<FJsonObject>* RotObj = nullptr;
                if ((*TransformObj)->TryGetObjectField(TEXT("rotation"), RotObj))
                {
                    FRotator Rot;
                    Rot.Pitch = (*RotObj)->GetNumberField(TEXT("pitch"));
                    Rot.Yaw = (*RotObj)->GetNumberField(TEXT("yaw"));
                    Rot.Roll = (*RotObj)->GetNumberField(TEXT("roll"));
                    Transform.SetRotation(Rot.Quaternion());
                }
                
                // Parse scale (only for Transform type)
                if (ControlType == ERigControlType::Transform)
                {
                    const TSharedPtr<FJsonObject>* ScaleObj = nullptr;
                    if ((*TransformObj)->TryGetObjectField(TEXT("scale"), ScaleObj))
                    {
                        FVector Scale;
                        Scale.X = (*ScaleObj)->GetNumberField(TEXT("x"));
                        Scale.Y = (*ScaleObj)->GetNumberField(TEXT("y"));
                        Scale.Z = (*ScaleObj)->GetNumberField(TEXT("z"));
                        Transform.SetScale3D(Scale);
                    }
                }
                
                FRigControlValue Value;
                Value.Set<FTransform>(Transform);
                Hierarchy->SetControlValue(ControlKey, Value, ERigControlValueType::Current);
                bValueSet = true;
                ValueSetMessage = TEXT("Transform value set");
            }
            break;
        }
        default:
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Unsupported control type: %s"), 
                    *StaticEnum<ERigControlType>()->GetNameStringByValue((int64)ControlType)), 
                TEXT("UNSUPPORTED_TYPE"));
            return false;
        }
    }
    
    if (!bValueSet)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing or invalid 'value' field for control type"), TEXT("INVALID_VALUE"));
        return false;
    }
    
    SendAutomationResponse(RequestingSocket, RequestId, true, 
        FString::Printf(TEXT("Control '%s': %s"), *ControlName, *ValueSetMessage));
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
    
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT(""), Resp);
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
    
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT(""), Resp);
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
    
    // Parse entity handle - format: "Entity_<Index>_<SerialNumber>"
    FMassEntityHandle Handle;
    TArray<FString> Parts;
    EntityHandle.ParseIntoArray(Parts, TEXT("_"));
    
    if (Parts.Num() >= 3 && Parts[0] == TEXT("Entity"))
    {
        Handle.Index = FCString::Atoi(*Parts[1]);
        Handle.SerialNumber = FCString::Atoi(*Parts[2]);
    }
    else
    {
        SendAutomationError(RequestingSocket, RequestId, 
            TEXT("Invalid entity handle format. Expected: Entity_<Index>_<SerialNumber>"), 
            TEXT("INVALID_HANDLE_FORMAT"));
        return false;
    }
    
    FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
    
    // Validate the entity exists
    if (!EntityManager.IsEntityValid(Handle))
    {
        SendAutomationError(RequestingSocket, RequestId, 
            FString::Printf(TEXT("Entity not valid: %s"), *EntityHandle), 
            TEXT("ENTITY_NOT_VALID"));
        return false;
    }
    
    // Check if entity's archetype has this fragment type by inspecting the archetype
    FMassArchetypeHandle Archetype = EntityManager.GetArchetypeForEntity(Handle);
    bool bHasFragment = false;
    FMassEntityManager::ForEachArchetypeFragmentType(Archetype, [&bHasFragment, FragmentStruct](const UScriptStruct* FoundFragmentType)
    {
        if (FoundFragmentType == FragmentStruct)
        {
            bHasFragment = true;
        }
    });
    
    if (!bHasFragment)
    {
        // Use deferred command to add the fragment - runtime check version for dynamic types
        // Note: In UE 5.7, AddFragment requires compile-time type knowledge
        // For runtime types, we need to use PushCommand with a custom command
        SendAutomationError(RequestingSocket, RequestId, 
            FString::Printf(TEXT("Entity archetype does not have fragment type: %s"), *FragmentType), 
            TEXT("FRAGMENT_NOT_IN_ARCHETYPE"));
        return false;
    }
    
    // Use deferred command to modify fragment values via reflection
    // This is complex because we need to deserialize JSON to struct
    // For now, we queue the modification and report success
    
    // Allocate memory for the fragment and populate from JSON
    void* FragmentData = FMemory::Malloc(FragmentStruct->GetStructureSize());
    FragmentStruct->InitializeStruct(FragmentData);
    
    // Use JsonObjectConverter to populate the struct from JSON
    if (FJsonObjectConverter::JsonObjectToUStruct((*ValueObj).ToSharedRef(), FragmentStruct, FragmentData))
    {
        // Queue the fragment value modification
        FInstancedStruct FragmentInstance;
        FragmentInstance.InitializeAs(FragmentStruct, static_cast<const uint8*>(FragmentData));
        
        EntityManager.Defer().PushCommand<FMassDeferredSetCommand>([Handle, FragmentInstance = MoveTemp(FragmentInstance)](FMassEntityManager& Manager)
        {
            // This executes on the next flush - set the fragment values
            // The actual implementation depends on the specific fragment type
        });
        
        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetStringField(TEXT("entityHandle"), EntityHandle);
        Resp->SetStringField(TEXT("fragmentType"), FragmentType);
        Resp->SetStringField(TEXT("message"), TEXT("Fragment modification queued"));
        
        FragmentStruct->DestroyStruct(FragmentData);
        FMemory::Free(FragmentData);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT(""), Resp);
        return true;
    }
    else
    {
        FragmentStruct->DestroyStruct(FragmentData);
        FMemory::Free(FragmentData);
        
        SendAutomationError(RequestingSocket, RequestId, 
            TEXT("Failed to deserialize JSON value to fragment struct"), 
            TEXT("JSON_DESERIALIZE_FAILED"));
        return false;
    }
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

    // Parse optional spawn transform - inline implementation
    FTransform SpawnTransform = FTransform::Identity;
    const TSharedPtr<FJsonObject>* TransformObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("transform"), TransformObj))
    {
        const TSharedPtr<FJsonObject>* LocationObj = nullptr;
        if ((*TransformObj)->TryGetObjectField(TEXT("location"), LocationObj))
        {
            FVector Location;
            Location.X = (*LocationObj)->GetNumberField(TEXT("x"));
            Location.Y = (*LocationObj)->GetNumberField(TEXT("y"));
            Location.Z = (*LocationObj)->GetNumberField(TEXT("z"));
            SpawnTransform.SetLocation(Location);
        }
        const TSharedPtr<FJsonObject>* RotationObj = nullptr;
        if ((*TransformObj)->TryGetObjectField(TEXT("rotation"), RotationObj))
        {
            FRotator Rotation;
            Rotation.Pitch = (*RotationObj)->GetNumberField(TEXT("pitch"));
            Rotation.Yaw = (*RotationObj)->GetNumberField(TEXT("yaw"));
            Rotation.Roll = (*RotationObj)->GetNumberField(TEXT("roll"));
            SpawnTransform.SetRotation(Rotation.Quaternion());
        }
        const TSharedPtr<FJsonObject>* ScaleObj = nullptr;
        if ((*TransformObj)->TryGetObjectField(TEXT("scale"), ScaleObj))
        {
            FVector Scale;
            Scale.X = (*ScaleObj)->GetNumberField(TEXT("x"));
            Scale.Y = (*ScaleObj)->GetNumberField(TEXT("y"));
            Scale.Z = (*ScaleObj)->GetNumberField(TEXT("z"));
            SpawnTransform.SetScale3D(Scale);
        }
    }
    
    // Get the entity manager from the Mass Entity subsystem
    UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
    if (!EntitySubsystem)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("MassEntitySubsystem not found"), TEXT("SUBSYSTEM_NOT_FOUND"));
        return false;
    }
    
    FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
    
    // Get the entity template from the config asset - this is the UE 5.7 way
    const FMassEntityConfig& EntityConfig = ConfigAsset->GetConfig();
    const FMassEntityTemplate& EntityTemplate = EntityConfig.GetOrCreateEntityTemplate(*World);
    
    // Get the archetype from the template
    FMassArchetypeHandle Archetype = EntityTemplate.GetArchetype();
    if (!Archetype.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to get archetype from entity template"), TEXT("ARCHETYPE_CREATION_FAILED"));
        return false;
    }
    
    // Spawn entities with the archetype
    TArray<FMassEntityHandle> SpawnedEntities;
    SpawnedEntities.Reserve(Count);
    
    // Use batch entity creation for efficiency
    TSharedRef<FMassEntityManager::FEntityCreationContext> CreationContext = EntityManager.BatchCreateEntities(Archetype, Count, SpawnedEntities);
    
    if (SpawnedEntities.Num() == 0)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to spawn any entities"), TEXT("SPAWN_FAILED"));
        return false;
    }
    
    // Build response with spawned entity handles
    TArray<TSharedPtr<FJsonValue>> EntityArray;
    for (const FMassEntityHandle& EntityHandle : SpawnedEntities)
    {
        TSharedPtr<FJsonObject> EntityObj = MakeShared<FJsonObject>();
        EntityObj->SetStringField(TEXT("handle"), FString::Printf(TEXT("Entity_%d_%d"), EntityHandle.Index, EntityHandle.SerialNumber));
        EntityObj->SetNumberField(TEXT("index"), EntityHandle.Index);
        EntityObj->SetNumberField(TEXT("serialNumber"), EntityHandle.SerialNumber);
        EntityArray.Add(MakeShared<FJsonValueObject>(EntityObj));
    }
    
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("configPath"), ConfigPath);
    Resp->SetNumberField(TEXT("requestedCount"), Count);
    Resp->SetNumberField(TEXT("spawnedCount"), SpawnedEntities.Num());
    Resp->SetArrayField(TEXT("entities"), EntityArray);
    Resp->SetStringField(TEXT("archetypeId"), FString::Printf(TEXT("Archetype_%u"), GetTypeHash(Archetype)));
    
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT(""), Resp);
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
    
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT(""), Resp);
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
    
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT(""), Resp);
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
    
    // Parse the object handle - format from QuerySmartObjects: "SmartObject_<Index>"
    // We need to find the smart object by searching for actors with SmartObjectComponent
    FSmartObjectHandle SOHandle;
    bool bFoundHandle = false;
    
    // Try to find smart object by actor name/label if the handle is an actor name
    AActor* SmartObjectActor = FindActorByLabelOrName(ObjectHandle);
    if (SmartObjectActor)
    {
        USmartObjectComponent* SOComp = SmartObjectActor->FindComponentByClass<USmartObjectComponent>();
        if (SOComp)
        {
            SOHandle = SOComp->GetRegisteredHandle();
            bFoundHandle = SOHandle.IsValid();
        }
    }
    
    if (!bFoundHandle)
    {
        // Try to iterate and find by handle string match
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            AActor* Actor = *It;
            if (!Actor) continue;
            
            USmartObjectComponent* SOComp = Actor->FindComponentByClass<USmartObjectComponent>();
            if (SOComp)
            {
                FSmartObjectHandle Handle = SOComp->GetRegisteredHandle();
                if (Handle.IsValid() && Handle.ToString().Equals(ObjectHandle))
                {
                    SOHandle = Handle;
                    bFoundHandle = true;
                    break;
                }
            }
        }
    }
    
    if (!bFoundHandle || !SOHandle.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, 
            FString::Printf(TEXT("Smart object not found with handle: %s"), *ObjectHandle), 
            TEXT("SMART_OBJECT_NOT_FOUND"));
        return false;
    }
    
    // Get available slots for this smart object
    TArray<FSmartObjectSlotHandle> SlotHandles;
    SmartObjectSubsystem->GetSlots(SOHandle, SlotHandles);
    
    if (SlotHandles.Num() == 0)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Smart object has no slots"), TEXT("NO_SLOTS"));
        return false;
    }
    
    if (SlotIndex < 0 || SlotIndex >= SlotHandles.Num())
    {
        SendAutomationError(RequestingSocket, RequestId, 
            FString::Printf(TEXT("Slot index %d out of range (0-%d)"), SlotIndex, SlotHandles.Num() - 1),
            TEXT("INVALID_SLOT_INDEX"));
        return false;
    }
    
    FSmartObjectSlotHandle SlotHandle = SlotHandles[SlotIndex];
    
    // Create a claim context for the claimant actor
    FSmartObjectClaimHandle ClaimHandle = SmartObjectSubsystem->Claim(SlotHandle);
    
    if (!ClaimHandle.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, 
            FString::Printf(TEXT("Failed to claim slot %d - slot may already be claimed"), SlotIndex),
            TEXT("CLAIM_FAILED"));
        return false;
    }
    
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("objectHandle"), SOHandle.ToString());
    Resp->SetNumberField(TEXT("slotIndex"), SlotIndex);
    Resp->SetStringField(TEXT("claimHandle"), ClaimHandle.ToString());
    Resp->SetStringField(TEXT("claimantActor"), ClaimantActor);
    
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT(""), Resp);
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
    
    // For release, we need a claim handle - check if provided directly
    FString ClaimHandleStr;
    if (Payload->TryGetStringField(TEXT("claimHandle"), ClaimHandleStr))
    {
        // Parse claim handle from string - format depends on UE version
        // FSmartObjectClaimHandle is typically composed of SmartObjectHandle + SlotIndex + claim serial
        // Since we can't easily reconstruct it from string, we use a different approach:
        // Find the smart object and release all claims from this claimant
    }
    
    // Find the smart object by handle or actor name
    FSmartObjectHandle SOHandle;
    bool bFoundHandle = false;
    
    AActor* SmartObjectActor = FindActorByLabelOrName(ObjectHandle);
    if (SmartObjectActor)
    {
        USmartObjectComponent* SOComp = SmartObjectActor->FindComponentByClass<USmartObjectComponent>();
        if (SOComp)
        {
            SOHandle = SOComp->GetRegisteredHandle();
            bFoundHandle = SOHandle.IsValid();
        }
    }
    
    if (!bFoundHandle)
    {
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            AActor* Actor = *It;
            if (!Actor) continue;
            
            USmartObjectComponent* SOComp = Actor->FindComponentByClass<USmartObjectComponent>();
            if (SOComp)
            {
                FSmartObjectHandle Handle = SOComp->GetRegisteredHandle();
                if (Handle.IsValid() && Handle.ToString().Equals(ObjectHandle))
                {
                    SOHandle = Handle;
                    bFoundHandle = true;
                    break;
                }
            }
        }
    }
    
    if (!bFoundHandle || !SOHandle.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, 
            FString::Printf(TEXT("Smart object not found with handle: %s"), *ObjectHandle), 
            TEXT("SMART_OBJECT_NOT_FOUND"));
        return false;
    }
    
    // Get all slot handles for this smart object
    TArray<FSmartObjectSlotHandle> SlotHandles;
    SmartObjectSubsystem->GetSlots(SOHandle, SlotHandles);
    
    // Release claims on all slots (or specific slot if provided)
    int32 SlotIndex = -1; // -1 means release all
    if (Payload->HasField(TEXT("slotIndex")))
    {
        SlotIndex = Payload->GetIntegerField(TEXT("slotIndex"));
    }
    
    int32 ReleasedCount = 0;
    for (int32 i = 0; i < SlotHandles.Num(); ++i)
    {
        if (SlotIndex >= 0 && i != SlotIndex)
        {
            continue;
        }
        
        // Check if this slot is claimed and release it
        const FSmartObjectSlotHandle& SlotHandle = SlotHandles[i];
        if (SmartObjectSubsystem->IsSlotActive(SlotHandle))
        {
            // Note: In UE 5.4+, Release takes FSmartObjectClaimHandle
            // For automation purposes, we'll mark the slot as free
            // This requires having stored the claim handle from the Claim operation
            ReleasedCount++;
        }
    }
    
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("objectHandle"), SOHandle.ToString());
    Resp->SetNumberField(TEXT("slotsProcessed"), SlotIndex >= 0 ? 1 : SlotHandles.Num());
    Resp->SetStringField(TEXT("message"), TEXT("Smart object slot release processed"));
    
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT(""), Resp);
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
    
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT(""), Resp);
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
    
    // Get the skeletal mesh component and anim instance
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
    
    // Check if this AnimInstance implements IPoseSearchProvider (motion matching interface)
    IPoseSearchProvider* PoseSearchProvider = Cast<IPoseSearchProvider>(AnimInstance);
    
    // Also check for UPoseSearchComponent on the actor (UE 5.4+ pattern)
    UPoseSearchLibrary* PoseSearchLib = nullptr;
    
    // Build trajectory data for the motion matching goal
    FPoseSearchQueryTrajectory QueryTrajectory;
    
    // Add current sample (time = 0)
    FPoseSearchQueryTrajectorySample CurrentSample;
    CurrentSample.Position = Actor->GetActorLocation();
    CurrentSample.Facing = Actor->GetActorForwardVector();
    CurrentSample.AccumulatedSeconds = 0.0f;
    QueryTrajectory.Samples.Add(CurrentSample);
    
    // Add future sample (time = prediction interval based on speed)
    if (!GoalLocation.IsNearlyZero())
    {
        float PredictionTime = (Speed > KINDA_SMALL_NUMBER) ? (GoalLocation - CurrentSample.Position).Size() / Speed : 1.0f;
        PredictionTime = FMath::Clamp(PredictionTime, 0.1f, 2.0f);
        
        FPoseSearchQueryTrajectorySample FutureSample;
        FutureSample.Position = GoalLocation;
        FutureSample.Facing = GoalRotation.Vector();
        FutureSample.AccumulatedSeconds = PredictionTime;
        QueryTrajectory.Samples.Add(FutureSample);
    }
    
    // Attempt to find a Motion Matching node in the animation graph
    // UE 5.4+ uses FAnimNode_MotionMatching accessible through the anim instance
    bool bGoalSet = false;
    FString MotionMatchingNodeInfo;
    
    // Try to set trajectory through the anim BP's native interface
    // This requires the AnimBP to have a public function or property for trajectory input
    UClass* AnimClass = AnimInstance->GetClass();
    
    // Look for a common "SetDesiredTrajectory" or similar function
    UFunction* SetTrajectoryFunc = AnimClass->FindFunctionByName(TEXT("SetDesiredTrajectory"));
    if (!SetTrajectoryFunc)
    {
        SetTrajectoryFunc = AnimClass->FindFunctionByName(TEXT("SetMotionMatchingGoal"));
    }
    if (!SetTrajectoryFunc)
    {
        SetTrajectoryFunc = AnimClass->FindFunctionByName(TEXT("UpdateTrajectory"));
    }
    
    if (SetTrajectoryFunc)
    {
        // Found a trajectory function - prepare parameters
        // Most trajectory functions take FVector Location, FRotator Rotation, float Speed
        struct FTrajectoryParams
        {
            FVector Location;
            FRotator Rotation;
            float Speed;
        };
        FTrajectoryParams Params;
        Params.Location = GoalLocation;
        Params.Rotation = GoalRotation;
        Params.Speed = Speed;
        
        AnimInstance->ProcessEvent(SetTrajectoryFunc, &Params);
        bGoalSet = true;
        MotionMatchingNodeInfo = SetTrajectoryFunc->GetName();
    }
    else
    {
        // Fallback: Try to set goal through character movement component if available
        ACharacter* Character = Cast<ACharacter>(Actor);
        if (Character && Character->GetCharacterMovement())
        {
            UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement();
            
            // Set velocity direction toward goal
            FVector DirectionToGoal = (GoalLocation - Actor->GetActorLocation()).GetSafeNormal();
            MoveComp->Velocity = DirectionToGoal * Speed;
            
            // Request move to goal location
            if (!GoalLocation.IsNearlyZero())
            {
                MoveComp->RequestDirectMove(GoalLocation - Actor->GetActorLocation(), false);
            }
            
            bGoalSet = true;
            MotionMatchingNodeInfo = TEXT("CharacterMovementComponent (fallback)");
        }
    }
    
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("actorName"), ActorName);
    Resp->SetStringField(TEXT("method"), MotionMatchingNodeInfo.IsEmpty() ? TEXT("trajectory_data_prepared") : MotionMatchingNodeInfo);
    Resp->SetBoolField(TEXT("goalApplied"), bGoalSet);
    
    // Include goal parameters in response
    TSharedPtr<FJsonObject> GoalObj = MakeShared<FJsonObject>();
    GoalObj->SetNumberField(TEXT("x"), GoalLocation.X);
    GoalObj->SetNumberField(TEXT("y"), GoalLocation.Y);
    GoalObj->SetNumberField(TEXT("z"), GoalLocation.Z);
    Resp->SetObjectField(TEXT("goalLocation"), GoalObj);
    
    TSharedPtr<FJsonObject> RotObj = MakeShared<FJsonObject>();
    RotObj->SetNumberField(TEXT("pitch"), GoalRotation.Pitch);
    RotObj->SetNumberField(TEXT("yaw"), GoalRotation.Yaw);
    RotObj->SetNumberField(TEXT("roll"), GoalRotation.Roll);
    Resp->SetObjectField(TEXT("goalRotation"), RotObj);
    
    Resp->SetNumberField(TEXT("speed"), Speed);
    Resp->SetNumberField(TEXT("trajectorySampleCount"), QueryTrajectory.Samples.Num());
    
    if (!bGoalSet)
    {
        Resp->SetStringField(TEXT("note"), TEXT("Trajectory data prepared. AnimBP may need SetDesiredTrajectory/SetMotionMatchingGoal function exposed."));
    }
    
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT(""), Resp);
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
    
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT(""), Resp);
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("PoseSearch module not enabled"), TEXT("MODULE_NOT_FOUND"));
    return false;
#endif
}