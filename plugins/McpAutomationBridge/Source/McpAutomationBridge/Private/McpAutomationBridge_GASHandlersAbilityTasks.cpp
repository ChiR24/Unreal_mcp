#include "McpAutomationBridge_GASPayloadFields.h"
#include "McpAutomationBridge_GASRequestContext.h"
#include "McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "McpAutomationBridgeHelpersSafeOperationsFacade.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR && MCP_HAS_GAS
#include "Abilities/GameplayAbility.h"
#include "Dom/JsonValue.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "GameplayTagContainer.h"
#include "Kismet2/BlueprintEditorUtils.h"
#endif

#if WITH_EDITOR && MCP_HAS_GAS
namespace McpGASHandlers
{
bool HandleGASAbilityTasks(const FGASRequestContext& Context, const FString& SubAction)
{
    UMcpAutomationBridgeSubsystem* Bridge = Context.Subsystem;
    const FString& RequestId = Context.RequestId;
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket = Context.RequestingSocket;
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    const FString& Name = Context.Name;
    const FString& Path = Context.Path;
    const FString& BlueprintPath = Context.BlueprintPath;
    const FString& AssetPath = Context.AssetPath;

    if (SubAction == TEXT("add_ability_task"))
    {
        if (BlueprintPath.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString TaskType = GetStringFieldGAS(Payload, TEXT("taskType"));
        if (TaskType.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing taskType."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString TaskClassName = GetStringFieldGAS(Payload, TEXT("taskClassName"));

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint || !Blueprint->GeneratedClass)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        // Verify this is a GameplayAbility blueprint
        if (!Blueprint->GeneratedClass->IsChildOf(UGameplayAbility::StaticClass()))
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint is not a GameplayAbility"), TEXT("INVALID_TYPE"));
            return true;
        }

        // Create meaningful task configuration variables
        FString TaskVarPrefix = FString::Printf(TEXT("Task_%s"), *TaskType);
        TArray<FString> VariablesAdded;

        // 1. Task active state tracking
        FEdGraphPinType BoolPinType;
        BoolPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
        FString ActiveVarName = FString::Printf(TEXT("b%s_Active"), *TaskVarPrefix);
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*ActiveVarName), BoolPinType);
        FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, FName(*ActiveVarName), nullptr, FText::FromString(TEXT("Ability Tasks")));
        VariablesAdded.Add(ActiveVarName);

        // 2. Task class reference (soft class reference to the AbilityTask)
        FEdGraphPinType ClassPinType;
        ClassPinType.PinCategory = UEdGraphSchema_K2::PC_SoftClass;
        ClassPinType.PinSubCategoryObject = UObject::StaticClass();
        FString ClassVarName = FString::Printf(TEXT("%s_Class"), *TaskVarPrefix);
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*ClassVarName), ClassPinType);
        FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, FName(*ClassVarName), nullptr, FText::FromString(TEXT("Ability Tasks")));
        VariablesAdded.Add(ClassVarName);

        // 3. Add task-specific configuration based on common task types
        FEdGraphPinType FloatPinType;
        FloatPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        FloatPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;

        if (TaskType == TEXT("WaitDelay") || TaskType == TEXT("Delay"))
        {
            FString DurationVarName = FString::Printf(TEXT("%s_Duration"), *TaskVarPrefix);
            FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*DurationVarName), FloatPinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, FName(*DurationVarName), nullptr, FText::FromString(TEXT("Ability Tasks")));

            // Set default value
            for (FBPVariableDescription& VarDesc : Blueprint->NewVariables)
            {
                if (VarDesc.VarName == FName(*DurationVarName))
                {
                    VarDesc.DefaultValue = TEXT("1.0");
                    break;
                }
            }
            VariablesAdded.Add(DurationVarName);
        }
        else if (TaskType == TEXT("WaitInputPress") || TaskType == TEXT("WaitInputRelease"))
        {
            FString InputActionVarName = FString::Printf(TEXT("%s_InputAction"), *TaskVarPrefix);
            FEdGraphPinType NamePinType;
            NamePinType.PinCategory = UEdGraphSchema_K2::PC_Name;
            FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*InputActionVarName), NamePinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, FName(*InputActionVarName), nullptr, FText::FromString(TEXT("Ability Tasks")));
            VariablesAdded.Add(InputActionVarName);
        }
        else if (TaskType == TEXT("PlayMontageAndWait") || TaskType == TEXT("Montage"))
        {
            // Montage reference
            FEdGraphPinType SoftObjPinType;
            SoftObjPinType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;
            SoftObjPinType.PinSubCategoryObject = UObject::StaticClass();
            FString MontageVarName = FString::Printf(TEXT("%s_Montage"), *TaskVarPrefix);
            FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*MontageVarName), SoftObjPinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, FName(*MontageVarName), nullptr, FText::FromString(TEXT("Ability Tasks")));
            VariablesAdded.Add(MontageVarName);

            // Play rate
            FString RateVarName = FString::Printf(TEXT("%s_PlayRate"), *TaskVarPrefix);
            FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*RateVarName), FloatPinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, FName(*RateVarName), nullptr, FText::FromString(TEXT("Ability Tasks")));
            for (FBPVariableDescription& VarDesc : Blueprint->NewVariables)
            {
                if (VarDesc.VarName == FName(*RateVarName))
                {
                    VarDesc.DefaultValue = TEXT("1.0");
                    break;
                }
            }
            VariablesAdded.Add(RateVarName);
        }
        else if (TaskType == TEXT("WaitTargetData") || TaskType == TEXT("TargetData"))
        {
            // Target data class
            FString TargetActorVarName = FString::Printf(TEXT("%s_TargetActorClass"), *TaskVarPrefix);
            FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*TargetActorVarName), ClassPinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, FName(*TargetActorVarName), nullptr, FText::FromString(TEXT("Ability Tasks")));
            VariablesAdded.Add(TargetActorVarName);
        }
        else if (TaskType == TEXT("WaitGameplayEvent") || TaskType == TEXT("GameplayEvent"))
        {
            // Gameplay tag to wait for
            FEdGraphPinType StructPinType;
            StructPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
            StructPinType.PinSubCategoryObject = FGameplayTag::StaticStruct();
            FString EventTagVarName = FString::Printf(TEXT("%s_EventTag"), *TaskVarPrefix);
            FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*EventTagVarName), StructPinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, FName(*EventTagVarName), nullptr, FText::FromString(TEXT("Ability Tasks")));
            VariablesAdded.Add(EventTagVarName);
        }

        // 4. Add generic task name variable for runtime reference
        FEdGraphPinType NamePinType;
        NamePinType.PinCategory = UEdGraphSchema_K2::PC_Name;
        FString TaskNameVarName = FString::Printf(TEXT("%s_Name"), *TaskVarPrefix);
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*TaskNameVarName), NamePinType);
        FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, FName(*TaskNameVarName), nullptr, FText::FromString(TEXT("Ability Tasks")));

        // Set default task name
        for (FBPVariableDescription& VarDesc : Blueprint->NewVariables)
        {
            if (VarDesc.VarName == FName(*TaskNameVarName))
            {
                VarDesc.DefaultValue = TaskType;
                break;
            }
        }
        VariablesAdded.Add(TaskNameVarName);

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("taskType"), TaskType);
        if (!TaskClassName.IsEmpty())
        {
            Result->SetStringField(TEXT("taskClassName"), TaskClassName);
        }

        TArray<TSharedPtr<FJsonValue>> VarsArray;
        for (const FString& VarName : VariablesAdded)
        {
            VarsArray.Add(MakeShared<FJsonValueString>(VarName));
        }
        Result->SetArrayField(TEXT("variablesAdded"), VarsArray);
        Result->SetNumberField(TEXT("variableCount"), VariablesAdded.Num());

        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Ability task configuration added"), Result);
        return true;
    }

    // set_activation_policy

    return false;
}
}
#endif
