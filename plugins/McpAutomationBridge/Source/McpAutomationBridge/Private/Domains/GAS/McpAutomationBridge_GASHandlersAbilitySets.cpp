#include "Domains/GAS/McpAutomationBridge_GASAbilitySetArrays.h"
#include "Domains/GAS/McpAutomationBridge_GASEffectClassResolution.h"
#include "Domains/GAS/McpAutomationBridge_GASPayloadFields.h"
#include "Domains/GAS/McpAutomationBridge_GASRequestContext.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersSafeOperationsFacade.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

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
        FString SetPath = GetJsonStringField(Payload, TEXT("setPath"));
        if (SetPath.IsEmpty())
        {
            SetPath = GetJsonStringField(Payload, TEXT("assetPath"));
        }
        if (SetPath.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing setPath or assetPath"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        if (!SetPath.StartsWith(TEXT("/Game/")))
        {
            SetPath = TEXT("/Game/") + SetPath;
        }

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

        if (UObject* ExistingAsset = LoadObject<UObject>(nullptr, *SetPath))
        {
            TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
            Result->SetStringField(TEXT("setPath"), SetPath);
            Result->SetStringField(TEXT("status"), TEXT("already_exists"));
            Bridge->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Ability set already exists"), Result);
            return true;
        }

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

        FString SetName = GetJsonStringField(Payload, TEXT("setName"));
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

    if (SubAction == TEXT("add_ability"))
    {
        FString SetPath = GetJsonStringField(Payload, TEXT("setPath"));
        if (SetPath.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing setPath"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString AbilityPath = GetJsonStringField(Payload, TEXT("abilityPath"));
        if (AbilityPath.IsEmpty())
        {
            AbilityPath = GetJsonStringField(Payload, TEXT("abilityClass"));
        }
        if (AbilityPath.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing abilityPath or abilityClass"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        // The set is either the DataAsset Blueprint create_ability_set makes
        // (abilities live on its CDO) or a DataAsset instance of a native set
        // class (abilities live on the asset itself).
        UObject* SetAsset = LoadObject<UObject>(nullptr, *SetPath);
        if (!SetAsset)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Ability set not found: %s"), *SetPath), TEXT("NOT_FOUND"));
            return true;
        }

        UClass* AbilityClass = ResolveClassFromAssetOrScriptPath(AbilityPath, UGameplayAbility::StaticClass());
        if (!AbilityClass)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Invalid ability class: %s"), *AbilityPath), TEXT("INVALID_CLASS"));
            return true;
        }

        UBlueprint* SetBlueprint = Cast<UBlueprint>(SetAsset);
        UObject* Container = SetAsset;
        UStruct* ContainerType = SetAsset->GetClass();
        if (SetBlueprint)
        {
            if (!SetBlueprint->GeneratedClass)
            {
                McpSafeCompileBlueprint(SetBlueprint);
            }
            if (!SetBlueprint->GeneratedClass)
            {
                Bridge->SendAutomationError(RequestingSocket, RequestId,
                    FString::Printf(TEXT("Ability set Blueprint has no generated class: %s"), *SetPath), TEXT("COMPILE_FAILED"));
                return true;
            }
            Container = SetBlueprint->GeneratedClass->GetDefaultObject();
            ContainerType = SetBlueprint->GeneratedClass;
        }

        TArray<FString> ArrayNames;
        FGASAbilityArrayTarget Target = FindAbilityArrayTarget(ContainerType, AbilityClass, ArrayNames);
        // A freshly authored set may not have been compiled since its
        // GrantedAbilities variable was added; compile once and look again.
        if (!Target.IsValid() && SetBlueprint && McpSafeCompileBlueprint(SetBlueprint) && SetBlueprint->GeneratedClass)
        {
            Container = SetBlueprint->GeneratedClass->GetDefaultObject();
            ContainerType = SetBlueprint->GeneratedClass;
            ArrayNames.Reset();
            Target = FindAbilityArrayTarget(ContainerType, AbilityClass, ArrayNames);
        }
        if (!Target.IsValid() || !Container)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("No class array on %s accepts %s. Array properties: [%s]"),
                    *ContainerType->GetName(), *AbilityClass->GetName(),
                    ArrayNames.Num() > 0 ? *FString::Join(ArrayNames, TEXT(", ")) : TEXT("none")),
                TEXT("PROPERTY_NOT_FOUND"));
            return true;
        }

        bool bAlreadyPresent = false;
        const int32 AbilityCount = AppendAbilityClass(Container, Target, AbilityClass, bAlreadyPresent);
        if (SetBlueprint)
        {
            FBlueprintEditorUtils::MarkBlueprintAsModified(SetBlueprint);
        }
        SetAsset->MarkPackageDirty();
        const bool bSaved = McpSafeAssetSave(SetAsset);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("setPath"), SetPath);
        Result->SetStringField(TEXT("abilityPath"), AbilityPath);
        Result->SetStringField(TEXT("abilityClass"), AbilityClass->GetName());
        Result->SetStringField(TEXT("abilityClassPath"), AbilityClass->GetPathName());
        Result->SetStringField(TEXT("propertyName"), Target.Describe());
        Result->SetNumberField(TEXT("abilityCount"), AbilityCount);
        Result->SetBoolField(TEXT("added"), !bAlreadyPresent);
        Result->SetBoolField(TEXT("alreadyPresent"), bAlreadyPresent);
        Result->SetBoolField(TEXT("saved"), bSaved);

        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true,
            bAlreadyPresent ? TEXT("Ability already present in set") : TEXT("Ability added to set"), Result);
        return true;
    }

    return false;
}
}
#endif
