#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "Async/Async.h"

#if WITH_EDITOR
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Components/SceneComponent.h"
#include "Components/ActorComponent.h"
#include "EditorAssetLibrary.h"
#include "UObject/UObjectIterator.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#endif

/**
 * FSCSHandlers
 * 
 * Native C++ handlers for Simple Construction Script (SCS) Blueprint authoring.
 * Provides full programmatic control of Blueprint component hierarchies.
 */
class FSCSHandlers
{
public:
    // Get Blueprint SCS structure
    static TSharedPtr<FJsonObject> GetBlueprintSCS(const FString& BlueprintPath)
    {
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
        
#if WITH_EDITOR
        // Load blueprint
        UBlueprint* Blueprint = Cast<UBlueprint>(UEditorAssetLibrary::LoadAsset(BlueprintPath));
        if (!Blueprint)
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), TEXT("Blueprint not found or not a valid Blueprint asset"));
            return Result;
        }
        
        // Get SCS
        USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
        if (!SCS)
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), TEXT("Blueprint has no SimpleConstructionScript"));
            return Result;
        }
        
        // Build component tree
        TArray<TSharedPtr<FJsonValue>> Components;
        const TArray<USCS_Node*>& AllNodes = SCS->GetAllNodes();
        
        for (USCS_Node* Node : AllNodes)
        {
            if (Node)
            {
                TSharedPtr<FJsonObject> ComponentObj = MakeShareable(new FJsonObject);
                ComponentObj->SetStringField(TEXT("name"), Node->GetVariableName().ToString());
                ComponentObj->SetStringField(TEXT("class"), Node->ComponentClass ? Node->ComponentClass->GetName() : TEXT("Unknown"));
                ComponentObj->SetStringField(TEXT("parent"), Node->ParentComponentOrVariableName.ToString());
                
                // Add transform if component template exists (cast to SceneComponent for UE 5.6)
                if (Node->ComponentTemplate)
                {
                    FTransform Transform = FTransform::Identity;
                    if (USceneComponent* SceneComp = Cast<USceneComponent>(Node->ComponentTemplate))
                    {
                        Transform = SceneComp->GetRelativeTransform();
                    }
                    TSharedPtr<FJsonObject> TransformObj = MakeShareable(new FJsonObject);
                    
                    FVector Loc = Transform.GetLocation();
                    FRotator Rot = Transform.GetRotation().Rotator();
                    FVector Scale = Transform.GetScale3D();
                    
                    TransformObj->SetStringField(TEXT("location"), FString::Printf(TEXT("X=%.2f Y=%.2f Z=%.2f"), Loc.X, Loc.Y, Loc.Z));
                    TransformObj->SetStringField(TEXT("rotation"), FString::Printf(TEXT("P=%.2f Y=%.2f R=%.2f"), Rot.Pitch, Rot.Yaw, Rot.Roll));
                    TransformObj->SetStringField(TEXT("scale"), FString::Printf(TEXT("X=%.2f Y=%.2f Z=%.2f"), Scale.X, Scale.Y, Scale.Z));
                    
                    ComponentObj->SetObjectField(TEXT("transform"), TransformObj);
                }
                
                // Add child count
                ComponentObj->SetNumberField(TEXT("child_count"), Node->GetChildNodes().Num());
                
                Components.Add(MakeShareable(new FJsonValueObject(ComponentObj)));
            }
        }
        
        Result->SetBoolField(TEXT("success"), true);
        Result->SetArrayField(TEXT("components"), Components);
        Result->SetNumberField(TEXT("count"), Components.Num());
        Result->SetStringField(TEXT("blueprint_path"), BlueprintPath);
