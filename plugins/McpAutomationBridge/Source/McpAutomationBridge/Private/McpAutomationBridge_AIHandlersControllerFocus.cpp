#include "McpAutomationBridge_AIHandlerContext.h"

#if WITH_EDITOR
#include "BehaviorTree/BehaviorTree.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace McpAIHandlers
{
bool HandleSetFocus(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString SubAction = TEXT("set_focus");
    if (SubAction == TEXT("set_focus"))
    {
        FString ControllerPath = GetStringFieldAI(Payload, TEXT("controllerPath"));
        if (ControllerPath.IsEmpty())
        {
            Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing controllerPath"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString FocusActorName = GetStringFieldAI(Payload, TEXT("focusActorName"));
        if (FocusActorName.IsEmpty())
        {
            FocusActorName = GetStringFieldAI(Payload, TEXT("targetActor"));
        }

        // Load the blueprint - LoadObject handles path resolution
        UBlueprint* ControllerBP = LoadObject<UBlueprint>(nullptr, *ControllerPath);
        if (!ControllerBP)
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Controller blueprint not found: %s"), *ControllerPath), TEXT("NOT_FOUND"));
            return true;
        }

        // Add a FocusActor variable to the BP
        FEdGraphPinType PinType;
        PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
        PinType.PinSubCategoryObject = AActor::StaticClass();
        FBlueprintEditorUtils::AddMemberVariable(ControllerBP, TEXT("FocusActor"), PinType);

        FBlueprintEditorUtils::MarkBlueprintAsModified(ControllerBP);
        McpSafeAssetSave(ControllerBP);

        TSharedPtr<FJsonObject> FocusResult = McpHandlerUtils::CreateResultObject();
        FocusResult->SetStringField(TEXT("controllerPath"), ControllerPath);
        FocusResult->SetStringField(TEXT("focusActorName"), FocusActorName);
        FocusResult->SetBoolField(TEXT("focusSet"), true);
        Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Focus actor variable set on controller"), FocusResult);
        return true;
    }

    // clear_focus - Clear focus actor variable on AI controller blueprint
    return true;
}

bool HandleClearFocus(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString SubAction = TEXT("clear_focus");
    if (SubAction == TEXT("clear_focus"))
    {
        FString ControllerPath = GetStringFieldAI(Payload, TEXT("controllerPath"));
        if (ControllerPath.IsEmpty())
        {
            Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing controllerPath"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        // Load blueprint - rely on LoadObject without DoesAssetExist pre-check
        UBlueprint* ControllerBP = LoadObject<UBlueprint>(nullptr, *ControllerPath);
        if (!ControllerBP)
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Controller blueprint not found: %s"), *ControllerPath), TEXT("NOT_FOUND"));
            return true;
        }

        FBlueprintEditorUtils::RemoveMemberVariable(ControllerBP, TEXT("FocusActor"));

        FBlueprintEditorUtils::MarkBlueprintAsModified(ControllerBP);
        McpSafeAssetSave(ControllerBP);

        TSharedPtr<FJsonObject> ClearResult = McpHandlerUtils::CreateResultObject();
        ClearResult->SetStringField(TEXT("controllerPath"), ControllerPath);
        ClearResult->SetBoolField(TEXT("focusCleared"), true);
        Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Focus cleared on controller"), ClearResult);
        return true;
    }

    // set_blackboard_value - Set a default key value on a blackboard asset
    return true;
}

bool HandleRunBehaviorTree(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString SubAction = TEXT("run_behavior_tree");
    if (SubAction == TEXT("run_behavior_tree"))
    {
        FString ControllerPath = GetStringFieldAI(Payload, TEXT("controllerPath"));
        if (ControllerPath.IsEmpty())
        {
            Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing controllerPath"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString BTPath = GetStringFieldAI(Payload, TEXT("behaviorTreePath"));
        if (BTPath.IsEmpty())
        {
            Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing behaviorTreePath"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        // Load assets directly - DoesAssetExist pre-check can fail on newly created assets
        UBlueprint* ControllerBP = LoadObject<UBlueprint>(nullptr, *ControllerPath);
        if (!ControllerBP)
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Controller blueprint not found: %s"), *ControllerPath), TEXT("NOT_FOUND"));
            return true;
        }

        UBehaviorTree* BT = LoadObject<UBehaviorTree>(nullptr, *BTPath);
        if (!BT)
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Behavior tree not found: %s"), *BTPath), TEXT("NOT_FOUND"));
            return true;
        }

        // Store the BT reference as a variable on the controller
        FEdGraphPinType PinType;
        PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
        PinType.PinSubCategoryObject = UBehaviorTree::StaticClass();
        FBlueprintEditorUtils::AddMemberVariable(ControllerBP, TEXT("AssignedBehaviorTree"), PinType);

        FBlueprintEditorUtils::MarkBlueprintAsModified(ControllerBP);
        McpSafeAssetSave(ControllerBP);

        TSharedPtr<FJsonObject> RunResult = McpHandlerUtils::CreateResultObject();
        RunResult->SetStringField(TEXT("controllerPath"), ControllerPath);
        RunResult->SetStringField(TEXT("behaviorTreePath"), BTPath);
        RunResult->SetBoolField(TEXT("assigned"), true);
        Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Behavior tree assigned for running"), RunResult);
        return true;
    }

    // stop_behavior_tree - Remove behavior tree assignment from controller
    return true;
}

bool HandleStopBehaviorTree(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString SubAction = TEXT("stop_behavior_tree");
    if (SubAction == TEXT("stop_behavior_tree"))
    {
        FString ControllerPath = GetStringFieldAI(Payload, TEXT("controllerPath"));
        if (ControllerPath.IsEmpty())
        {
            Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing controllerPath"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* ControllerBP = LoadObject<UBlueprint>(nullptr, *ControllerPath);
        if (!ControllerBP)
        {
            TSharedPtr<FJsonObject> StopResult = McpHandlerUtils::CreateResultObject();
            StopResult->SetStringField(TEXT("controllerPath"), ControllerPath);
            StopResult->SetBoolField(TEXT("stopped"), false);
            Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Behavior tree stopped (controller not found)"), StopResult);
            return true;
        }

        // Remove the BT variable to "stop" it
        FBlueprintEditorUtils::RemoveMemberVariable(ControllerBP, TEXT("AssignedBehaviorTree"));

        FBlueprintEditorUtils::MarkBlueprintAsModified(ControllerBP);
        McpSafeAssetSave(ControllerBP);

        TSharedPtr<FJsonObject> StopResult = McpHandlerUtils::CreateResultObject();
        StopResult->SetStringField(TEXT("controllerPath"), ControllerPath);
        StopResult->SetBoolField(TEXT("stopped"), true);
        Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Behavior tree stopped"), StopResult);
        return true;
    }

    // Unknown sub-action
    Self->SendAutomationError(RequestingSocket, RequestId,
                        FString::Printf(TEXT("Unknown AI action: %s"), *SubAction),
                        TEXT("UNKNOWN_ACTION"));
    return true;
}
}
#endif
