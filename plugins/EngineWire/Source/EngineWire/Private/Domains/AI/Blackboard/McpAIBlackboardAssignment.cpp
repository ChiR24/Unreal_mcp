#include "Domains/AI/McpAIContext.h"

#include "Domains/BehaviorTree/McpBTPrivate.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPCompileHelpers.h"
#include "Foundation/McpScopedEditorTransaction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/UnrealType.h"

namespace McpAIHandlers
{
// Implements the "assign_blackboard" action.
bool HandleAssignBlackboard(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    FString ControllerPath = GetJsonStringField(Payload, TEXT("controllerPath"));
    FString BehaviorTreePath = GetJsonStringField(Payload, TEXT("behaviorTreePath"));
    FString BlackboardPath = GetJsonStringField(Payload, TEXT("blackboardPath"));

    if (ControllerPath.IsEmpty() && !BehaviorTreePath.IsEmpty())
    {
        UBehaviorTree* BT = LoadObject<UBehaviorTree>(nullptr, *BehaviorTreePath);
        if (!BT)
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Behavior tree not found: %s"), *BehaviorTreePath), TEXT("NOT_FOUND"));
            return true;
        }

        UBlackboardData* BB = LoadObject<UBlackboardData>(nullptr, *BlackboardPath);
        if (!BB)
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Blackboard not found: %s"), *BlackboardPath), TEXT("NOT_FOUND"));
            return true;
        }

        // The tree's asset field and its graph's root node both name the Blackboard: one undo step for both.
        TArray<UObject*> Edited = {BT};
#if MCP_HAS_BEHAVIOR_TREE_GRAPH
        UBehaviorTreeGraph* BTGraph = Cast<UBehaviorTreeGraph>(BT->BTGraph);
        UBehaviorTreeGraphNode_Root* RootNode = McpBehaviorTreeHandlers::FindBehaviorTreeGraphRoot(BTGraph);
        if (BTGraph)
        {
            for (UObject* Object : TArray<UObject*>{BTGraph, RootNode})
            {
                if (Object)
                {
                    Object->SetFlags(RF_Transactional);
                    Edited.Add(Object);
                }
            }
        }
#endif
        {
            const FMcpAssetEdit Edit(TEXT("Assign Blackboard"), Edited, Result);
            BT->BlackboardAsset = BB;
#if MCP_HAS_BEHAVIOR_TREE_GRAPH
            if (RootNode)
            {
                RootNode->BlackboardAsset = BB;
                BTGraph->UpdateBlackboardChange();
            }
#endif
            BT->MarkPackageDirty();
        }
        bool bSaved = McpSafeAssetSave(BT);

        Result->SetStringField(TEXT("behaviorTreePath"), BehaviorTreePath);
        Result->SetStringField(TEXT("blackboardPath"), BlackboardPath);
        Result->SetBoolField(TEXT("saved"), bSaved);
        Result->SetStringField(TEXT("message"), TEXT("Blackboard assigned to Behavior Tree"));
        McpHandlerUtils::AddVerification(Result, BT);
        Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Blackboard assigned to Behavior Tree"), Result);
        return true;
    }

    // LoadObject, not a registry check: a Blueprint created moments ago may not be in the asset registry yet.
    UBlueprint* ControllerBP = LoadObject<UBlueprint>(nullptr, *ControllerPath);
    if (!ControllerBP)
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("AI Controller not found: %s"), *ControllerPath), TEXT("NOT_FOUND"));
        return true;
    }
    // Any other Blueprint took the success reply below with nothing assigned.
    if (!ControllerBP->GeneratedClass || !ControllerBP->GeneratedClass->IsChildOf(AAIController::StaticClass()))
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("%s is not an AI Controller Blueprint (its parent class is %s), so it holds no Blackboard. Nothing was changed."),
                *ControllerPath, ControllerBP->ParentClass ? *ControllerBP->ParentClass->GetName() : TEXT("none")), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UBlackboardData* BB = LoadObject<UBlackboardData>(nullptr, *BlackboardPath);
    if (!BB)
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Blackboard not found: %s"), *BlackboardPath), TEXT("NOT_FOUND"));
        return true;
    }

    // Set default Blackboard property on the generated class CDO using reflection
    if (ControllerBP->GeneratedClass)
    {
        if (AAIController* CDO = Cast<AAIController>(ControllerBP->GeneratedClass->GetDefaultObject()))
        {
            // Use reflection to find and set Blackboard-related properties
            bool bPropertySet = false;

            // Try to find a UBlackboardData* property on the CDO
            for (TFieldIterator<FObjectProperty> PropIt(ControllerBP->GeneratedClass); PropIt; ++PropIt)
            {
                FObjectProperty* ObjProp = *PropIt;
                if (ObjProp && ObjProp->PropertyClass && ObjProp->PropertyClass->IsChildOf(UBlackboardData::StaticClass()))
                {
                    // Found a BlackboardData property - set it
                    ObjProp->SetObjectPropertyValue(ObjProp->ContainerPtrToValuePtr<void>(CDO), BB);
                    bPropertySet = true;
                    Result->SetStringField(TEXT("propertyName"), ObjProp->GetName());
                    break;
                }
            }

            // If no existing property found, add a Blueprint variable for the Blackboard reference
            if (!bPropertySet)
            {
                // Add a Blueprint variable to store the BlackboardData reference
                FEdGraphPinType PinType;
                PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
                PinType.PinSubCategoryObject = UBlackboardData::StaticClass();

                const FName VarName = TEXT("DefaultBlackboard");
                if (FBlueprintEditorUtils::AddMemberVariable(ControllerBP, VarName, PinType))
                {
                    // The variable is a property of the class only after a compile: looked up before one it was never found,
                    // so it was made with no default and propertyAssigned read false. The compile makes a new default object.
                    McpSafeCompileBlueprint(ControllerBP);
                    UObject* Defaults = ControllerBP->GeneratedClass ? ControllerBP->GeneratedClass->GetDefaultObject() : nullptr;
                    FObjectProperty* ObjProp = Defaults ? CastField<FObjectProperty>(ControllerBP->GeneratedClass->FindPropertyByName(VarName)) : nullptr;
                    if (ObjProp)
                    {
                        ObjProp->SetObjectPropertyValue(ObjProp->ContainerPtrToValuePtr<void>(Defaults), BB);
                        bPropertySet = true;
                    }
                }
                Result->SetStringField(TEXT("propertyName"), VarName.ToString());
            }

            Result->SetBoolField(TEXT("propertyAssigned"), bPropertySet);
            Result->SetStringField(TEXT("message"), bPropertySet
                ? TEXT("Blackboard property assigned on CDO (call UseBlackboard in BeginPlay with this asset)")
                : TEXT("Blackboard reference registered (call UseBlackboard in BeginPlay with this asset)"));
        }
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(ControllerBP);
    bool bSaved = McpSafeAssetSave(ControllerBP);
    Result->SetBoolField(TEXT("saved"), bSaved);
    Result->SetStringField(TEXT("controllerPath"), ControllerPath);
    Result->SetStringField(TEXT("blackboardPath"), BlackboardPath);
    McpHandlerUtils::AddVerification(Result, ControllerBP);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Blackboard reference set"), Result);
    return true;
}
}
