#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/AnimationAuthoring/McpAnimSupport.h"

namespace McpAnimationAuthoring {

TSharedPtr<FJsonObject> HandleBlueprintStateMachineActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
    if (SubAction == TEXT("add_state_machine"))
    {
        FString BlueprintPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("blueprintPath"), TEXT("")));
        FString StateMachineName = GetJsonStringField(Params, TEXT("stateMachineName"), TEXT(""));
        if (StateMachineName.IsEmpty())
        {
            // Sibling create_state_machine takes machineName; accept both.
            StateMachineName = GetJsonStringField(Params, TEXT("machineName"), GetJsonStringField(Params, TEXT("name"), TEXT("")));
        }
        int32 NodePosX = static_cast<int32>(GetJsonNumberField(Params, TEXT("positionX"), 0));
        int32 NodePosY = static_cast<int32>(GetJsonNumberField(Params, TEXT("positionY"), 0));
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

        if (StateMachineName.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("stateMachineName is required"), TEXT("MISSING_STATE_MACHINE_NAME"));
        }

        UEdGraph* AnimGraph = nullptr;
        UAnimBlueprint* AnimBP = LoadAnimBlueprintOrFail(BlueprintPath, Response, &AnimGraph);
        if (!AnimBP)
        {
            return Response;
        }

        if (UAnimGraphNode_StateMachine* ExistingSMNode = FindStateMachineNode(AnimGraph, StateMachineName))
        {
            Response->SetStringField(TEXT("nodeName"), ExistingSMNode->GetStateMachineName());
            Response->SetBoolField(TEXT("existingAsset"), true);
            ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("State machine '%s' already exists in %s"), *StateMachineName, *BlueprintPath));
            return Response;
        }

        FString SMError, SMCode;
        if (!CreateStateMachineNode(AnimBP, *AnimGraph, StateMachineName, NodePosX, NodePosY, SMError, SMCode))
        {
            ANIM_ERROR_RESPONSE(SMError, SMCode);
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
        SaveAnimAsset(AnimBP, bSave);

        Response->SetStringField(TEXT("nodeName"), StateMachineName);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("State machine '%s' created with entry node"), *StateMachineName));
        return Response;
    }
    return nullptr;
}

} // namespace McpAnimationAuthoring