#else
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("SCS operations require editor build"));
#endif
        
        return Result;
    }
    
    // Add component to SCS
    static TSharedPtr<FJsonObject> AddSCSComponent(
        const FString& BlueprintPath,
        const FString& ComponentClass,
        const FString& ComponentName,
        const FString& ParentComponentName)
    {
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
        
#if WITH_EDITOR
        // Load blueprint
        UBlueprint* Blueprint = Cast<UBlueprint>(UEditorAssetLibrary::LoadAsset(BlueprintPath));
        if (!Blueprint)
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), TEXT("Blueprint not found"));
            return Result;
        }
        
        // Get or create SCS
        USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
        if (!SCS)
        {
            SCS = NewObject<USimpleConstructionScript>(Blueprint);
            Blueprint->SimpleConstructionScript = SCS;
        }
        
        // Find component class (UE 5.6: ANY_PACKAGE is deprecated, use nullptr)
        UClass* CompClass = FindObject<UClass>(nullptr, *ComponentClass);
        if (!CompClass)
        {
            // Try loading with Class prefix
            FString ClassPath = FString::Printf(TEXT("Class'/Script/Engine.%s'"), *ComponentClass);
            CompClass = LoadObject<UClass>(nullptr, *ClassPath);
        }
        
        if (!CompClass)
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Component class not found: %s"), *ComponentClass));
            return Result;
        }
        
        // Verify it's a component class
        if (!CompClass->IsChildOf(UActorComponent::StaticClass()))
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Class is not a component: %s"), *ComponentClass));
            return Result;
        }
        
        // Find parent node if specified
        USCS_Node* ParentNode = nullptr;
        if (!ParentComponentName.IsEmpty())
        {
            for (USCS_Node* Node : SCS->GetAllNodes())
            {
                if (Node && Node->GetVariableName().ToString().Equals(ParentComponentName, ESearchCase::IgnoreCase))
                {
                    ParentNode = Node;
                    break;
                }
            }
            
            if (!ParentNode)
            {
                Result->SetBoolField(TEXT("success"), false);
                Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Parent component not found: %s"), *ParentComponentName));
                return Result;
            }
        }
        
        // Check for duplicate name
        for (USCS_Node* Node : SCS->GetAllNodes())
        {
            if (Node && Node->GetVariableName().ToString().Equals(ComponentName, ESearchCase::IgnoreCase))
            {
                Result->SetBoolField(TEXT("success"), false);
                Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Component with name '%s' already exists"), *ComponentName));
                return Result;
            }
        }
        
        // Create new node
        USCS_Node* NewNode = SCS->CreateNode(CompClass, FName(*ComponentName));
        if (!NewNode)
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), TEXT("Failed to create SCS node"));
            return Result;
        }
        
        // Set parent or add as root
        if (ParentNode)
        {
            ParentNode->AddChildNode(NewNode);
        }
        else
        {
            SCS->AddNode(NewNode);
        }
        
        // Mark blueprint as modified and recompile
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Component '%s' added to SCS"), *ComponentName));
        Result->SetStringField(TEXT("component_name"), ComponentName);
        Result->SetStringField(TEXT("component_class"), CompClass->GetName());
        Result->SetStringField(TEXT("parent"), ParentComponentName.IsEmpty() ? TEXT("(root)") : ParentComponentName);
