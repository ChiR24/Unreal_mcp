#include "McpAutomationBridge_GASBlueprintCreation.h"
#include "McpAutomationBridge_GASPayloadFields.h"
#include "McpAutomationBridge_GASRequestContext.h"
#include "McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "McpAutomationBridgeHelpersSafeOperationsFacade.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR && MCP_HAS_GAS
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "AttributeSet.h"
#include "Dom/JsonValue.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectExecutionCalculation.h"
#include "Kismet2/BlueprintEditorUtils.h"
#endif

#if WITH_EDITOR && MCP_HAS_GAS
namespace McpGASHandlers
{
bool HandleGASAbilityGrantAndExecution(const FGASRequestContext& Context, const FString& SubAction)
{
    UMcpAutomationBridgeSubsystem* Bridge = Context.Subsystem;
    const FString& RequestId = Context.RequestId;
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket = Context.RequestingSocket;
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    const FString& Name = Context.Name;
    const FString& Path = Context.Path;
    const FString& BlueprintPath = Context.BlueprintPath;
    const FString& AssetPath = Context.AssetPath;

    if (SubAction == TEXT("grant_ability"))
    {
        FString ActorPath = GetStringFieldGAS(Payload, TEXT("actorPath"));
        if (ActorPath.IsEmpty())
        {
            ActorPath = GetStringFieldGAS(Payload, TEXT("blueprintPath"));
        }
        if (ActorPath.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing actorPath or blueprintPath"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString AbilityPath = GetStringFieldGAS(Payload, TEXT("abilityPath"));
        if (AbilityPath.IsEmpty())
        {
            AbilityPath = GetStringFieldGAS(Payload, TEXT("abilityClass"));
        }
        if (AbilityPath.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing abilityPath or abilityClass"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        // Load the actor blueprint
        UBlueprint* ActorBlueprint = LoadObject<UBlueprint>(nullptr, *ActorPath);
        if (!ActorBlueprint)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Actor blueprint not found: %s"), *ActorPath), TEXT("NOT_FOUND"));
            return true;
        }

        // Verify the ability exists
        UBlueprint* AbilityBlueprint = LoadObject<UBlueprint>(nullptr, *AbilityPath);
        UClass* AbilityClass = nullptr;

        if (AbilityBlueprint && AbilityBlueprint->GeneratedClass)
        {
            AbilityClass = AbilityBlueprint->GeneratedClass;
        }
        else
        {
            AbilityClass = LoadClass<UGameplayAbility>(nullptr, *AbilityPath);
        }

        if (!AbilityClass || !AbilityClass->IsChildOf(UGameplayAbility::StaticClass()))
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Invalid ability class: %s"), *AbilityPath), TEXT("INVALID_CLASS"));
            return true;
        }

        // Find ASC on the actor blueprint
        UAbilitySystemComponent* ASC = nullptr;
        bool bHasASC = false;

        if (ActorBlueprint->SimpleConstructionScript)
        {
            for (USCS_Node* Node : ActorBlueprint->SimpleConstructionScript->GetAllNodes())
            {
                if (Node && Node->ComponentTemplate)
                {
                    if (Cast<UAbilitySystemComponent>(Node->ComponentTemplate))
                    {
                        bHasASC = true;
                        break;
                    }
                }
            }
        }

        // Check CDO for native ASC
        if (!bHasASC && ActorBlueprint->GeneratedClass)
        {
            if (AActor* CDO = Cast<AActor>(ActorBlueprint->GeneratedClass->GetDefaultObject()))
            {
                if (CDO->FindComponentByClass<UAbilitySystemComponent>())
                {
                    bHasASC = true;
                }
            }
        }

        if (!bHasASC)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                TEXT("Actor does not have an AbilitySystemComponent"), TEXT("ASC_NOT_FOUND"));
            return true;
        }

        // To grant abilities at design time, we need to add them to the ASC's DefaultAbilitiesGranted
        // or use a custom initialization. For now, we'll add a variable to track granted abilities.

        // Check if GrantedAbilities variable exists
        bool bHasGrantedVar = false;
        for (const FBPVariableDescription& VarDesc : ActorBlueprint->NewVariables)
        {
            if (VarDesc.VarName == TEXT("InitialAbilities"))
            {
                bHasGrantedVar = true;
                break;
            }
        }

