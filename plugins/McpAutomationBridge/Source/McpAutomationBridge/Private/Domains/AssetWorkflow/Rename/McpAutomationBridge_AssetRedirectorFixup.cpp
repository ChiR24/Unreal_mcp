// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "Domains/AssetWorkflow/Rename/McpAutomationBridge_AssetRenameGuard.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "ISourceControlModule.h"
#include "ObjectTools.h"
#include "SourceControlHelpers.h"
#include "UObject/ObjectRedirector.h"

namespace McpAssetRename
{
void FixupRedirectorsIn(const FString& Folder, int32& OutFound, int32& OutFixed, bool bCheckoutFiles)
{
    FARFilter Filter;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/CoreUObject"), TEXT("ObjectRedirector")));
#else
    Filter.ClassNames.Add(FName(TEXT("ObjectRedirector")));
#endif
    Filter.PackagePaths.Add(FName(*Folder));
    Filter.bRecursivePaths = true;
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    TArray<FAssetData> Found;
    Registry.GetAssets(Filter, Found);
    OutFound = Found.Num();
    OutFixed = 0;
    TArray<UObjectRedirector*> Redirectors;
    for (const FAssetData& Asset : Found)
    {
        if (UObjectRedirector* Redirector = Cast<UObjectRedirector>(Asset.GetAsset()))
        {
            Redirectors.Add(Redirector);
        }
    }
    if (Redirectors.Num() == 0)
    {
        return;
    }
    if (bCheckoutFiles && ISourceControlModule::Get().IsEnabled())
    {
        TArray<FString> PackageNames;
        for (const FAssetData& Asset : Found)
        {
            PackageNames.Add(Asset.PackageName.ToString());
        }
        SourceControlHelpers::CheckOutFiles(PackageNames, true);
    }
    // FixupReferencers deletes every redirector it could fix; whatever it left is deleted now that
    // nothing points at it any more.
    FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().FixupReferencers(Redirectors);
    TArray<FAssetData> Left;
    Registry.GetAssets(Filter, Left);
    TArray<UObject*> ToDelete;
    for (const FAssetData& Asset : Left)
    {
        if (UObject* Object = Asset.GetAsset())
        {
            ToDelete.Add(Object);
        }
    }
    const int32 Deleted = ToDelete.Num() > 0 ? ObjectTools::DeleteObjects(ToDelete, false) : 0;
    OutFixed = OutFound - (Left.Num() - Deleted);
}
} // namespace McpAssetRename