#else
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("SCS operations require editor build"));
#endif
        
        return Result;
    }
    
    // Remove component from SCS
    static TSharedPtr<FJsonObject> RemoveSCSComponent(
        const FString& BlueprintPath,
        const FString& ComponentName)
    {
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
        
#if WITH_EDITOR
        UBlueprint* Blueprint = Cast<UBlueprint>(UEditorAssetLibrary::LoadAsset(BlueprintPath));
        if (!Blueprint || !Blueprint->SimpleConstructionScript)
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), TEXT("Blueprint or SCS not found"));
            return Result;
        }
        
        USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
        
        // Find node to remove
        USCS_Node* NodeToRemove = nullptr;
        for (USCS_Node* Node : SCS->GetAllNodes())
        {
            if (Node && Node->GetVariableName().ToString().Equals(ComponentName, ESearchCase::IgnoreCase))
            {
                NodeToRemove = Node;
                break;
            }
        }
        
        if (!NodeToRemove)
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Component not found in SCS: %s"), *ComponentName));
            return Result;
        }
        
        // Remove node
        SCS->RemoveNode(NodeToRemove);
        
        // Mark blueprint as modified
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Component '%s' removed from SCS"), *ComponentName));
#else
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("SCS operations require editor build"));
#endif
        
        return Result;
    }
    
    // Reparent component in SCS
    static TSharedPtr<FJsonObject> ReparentSCSComponent(
        const FString& BlueprintPath,
        const FString& ComponentName,
        const FString& NewParentName)
    {
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
        
#if WITH_EDITOR
        UBlueprint* Blueprint = Cast<UBlueprint>(UEditorAssetLibrary::LoadAsset(BlueprintPath));
        if (!Blueprint || !Blueprint->SimpleConstructionScript)
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), TEXT("Blueprint or SCS not found"));
            return Result;
        }
        
        USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
        
        // Find component to reparent
        USCS_Node* ComponentNode = nullptr;
        for (USCS_Node* Node : SCS->GetAllNodes())
        {
            if (Node && Node->GetVariableName().ToString().Equals(ComponentName, ESearchCase::IgnoreCase))
            {
                ComponentNode = Node;
                break;
            }
        }
        
        if (!ComponentNode)
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Component not found: %s"), *ComponentName));
            return Result;
        }
        
        // Find new parent (empty string means root)
        USCS_Node* NewParentNode = nullptr;
        if (!NewParentName.IsEmpty())
        {
            for (USCS_Node* Node : SCS->GetAllNodes())
            {
                if (Node && Node->GetVariableName().ToString().Equals(NewParentName, ESearchCase::IgnoreCase))
                {
                    NewParentNode = Node;
                    break;
                }
            }
            
            if (!NewParentNode)
            {
                Result->SetBoolField(TEXT("success"), false);
                Result->SetStringField(TEXT("error"), FString::Printf(TEXT("New parent not found: %s"), *NewParentName));
                return Result;
            }
            
            // Check for circular dependency (UE 5.6: manually traverse hierarchy)
            USCS_Node* CheckNode = NewParentNode;
            while (CheckNode)
            {
                if (CheckNode == ComponentNode)
                {
                    Result->SetBoolField(TEXT("success"), false);
                    Result->SetStringField(TEXT("error"), TEXT("Cannot create circular parent-child relationship"));
                    return Result;
                }
                // Find parent by checking all nodes
                USCS_Node* ParentOfCheck = nullptr;
                for (USCS_Node* Candidate : SCS->GetAllNodes())
                {
                    if (Candidate && Candidate->GetChildNodes().Contains(CheckNode))
                    {
                        ParentOfCheck = Candidate;
                        break;
                    }
                }
                CheckNode = ParentOfCheck;
            }
        }
        
        // Remove from current parent (UE 5.6: find parent manually)
        USCS_Node* OldParent = nullptr;
        for (USCS_Node* Candidate : SCS->GetAllNodes())
        {
            if (Candidate && Candidate->GetChildNodes().Contains(ComponentNode))
            {
                OldParent = Candidate;
                break;
            }
        }
        
        if (OldParent)
        {
            OldParent->RemoveChildNode(ComponentNode);
        }
        else
        {
            // Was a root node
            SCS->RemoveNodeAndPromoteChildren(ComponentNode);
        }
        
        // Add to new parent
        if (NewParentNode)
        {
            NewParentNode->AddChildNode(ComponentNode);
        }
        else
        {
            SCS->AddNode(ComponentNode);
        }
        
        // Mark blueprint as modified
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Component '%s' reparented to '%s'"), 
            *ComponentName, 
            NewParentName.IsEmpty() ? TEXT("(root)") : *NewParentName));
