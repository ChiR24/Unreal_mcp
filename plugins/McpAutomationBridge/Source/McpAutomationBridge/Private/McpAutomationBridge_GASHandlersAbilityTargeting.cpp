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
#include "Kismet2/BlueprintEditorUtils.h"
#endif

#if WITH_EDITOR && MCP_HAS_GAS
namespace McpGASHandlers
{
bool HandleGASAbilityTargeting(const FGASRequestContext& Context, const FString& SubAction)
{
    UMcpAutomationBridgeSubsystem* Bridge = Context.Subsystem;
    const FString& RequestId = Context.RequestId;
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket = Context.RequestingSocket;
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    const FString& Name = Context.Name;
    const FString& Path = Context.Path;
    const FString& BlueprintPath = Context.BlueprintPath;
    const FString& AssetPath = Context.AssetPath;

    if (SubAction == TEXT("set_ability_targeting"))
    {
        if (BlueprintPath.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString TargetingType = GetGASStringFieldWithFallback(Payload, TEXT("targetingType"), TEXT("targetingMode"), TEXT("self"));
        float TargetingRange = static_cast<float>(GetGASNumberFieldWithFallback(Payload, TEXT("targetingRange"), TEXT("targetRange"), 1000.0));
        float AOERadius = static_cast<float>(GetNumberFieldGAS(Payload, TEXT("aoeRadius"), 0.0));
        bool bRequiresLineOfSight = GetBoolFieldGAS(Payload, TEXT("requiresLineOfSight"), false);
        float TargetingAngle = static_cast<float>(GetNumberFieldGAS(Payload, TEXT("targetingAngle"), 360.0));

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

        // Add targeting configuration variables based on targeting type

        // 1. Targeting Type enum-like variable (stored as Name for flexibility)
        FEdGraphPinType NamePinType;
        NamePinType.PinCategory = UEdGraphSchema_K2::PC_Name;
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("TargetingType"), NamePinType);
        FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("TargetingType"), nullptr, FText::FromString(TEXT("Targeting")));

        // Set the default value
        for (FBPVariableDescription& VarDesc : Blueprint->NewVariables)
        {
            if (VarDesc.VarName == TEXT("TargetingType"))
            {
                VarDesc.DefaultValue = TargetingType;
                break;
            }
        }

        // 2. Targeting Range
        FEdGraphPinType FloatPinType;
        FloatPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        FloatPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("TargetingRange"), FloatPinType);
        FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("TargetingRange"), nullptr, FText::FromString(TEXT("Targeting")));

        for (FBPVariableDescription& VarDesc : Blueprint->NewVariables)
        {
            if (VarDesc.VarName == TEXT("TargetingRange"))
            {
                VarDesc.DefaultValue = FString::SanitizeFloat(TargetingRange);
                break;
            }
        }

        // 3. Line of Sight requirement
        FEdGraphPinType BoolPinType;
        BoolPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("bRequiresLineOfSight"), BoolPinType);
        FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("bRequiresLineOfSight"), nullptr, FText::FromString(TEXT("Targeting")));

        for (FBPVariableDescription& VarDesc : Blueprint->NewVariables)
        {
            if (VarDesc.VarName == TEXT("bRequiresLineOfSight"))
            {
                VarDesc.DefaultValue = bRequiresLineOfSight ? TEXT("true") : TEXT("false");
                break;
            }
        }

        // 4. Targeting Angle (for cone-based targeting)
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("TargetingAngle"), FloatPinType);
        FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("TargetingAngle"), nullptr, FText::FromString(TEXT("Targeting")));

        for (FBPVariableDescription& VarDesc : Blueprint->NewVariables)
        {
            if (VarDesc.VarName == TEXT("TargetingAngle"))
            {
                VarDesc.DefaultValue = FString::SanitizeFloat(TargetingAngle);
                break;
            }
        }

        if (AOERadius > 0.0f)
        {
            FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("AOERadius"), FloatPinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("AOERadius"), nullptr, FText::FromString(TEXT("Targeting")));
            for (FBPVariableDescription& VarDesc : Blueprint->NewVariables)
            {
                if (VarDesc.VarName == TEXT("AOERadius"))
                {
                    VarDesc.DefaultValue = FString::SanitizeFloat(AOERadius);
                    break;
                }
            }
        }

        // 5. Add target actor variable for runtime use
        FEdGraphPinType ActorPinType;
        ActorPinType.PinCategory = UEdGraphSchema_K2::PC_Object;
        ActorPinType.PinSubCategoryObject = AActor::StaticClass();
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("TargetActor"), ActorPinType);
        FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("TargetActor"), nullptr, FText::FromString(TEXT("Targeting")));

        // 6. Add target location variable for ground/point targeting
        FEdGraphPinType VectorPinType;
        VectorPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        VectorPinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("TargetLocation"), VectorPinType);
        FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("TargetLocation"), nullptr, FText::FromString(TEXT("Targeting")));

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("targetingType"), TargetingType);
        Result->SetNumberField(TEXT("targetingRange"), TargetingRange);
        Result->SetNumberField(TEXT("aoeRadius"), AOERadius);
        Result->SetBoolField(TEXT("requiresLineOfSight"), bRequiresLineOfSight);
        Result->SetNumberField(TEXT("targetingAngle"), TargetingAngle);

        TArray<TSharedPtr<FJsonValue>> VariablesArray;
        VariablesArray.Add(MakeShared<FJsonValueString>(TEXT("TargetingType")));
        VariablesArray.Add(MakeShared<FJsonValueString>(TEXT("TargetingRange")));
        VariablesArray.Add(MakeShared<FJsonValueString>(TEXT("bRequiresLineOfSight")));
        VariablesArray.Add(MakeShared<FJsonValueString>(TEXT("TargetingAngle")));
        if (AOERadius > 0.0f)
        {
            VariablesArray.Add(MakeShared<FJsonValueString>(TEXT("AOERadius")));
        }
        VariablesArray.Add(MakeShared<FJsonValueString>(TEXT("TargetActor")));
        VariablesArray.Add(MakeShared<FJsonValueString>(TEXT("TargetLocation")));
        Result->SetArrayField(TEXT("variablesAdded"), VariablesArray);

        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Targeting configuration complete"), Result);
        return true;
    }

    // add_ability_task - REAL IMPLEMENTATION with AbilityTask class reference and configuration

    return false;
}
}
#endif