        if (!bHasGrantedVar)
        {
            // Add InitialAbilities array variable
            FEdGraphPinType AbilityArrayType;
            AbilityArrayType.PinCategory = UEdGraphSchema_K2::PC_SoftClass;
            AbilityArrayType.PinSubCategoryObject = UGameplayAbility::StaticClass();
            AbilityArrayType.ContainerType = EPinContainerType::Array;

            FBlueprintEditorUtils::AddMemberVariable(ActorBlueprint, TEXT("InitialAbilities"), AbilityArrayType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(ActorBlueprint, TEXT("InitialAbilities"), nullptr,
                FText::FromString(TEXT("GAS")));
        }

        int32 AbilityLevel = static_cast<int32>(GetNumberFieldGAS(Payload, TEXT("abilityLevel"), 1.0));
        int32 InputID = static_cast<int32>(GetNumberFieldGAS(Payload, TEXT("inputID"), -1.0));

        FBlueprintEditorUtils::MarkBlueprintAsModified(ActorBlueprint);
        McpSafeAssetSave(ActorBlueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("actorPath"), ActorPath);
        Result->SetStringField(TEXT("abilityClass"), AbilityClass->GetName());
        Result->SetNumberField(TEXT("abilityLevel"), AbilityLevel);
        Result->SetNumberField(TEXT("inputID"), InputID);
        Result->SetBoolField(TEXT("hasASC"), bHasASC);
        Result->SetBoolField(TEXT("createdInitialAbilitiesVar"), !bHasGrantedVar);
        Result->SetStringField(TEXT("note"), TEXT("Add ability to InitialAbilities array. Call GiveAbility on ASC in BeginPlay to grant."));

        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Ability grant configured"), Result);
        return true;
    }

    // ============================================================
    // 13.7 EXECUTION CALCULATIONS
    // ============================================================

    // create_execution_calculation - Create UGameplayEffectExecutionCalculation blueprint
    if (SubAction == TEXT("create_execution_calculation"))
    {
        if (Name.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing name."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString Error;
        bool bReusedExisting = false;
        UBlueprint* Blueprint = CreateGASBlueprint(Path, Name, UGameplayEffectExecutionCalculation::StaticClass(), Error, bReusedExisting);
        if (!Blueprint)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, Error, TEXT("CREATION_FAILED"));
            return true;
        }

        // Add common execution calculation variables for configuration (only for new blueprints)
        if (!bReusedExisting)
        {
            // 1. RelevantAttributesToCapture - array of FGameplayAttribute references for captured attributes
            FEdGraphPinType StructArrayType;
            StructArrayType.PinCategory = UEdGraphSchema_K2::PC_Struct;
            StructArrayType.PinSubCategoryObject = FGameplayAttribute::StaticStruct();
            StructArrayType.ContainerType = EPinContainerType::Array;

            FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("CapturedSourceAttributes"), StructArrayType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("CapturedSourceAttributes"), nullptr,
                FText::FromString(TEXT("Execution Calculation")));

            FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("CapturedTargetAttributes"), StructArrayType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("CapturedTargetAttributes"), nullptr,
                FText::FromString(TEXT("Execution Calculation")));

            // 2. bRequiresPassedInTags - whether the calculation needs gameplay tags passed in
            FEdGraphPinType BoolPinType;
            BoolPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
            FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("bRequiresPassedInTags"), BoolPinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("bRequiresPassedInTags"), nullptr,
                FText::FromString(TEXT("Execution Calculation")));

            // 3. CalculationDescription - human readable description
            FEdGraphPinType StringPinType;
            StringPinType.PinCategory = UEdGraphSchema_K2::PC_String;
            FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("CalculationDescription"), StringPinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("CalculationDescription"), nullptr,
                FText::FromString(TEXT("Execution Calculation")));

            // 4. OutputModifiers - array to configure output modifier attributes
            FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("OutputModifierAttributes"), StructArrayType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("OutputModifierAttributes"), nullptr,
                FText::FromString(TEXT("Execution Calculation")));


            FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
            McpSafeCompileBlueprint(Blueprint);
            McpSafeAssetSave(Blueprint);
        }

        // Use the actual blueprint name (which may have been sanitized) in the response
        FString ActualName = Blueprint->GetName();
        FString ActualPath = Path / ActualName;

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("assetPath"), ActualPath);
        Result->SetStringField(TEXT("name"), ActualName);
        Result->SetStringField(TEXT("parentClass"), TEXT("GameplayEffectExecutionCalculation"));
        Result->SetBoolField(TEXT("reusedExisting"), bReusedExisting);

        TArray<TSharedPtr<FJsonValue>> VariablesArray;
        VariablesArray.Add(MakeShared<FJsonValueString>(TEXT("CapturedSourceAttributes")));
        VariablesArray.Add(MakeShared<FJsonValueString>(TEXT("CapturedTargetAttributes")));
        VariablesArray.Add(MakeShared<FJsonValueString>(TEXT("bRequiresPassedInTags")));
        VariablesArray.Add(MakeShared<FJsonValueString>(TEXT("CalculationDescription")));
        VariablesArray.Add(MakeShared<FJsonValueString>(TEXT("OutputModifierAttributes")));
        Result->SetArrayField(TEXT("variablesAdded"), VariablesArray);

        Result->SetStringField(TEXT("note"), TEXT("Override Execute_Implementation in Blueprint to implement custom calculation logic. Use CapturedSourceAttributes and CapturedTargetAttributes to define which attributes to capture."));

        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true,
            bReusedExisting ? TEXT("Execution calculation already exists") : TEXT("Execution calculation created"), Result);
        return true;
    }

    return false;
}
}
#endif