#else
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("SCS operations require editor build"));
#endif
        
        return Result;
    }
    
    // Set component transform in SCS
    static TSharedPtr<FJsonObject> SetSCSComponentTransform(
        const FString& BlueprintPath,
        const FString& ComponentName,
        const TSharedPtr<FJsonObject>& TransformData)
    {
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
        
#if WITH_EDITOR
        UBlueprint* Blueprint = Cast<UBlueprint>(UEditorAssetLibrary::LoadAsset(BlueprintPath));
        if (!Blueprint || !Blueprint->SimpleConstructionScript)
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), TEXT("Blueprint or SCS not found"));
            return Result;
        }
        
        USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
        
        // Find component
        USCS_Node* ComponentNode = nullptr;
        for (USCS_Node* Node : SCS->GetAllNodes())
        {
            if (Node && Node->GetVariableName().ToString().Equals(ComponentName, ESearchCase::IgnoreCase))
            {
                ComponentNode = Node;
                break;
            }
        }
        
        if (!ComponentNode || !ComponentNode->ComponentTemplate)
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Component or template not found: %s"), *ComponentName));
            return Result;
        }
        
        // Parse transform from JSON
        FVector Location(0, 0, 0);
        FRotator Rotation(0, 0, 0);
        FVector Scale(1, 1, 1);
        
        // Parse location array [x, y, z]
        const TArray<TSharedPtr<FJsonValue>>* LocArray;
        if (TransformData->TryGetArrayField(TEXT("location"), LocArray) && LocArray->Num() >= 3)
        {
            Location.X = (*LocArray)[0]->AsNumber();
            Location.Y = (*LocArray)[1]->AsNumber();
            Location.Z = (*LocArray)[2]->AsNumber();
        }
        
        // Parse rotation array [pitch, yaw, roll]
        const TArray<TSharedPtr<FJsonValue>>* RotArray;
        if (TransformData->TryGetArrayField(TEXT("rotation"), RotArray) && RotArray->Num() >= 3)
        {
            Rotation.Pitch = (*RotArray)[0]->AsNumber();
            Rotation.Yaw = (*RotArray)[1]->AsNumber();
            Rotation.Roll = (*RotArray)[2]->AsNumber();
        }
        
        // Parse scale array [x, y, z]
        const TArray<TSharedPtr<FJsonValue>>* ScaleArray;
        if (TransformData->TryGetArrayField(TEXT("scale"), ScaleArray) && ScaleArray->Num() >= 3)
        {
            Scale.X = (*ScaleArray)[0]->AsNumber();
            Scale.Y = (*ScaleArray)[1]->AsNumber();
            Scale.Z = (*ScaleArray)[2]->AsNumber();
        }
        
        // Apply transform to component template
        FTransform NewTransform(Rotation, Location, Scale);
        
        if (USceneComponent* SceneComp = Cast<USceneComponent>(ComponentNode->ComponentTemplate))
        {
            SceneComp->SetRelativeTransform(NewTransform);
            
            // Mark blueprint as modified
            FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
            
            Result->SetBoolField(TEXT("success"), true);
            Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Transform set for component '%s'"), *ComponentName));
        }
        else
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), TEXT("Component is not a SceneComponent (no transform)"));
        }
#else
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("SCS operations require editor build"));
#endif
        
        return Result;
    }
    
    // Set component property in SCS
    static TSharedPtr<FJsonObject> SetSCSComponentProperty(
        const FString& BlueprintPath,
        const FString& ComponentName,
        const FString& PropertyName,
        const FString& PropertyValueJson)
    {
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
        
#if WITH_EDITOR
        UBlueprint* Blueprint = Cast<UBlueprint>(UEditorAssetLibrary::LoadAsset(BlueprintPath));
        if (!Blueprint || !Blueprint->SimpleConstructionScript)
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), TEXT("Blueprint or SCS not found"));
            return Result;
        }
        
        USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
        
        // Find component
        USCS_Node* ComponentNode = nullptr;
        for (USCS_Node* Node : SCS->GetAllNodes())
        {
            if (Node && Node->GetVariableName().ToString().Equals(ComponentName, ESearchCase::IgnoreCase))
            {
                ComponentNode = Node;
                break;
            }
        }
        
        if (!ComponentNode || !ComponentNode->ComponentTemplate)
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Component or template not found: %s"), *ComponentName));
            return Result;
        }
        
        // Find property
        FProperty* Property = ComponentNode->ComponentTemplate->GetClass()->FindPropertyByName(FName(*PropertyName));
        if (!Property)
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Property not found: %s"), *PropertyName));
            return Result;
        }
        
        // Parse value and attempt to set
        // This is simplified - full implementation would handle all property types
        TSharedPtr<FJsonObject> ValueObj;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PropertyValueJson);
        
        if (FJsonSerializer::Deserialize(Reader, ValueObj) && ValueObj.IsValid())
        {
            // Handle simple types for now
            if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
            {
                bool Value;
                if (ValueObj->TryGetBoolField(TEXT("value"), Value))
                {
                    BoolProp->SetPropertyValue_InContainer(ComponentNode->ComponentTemplate, Value);
                }
            }
            else if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
            {
                double Value;
                if (ValueObj->TryGetNumberField(TEXT("value"), Value))
                {
                    FloatProp->SetPropertyValue_InContainer(ComponentNode->ComponentTemplate, static_cast<float>(Value));
                }
            }
            else if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
            {
                int32 Value;
                if (ValueObj->TryGetNumberField(TEXT("value"), Value))
                {
                    IntProp->SetPropertyValue_InContainer(ComponentNode->ComponentTemplate, Value);
                }
            }
        }
        
        // Mark blueprint as modified
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Property '%s' set on component '%s'"), *PropertyName, *ComponentName));
#else
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("SCS operations require editor build"));
#endif
        
        return Result;
    }
};

