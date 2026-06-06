#include "McpAutomationBridge_AIHandlerContext.h"

#if WITH_EDITOR
#include "McpAutomationBridge_AIBehaviorTreeGraphFeature.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/UnrealType.h"

namespace McpAIHandlers
{
bool HandleAssignBlackboard(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString SubAction = TEXT("assign_blackboard");
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    if (SubAction == TEXT("assign_blackboard"))
    {
        FString ControllerPath = GetStringFieldAI(Payload, TEXT("controllerPath"));
        FString BehaviorTreePath = GetStringFieldAI(Payload, TEXT("behaviorTreePath"));
        FString BlackboardPath = GetStringFieldAI(Payload, TEXT("blackboardPath"));

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

            BT->Modify();
            BT->BlackboardAsset = BB;

#if MCP_AI_HAS_BEHAVIOR_TREE_GRAPH
            if (UBehaviorTreeGraph* BTGraph = Cast<UBehaviorTreeGraph>(BT->BTGraph))
            {
                UClass* BTRootNodeClass = FindObject<UClass>(nullptr, TEXT("/Script/BehaviorTreeEditor.BehaviorTreeGraphNode_Root"));
                BTGraph->Modify();
                for (UEdGraphNode* GraphNode : BTGraph->Nodes)
                {
                    if (BTRootNodeClass && GraphNode && GraphNode->GetClass()->IsChildOf(BTRootNodeClass))
                    {
                        UBehaviorTreeGraphNode_Root* RootNode = static_cast<UBehaviorTreeGraphNode_Root*>(GraphNode);
                        RootNode->Modify();
                        RootNode->BlackboardAsset = BB;
                        BTGraph->UpdateBlackboardChange();
                        break;
                    }
                }
            }
#endif
            BT->MarkPackageDirty();
            bool bSaved = McpSafeAssetSave(BT);

            Result->SetStringField(TEXT("behaviorTreePath"), BehaviorTreePath);
            Result->SetStringField(TEXT("blackboardPath"), BlackboardPath);
            Result->SetBoolField(TEXT("saved"), bSaved);
            Result->SetStringField(TEXT("message"), TEXT("Blackboard assigned to Behavior Tree"));
            McpHandlerUtils::AddVerification(Result, BT);
            Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Blackboard assigned to Behavior Tree"), Result);
            return true;
        }

        // CRITICAL: Remove DoesAssetExist pre-check - newly created assets may not yet be
        // indexed in the asset registry. Rely on LoadObject null-check instead.
        UBlueprint* ControllerBP = LoadObject<UBlueprint>(nullptr, *ControllerPath);
        if (!ControllerBP)
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("AI Controller not found: %s"), *ControllerPath), TEXT("NOT_FOUND"));
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
                        // Set the default value for the variable
                        FProperty* NewProp = ControllerBP->GeneratedClass->FindPropertyByName(VarName);
                        if (FObjectProperty* ObjProp = CastField<FObjectProperty>(NewProp))
                        {
                            ObjProp->SetObjectPropertyValue(ObjProp->ContainerPtrToValuePtr<void>(CDO), BB);
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
    // =========================================================================

    return true;
}
}
#endif
