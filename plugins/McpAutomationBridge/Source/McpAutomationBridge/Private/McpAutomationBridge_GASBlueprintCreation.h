#pragma once

#include "McpAutomationBridge_GASAssetValidation.h"

#if WITH_EDITOR && MCP_HAS_GAS
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "Factories/BlueprintFactory.h"
#include "UObject/Package.h"

namespace McpGASHandlers
{
static inline UBlueprint* CreateGASBlueprint(
    const FString& Path,
    const FString& Name,
    UClass* ParentClass,
    FString& OutError,
    bool& bOutReusedExisting)
{
    bOutReusedExisting = false;
    if (!ParentClass)
    {
        OutError = TEXT("Invalid parent class");
        return nullptr;
    }

    FString PackageName;
    FString PathError;
    const FString SanitizedName = SanitizeAssetName(Name);
    if (!ValidateAssetCreationPath(Path, SanitizedName, PackageName, PathError))
    {
        OutError = PathError;
        return nullptr;
    }
    if (!IsValidAssetPath(PackageName))
    {
        OutError = FString::Printf(TEXT("Invalid asset path: %s"), *PackageName);
        return nullptr;
    }

    const FString FullAssetPath = PackageName + TEXT(".") + SanitizedName;
    if (UEditorAssetLibrary::DoesAssetExist(FullAssetPath))
    {
        UObject* ExistingAsset = UEditorAssetLibrary::LoadAsset(FullAssetPath);
        if (!ExistingAsset)
        {
            OutError = FString::Printf(TEXT("Failed to load existing asset: %s"), *FullAssetPath);
            return nullptr;
        }

        UBlueprint* ExistingBlueprint = Cast<UBlueprint>(ExistingAsset);
        if (!ExistingBlueprint)
        {
            OutError = FString::Printf(TEXT("Asset already exists and is not a Blueprint: %s"), *FullAssetPath);
            return nullptr;
        }

        UClass* ExistingParentClass = ExistingBlueprint->ParentClass;
        if (!ExistingParentClass && ExistingBlueprint->GeneratedClass)
        {
            ExistingParentClass = ExistingBlueprint->GeneratedClass->GetSuperClass();
        }
        if (ExistingParentClass && !ExistingParentClass->IsChildOf(ParentClass))
        {
            OutError = FString::Printf(TEXT("Blueprint already exists with incompatible parent class: %s"), *FullAssetPath);
            return nullptr;
        }

        bOutReusedExisting = true;
        return ExistingBlueprint;
    }

    UPackage* Package = CreatePackage(*PackageName);
    if (!Package)
    {
        OutError = FString::Printf(TEXT("Failed to create package: %s"), *PackageName);
        return nullptr;
    }

    UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
    Factory->ParentClass = ParentClass;
    UBlueprint* Blueprint = Cast<UBlueprint>(
        Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, FName(*SanitizedName),
                                  RF_Public | RF_Standalone, nullptr, GWarn));
    if (!Blueprint)
    {
        OutError = TEXT("Failed to create blueprint");
        return nullptr;
    }

    FAssetRegistryModule::AssetCreated(Blueprint);
    Blueprint->MarkPackageDirty();
    return Blueprint;
}
}
#endif
