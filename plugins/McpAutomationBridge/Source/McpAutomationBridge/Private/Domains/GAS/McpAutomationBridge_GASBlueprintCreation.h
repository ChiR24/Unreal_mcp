#pragma once

#include "Domains/GAS/McpAutomationBridge_GASAssetValidation.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintPaths.h"
#include "Domains/GAS/McpAutomationBridge_GASRequestContext.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Safety/McpSafeOperations.h"

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
    if (McpAssetExists(FullAssetPath))
    {
        UObject* ExistingAsset = McpLoadAsset(FullAssetPath);
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

// create_<GAS asset>: needs a name; creates (or reuses) the Blueprint under ParentClass; a NEW one gets
// ConfigureNew and is saved. Returns {name, assetPath, parentClass, reusedExisting} + verification for the
// caller to extend and send, or null after replying with the refusal itself.
template <typename TConfigure>
TSharedPtr<FJsonObject> CreateGASAsset(const FGASRequestContext& Context, UClass* ParentClass, const TCHAR* ParentLabel,
                                       bool& bOutReused, TConfigure&& ConfigureNew)
{
    if (Context.Name.IsEmpty())
    {
        Context.Subsystem->SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Missing name."), TEXT("INVALID_ARGUMENT"));
        return nullptr;
    }
    FString Error;
    UBlueprint* Blueprint = CreateGASBlueprint(Context.Path, Context.Name, ParentClass, Error, bOutReused);
    if (!Blueprint)
    {
        Context.Subsystem->SendAutomationError(Context.RequestingSocket, Context.RequestId, Error, TEXT("CREATION_FAILED"));
        return nullptr;
    }
    if (!bOutReused)
    {
        ConfigureNew(Blueprint);
        McpSafeOperations::McpSafeAssetSave(Blueprint);
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    // The actual (possibly sanitized) name; verification sets assetPath to the package.
    Result->SetStringField(TEXT("name"), Blueprint->GetName());
    Result->SetStringField(TEXT("parentClass"), ParentLabel);
    Result->SetBoolField(TEXT("reusedExisting"), bOutReused);
    McpHandlerUtils::AddVerification(Result, Blueprint);
    return Result;
}
}
