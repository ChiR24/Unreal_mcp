// Copyright (c) 2024 EngineWire Contributors
#include "Domains/ControlActor/Materials/McpMaterialBuild.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Factories/MaterialFactoryNew.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/PackageName.h"
#include "Safety/McpSafeOperations.h"
#include "UObject/Package.h"

UMaterialInterface* McpFindMaterialAsset(const TCHAR* Folder, const FString& Name)
{
    const FString PackagePath = FString(Folder) + Name;
    const FString ObjectPath = PackagePath + TEXT(".") + Name;
    if (UMaterialInterface* Found = FindObject<UMaterialInterface>(nullptr, *ObjectPath))
    {
        return Found;
    }
    return FPackageName::DoesPackageExist(PackagePath) ? LoadObject<UMaterialInterface>(nullptr, *ObjectPath) : nullptr;
}

UMaterial* McpNewMaterial(const TCHAR* Folder, const FString& Name)
{
    UPackage* Package = CreatePackage(*(FString(Folder) + Name));
    return Package ? Cast<UMaterial>(NewObject<UMaterialFactoryNew>()->FactoryCreateNew(
                         UMaterial::StaticClass(), Package, FName(*Name), RF_Public | RF_Standalone, nullptr, GWarn))
                   : nullptr;
}

UMaterialInstanceConstant* McpNewMaterialInstance(UMaterialInterface* Parent, const TCHAR* Folder, const FString& Name)
{
    UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
    Factory->InitialParent = Parent;
    UPackage* Package = CreatePackage(*(FString(Folder) + Name));
    return Package ? Cast<UMaterialInstanceConstant>(Factory->FactoryCreateNew(UMaterialInstanceConstant::StaticClass(),
                         Package, FName(*Name), RF_Public | RF_Standalone, nullptr, GWarn))
                   : nullptr;
}

void McpSaveNewMaterial(UMaterialInterface* Material)
{
    Material->PostEditChange();
    Material->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(Material);
    McpSafeAssetSave(Material);
}