// Integration with HandleAutomationRequest - add these handlers
bool UMcpAutomationBridgeSubsystem::HandleSCSAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    
    if (Lower.Equals(TEXT("get_blueprint_scs"), ESearchCase::IgnoreCase))
    {
        FString BlueprintPath;
        if (!Payload->TryGetStringField(TEXT("blueprint_path"), BlueprintPath))
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("blueprint_path required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        
        AsyncTask(ENamedThreads::GameThread, [this, RequestId, BlueprintPath, RequestingSocket]() {
            TSharedPtr<FJsonObject> Response = FSCSHandlers::GetBlueprintSCS(BlueprintPath);
            bool Success = Response->GetBoolField(TEXT("success"));
            FString Message = Success ? TEXT("Retrieved SCS structure") : Response->GetStringField(TEXT("error"));
            SendAutomationResponse(RequestingSocket, RequestId, Success, Message, Response, Success ? FString() : TEXT("GET_SCS_FAILED"));
        });
        return true;
    }
    
    if (Lower.Equals(TEXT("add_scs_component"), ESearchCase::IgnoreCase))
    {
        FString BlueprintPath, ComponentClass, ComponentName, ParentName;
        if (!Payload->TryGetStringField(TEXT("blueprint_path"), BlueprintPath) ||
            !Payload->TryGetStringField(TEXT("component_class"), ComponentClass) ||
            !Payload->TryGetStringField(TEXT("component_name"), ComponentName))
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("blueprint_path, component_class, and component_name required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        
        Payload->TryGetStringField(TEXT("parent_component"), ParentName);
        
        AsyncTask(ENamedThreads::GameThread, [this, RequestId, BlueprintPath, ComponentClass, ComponentName, ParentName, RequestingSocket]() {
            TSharedPtr<FJsonObject> Response = FSCSHandlers::AddSCSComponent(BlueprintPath, ComponentClass, ComponentName, ParentName);
            bool Success = Response->GetBoolField(TEXT("success"));
            FString Message = Success ? Response->GetStringField(TEXT("message")) : Response->GetStringField(TEXT("error"));
            SendAutomationResponse(RequestingSocket, RequestId, Success, Message, Response, Success ? FString() : TEXT("ADD_SCS_COMPONENT_FAILED"));
        });
        return true;
    }
    
    if (Lower.Equals(TEXT("remove_scs_component"), ESearchCase::IgnoreCase))
    {
        FString BlueprintPath, ComponentName;
        if (!Payload->TryGetStringField(TEXT("blueprint_path"), BlueprintPath) ||
            !Payload->TryGetStringField(TEXT("component_name"), ComponentName))
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("blueprint_path and component_name required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        
        AsyncTask(ENamedThreads::GameThread, [this, RequestId, BlueprintPath, ComponentName, RequestingSocket]() {
            TSharedPtr<FJsonObject> Response = FSCSHandlers::RemoveSCSComponent(BlueprintPath, ComponentName);
            bool Success = Response->GetBoolField(TEXT("success"));
            FString Message = Success ? Response->GetStringField(TEXT("message")) : Response->GetStringField(TEXT("error"));
            SendAutomationResponse(RequestingSocket, RequestId, Success, Message, Response, Success ? FString() : TEXT("REMOVE_SCS_COMPONENT_FAILED"));
        });
        return true;
    }
    
    if (Lower.Equals(TEXT("reparent_scs_component"), ESearchCase::IgnoreCase))
    {
        FString BlueprintPath, ComponentName, NewParent;
        if (!Payload->TryGetStringField(TEXT("blueprint_path"), BlueprintPath) ||
            !Payload->TryGetStringField(TEXT("component_name"), ComponentName) ||
            !Payload->TryGetStringField(TEXT("new_parent"), NewParent))
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("blueprint_path, component_name, and new_parent required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        
        AsyncTask(ENamedThreads::GameThread, [this, RequestId, BlueprintPath, ComponentName, NewParent, RequestingSocket]() {
            TSharedPtr<FJsonObject> Response = FSCSHandlers::ReparentSCSComponent(BlueprintPath, ComponentName, NewParent);
            bool Success = Response->GetBoolField(TEXT("success"));
            FString Message = Success ? Response->GetStringField(TEXT("message")) : Response->GetStringField(TEXT("error"));
            SendAutomationResponse(RequestingSocket, RequestId, Success, Message, Response, Success ? FString() : TEXT("REPARENT_SCS_COMPONENT_FAILED"));
        });
        return true;
    }
    
    if (Lower.Equals(TEXT("set_scs_component_transform"), ESearchCase::IgnoreCase))
    {
        FString BlueprintPath, ComponentName;
        if (!Payload->TryGetStringField(TEXT("blueprint_path"), BlueprintPath) ||
            !Payload->TryGetStringField(TEXT("component_name"), ComponentName))
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("blueprint_path and component_name required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        
        // Extract transform data
        TSharedPtr<FJsonObject> TransformData = MakeShareable(new FJsonObject);
        
        const TArray<TSharedPtr<FJsonValue>>* LocArray;
        if (Payload->TryGetArrayField(TEXT("location"), LocArray))
        {
            TransformData->SetArrayField(TEXT("location"), *LocArray);
        }
        
        const TArray<TSharedPtr<FJsonValue>>* RotArray;
        if (Payload->TryGetArrayField(TEXT("rotation"), RotArray))
        {
            TransformData->SetArrayField(TEXT("rotation"), *RotArray);
        }
        
        const TArray<TSharedPtr<FJsonValue>>* ScaleArray;
        if (Payload->TryGetArrayField(TEXT("scale"), ScaleArray))
        {
            TransformData->SetArrayField(TEXT("scale"), *ScaleArray);
        }
        
        AsyncTask(ENamedThreads::GameThread, [this, RequestId, BlueprintPath, ComponentName, TransformData, RequestingSocket]() {
            TSharedPtr<FJsonObject> Response = FSCSHandlers::SetSCSComponentTransform(BlueprintPath, ComponentName, TransformData);
            bool Success = Response->GetBoolField(TEXT("success"));
            FString Message = Success ? Response->GetStringField(TEXT("message")) : Response->GetStringField(TEXT("error"));
            SendAutomationResponse(RequestingSocket, RequestId, Success, Message, Response, Success ? FString() : TEXT("SET_SCS_TRANSFORM_FAILED"));
        });
        return true;
    }
    
    if (Lower.Equals(TEXT("set_scs_component_property"), ESearchCase::IgnoreCase))
    {
        FString BlueprintPath, ComponentName, PropertyName, PropertyValue;
        if (!Payload->TryGetStringField(TEXT("blueprint_path"), BlueprintPath) ||
            !Payload->TryGetStringField(TEXT("component_name"), ComponentName) ||
            !Payload->TryGetStringField(TEXT("property_name"), PropertyName) ||
            !Payload->TryGetStringField(TEXT("property_value"), PropertyValue))
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("blueprint_path, component_name, property_name, and property_value required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        
        AsyncTask(ENamedThreads::GameThread, [this, RequestId, BlueprintPath, ComponentName, PropertyName, PropertyValue, RequestingSocket]() {
            TSharedPtr<FJsonObject> Response = FSCSHandlers::SetSCSComponentProperty(BlueprintPath, ComponentName, PropertyName, PropertyValue);
            bool Success = Response->GetBoolField(TEXT("success"));
            FString Message = Success ? Response->GetStringField(TEXT("message")) : Response->GetStringField(TEXT("error"));
            SendAutomationResponse(RequestingSocket, RequestId, Success, Message, Response, Success ? FString() : TEXT("SET_SCS_PROPERTY_FAILED"));
        });
        return true;
    }
    
    return false;
}
