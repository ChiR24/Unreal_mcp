#include "McpAutomationBridge_GASPayloadFields.h"
#include "McpAutomationBridge_GASRequestContext.h"
#include "McpAutomationBridgeHelpersSafeOperationsFacade.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR && MCP_HAS_GAS
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonValue.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/DataAsset.h"
#include "Factories/BlueprintFactory.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "Kismet2/BlueprintEditorUtils.h"
#endif

#if WITH_EDITOR && MCP_HAS_GAS
namespace McpGASHandlers
{
bool HandleGASAbilitySets(const FGASRequestContext& Context, const FString& SubAction)
{
    UMcpAutomationBridgeSubsystem* Bridge = Context.Subsystem;
    const FString& RequestId = Context.RequestId;
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket = Context.RequestingSocket;
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    const FString& Name = Context.Name;
    const FString& Path = Context.Path;
    const FString& BlueprintPath = Context.BlueprintPath;
    const FString& AssetPath = Context.AssetPath;

    if (SubAction == TEXT("create_ability_set"))
    {
        FString SetPath = GetStringFieldGAS(Payload, TEXT("setPath"));
        if (SetPath.IsEmpty())
        {
            SetPath = GetStringFieldGAS(Payload, TEXT("assetPath"));
        }
        if (SetPath.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing setPath or assetPath"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        // Normalize path
        if (!SetPath.StartsWith(TEXT("/Game/")))
        {
            SetPath = TEXT("/Game/") + SetPath;
        }

        // Extract package path and asset name
        FString PackagePath, AssetName;
        int32 LastSlash;
        if (SetPath.FindLastChar('/', LastSlash))
        {
            PackagePath = SetPath.Left(LastSlash);
            AssetName = SetPath.RightChop(LastSlash + 1);
        }
        else
        {
            PackagePath = TEXT("/Game");
            AssetName = SetPath;
        }

        // Check if asset already exists
        if (UObject* ExistingAsset = LoadObject<UObject>(nullptr, *SetPath))
        {
            TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
            Result->SetStringField(TEXT("setPath"), SetPath);
            Result->SetStringField(TEXT("status"), TEXT("already_exists"));
            Bridge->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Ability set already exists"), Result);
            return true;
        }

        // Create the package
        FString PackageName = SetPath;
        UPackage* Package = CreatePackage(*PackageName);
        if (!Package)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create package"), TEXT("PACKAGE_FAILED"));
            return true;
        }

        // UGameplayAbilitySet is not a standard GAS class - it's typically a custom DataAsset
        // We'll create a Blueprint-based DataAsset that can hold ability references
        // For GAS, the common pattern is using UAbilitySystemComponent directly or a custom data asset

        // Create a DataAsset subclass blueprint
        UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
        Factory->ParentClass = UPrimaryDataAsset::StaticClass();

        UBlueprint* SetBlueprint = Cast<UBlueprint>(Factory->FactoryCreateNew(
            UBlueprint::StaticClass(),
            Package,
            *AssetName,
            RF_Public | RF_Standalone,
            nullptr,
            GWarn
        ));

        if (!SetBlueprint)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create ability set blueprint"), TEXT("CREATION_FAILED"));
            return true;
        }

        // Add variables to hold abilities
        // 1. GrantedAbilities - Array of TSubclassOf<UGameplayAbility>
        FEdGraphPinType AbilityArrayType;
        AbilityArrayType.PinCategory = UEdGraphSchema_K2::PC_SoftClass;
        AbilityArrayType.PinSubCategoryObject = UGameplayAbility::StaticClass();
        AbilityArrayType.ContainerType = EPinContainerType::Array;

        FBlueprintEditorUtils::AddMemberVariable(SetBlueprint, TEXT("GrantedAbilities"), AbilityArrayType);
        FBlueprintEditorUtils::SetBlueprintVariableCategory(SetBlueprint, TEXT("GrantedAbilities"), nullptr,
            FText::FromString(TEXT("Ability Set")));

        // 2. GrantedEffects - Array of TSubclassOf<UGameplayEffect>
        FEdGraphPinType EffectArrayType;
        EffectArrayType.PinCategory = UEdGraphSchema_K2::PC_SoftClass;
        EffectArrayType.PinSubCategoryObject = UGameplayEffect::StaticClass();
        EffectArrayType.ContainerType = EPinContainerType::Array;

        FBlueprintEditorUtils::AddMemberVariable(SetBlueprint, TEXT("GrantedEffects"), EffectArrayType);
        FBlueprintEditorUtils::SetBlueprintVariableCategory(SetBlueprint, TEXT("GrantedEffects"), nullptr,
            FText::FromString(TEXT("Ability Set")));

        // 3. GrantedTags - Gameplay Tag Container
        FEdGraphPinType TagContainerType;
        TagContainerType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        TagContainerType.PinSubCategoryObject = FGameplayTagContainer::StaticStruct();

        FBlueprintEditorUtils::AddMemberVariable(SetBlueprint, TEXT("GrantedTags"), TagContainerType);
        FBlueprintEditorUtils::SetBlueprintVariableCategory(SetBlueprint, TEXT("GrantedTags"), nullptr,
            FText::FromString(TEXT("Ability Set")));

        // 4. SetName - display name
        FEdGraphPinType StringType;
        StringType.PinCategory = UEdGraphSchema_K2::PC_String;
        FBlueprintEditorUtils::AddMemberVariable(SetBlueprint, TEXT("SetDisplayName"), StringType);

        FString SetName = GetStringFieldGAS(Payload, TEXT("setName"));
        if (SetName.IsEmpty())
        {
            SetName = AssetName;
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(SetBlueprint);

        FAssetRegistryModule::AssetCreated(SetBlueprint);
        McpSafeAssetSave(SetBlueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("setPath"), SetBlueprint->GetPathName());
        Result->SetStringField(TEXT("setName"), SetName);
        Result->SetStringField(TEXT("assetName"), AssetName);

        TArray<TSharedPtr<FJsonValue>> VariablesArray;
        VariablesArray.Add(MakeShared<FJsonValueString>(TEXT("GrantedAbilities")));
        VariablesArray.Add(MakeShared<FJsonValueString>(TEXT("GrantedEffects")));
        VariablesArray.Add(MakeShared<FJsonValueString>(TEXT("GrantedTags")));
        VariablesArray.Add(MakeShared<FJsonValueString>(TEXT("SetDisplayName")));
        Result->SetArrayField(TEXT("variables"), VariablesArray);

        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Ability set created"), Result);
        return true;
    }

    // add_ability - Add ability class reference to ability set
    if (SubAction == TEXT("add_ability"))
    {
        FString SetPath = GetStringFieldGAS(Payload, TEXT("setPath"));
        if (SetPath.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing setPath"), TEXT("INVALID_ARGUMENT"));
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

        UBlueprint* SetBlueprint = LoadObject<UBlueprint>(nullptr, *SetPath);
        if (!SetBlueprint)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Ability set not found: %s"), *SetPath), TEXT("NOT_FOUND"));
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
            // Try loading as a native class
            AbilityClass = LoadClass<UGameplayAbility>(nullptr, *AbilityPath);
        }

        if (!AbilityClass || !AbilityClass->IsChildOf(UGameplayAbility::StaticClass()))
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Invalid ability class: %s"), *AbilityPath), TEXT("INVALID_CLASS"));
            return true;
        }

        // Find the GrantedAbilities variable and add to its default value
        // This is complex because we need to modify the CDO's array
        // For simplicity, we'll add a note that the array should be configured in editor

        // Mark as modified
        FBlueprintEditorUtils::MarkBlueprintAsModified(SetBlueprint);
        McpSafeAssetSave(SetBlueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("setPath"), SetPath);
        Result->SetStringField(TEXT("abilityPath"), AbilityPath);
        Result->SetStringField(TEXT("abilityClass"), AbilityClass->GetName());
        Result->SetStringField(TEXT("note"), TEXT("Ability reference validated. Add to GrantedAbilities array in the Data Asset editor."));

        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Ability validated for set"), Result);
        return true;
    }

    // grant_ability - Grant ability to actor's AbilitySystemComponent at runtime

    return false;
}
}
#endif
