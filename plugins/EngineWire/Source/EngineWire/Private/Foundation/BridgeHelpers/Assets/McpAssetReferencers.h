#pragma once

#include "CoreMinimal.h"

#include "AssetRegistry/AssetRegistryModule.h"

// The packages that reference PackageName and are not themselves being deleted: DeleteSet holds the
// package names of the delete, and a folder in it covers everything under it. Deleting an asset these
// still use breaks them, so delete keeps it (unless force) and bulk_delete keeps it.
inline TArray<FString> McpOutsideReferencers(const FString& PackageName, const TArray<FString>& DeleteSet)
{
    TArray<FAssetIdentifier> Refs;
    FAssetRegistryModule::GetRegistry().GetReferencers(FAssetIdentifier(FName(*PackageName)), Refs,
                                                       UE::AssetRegistry::EDependencyCategory::Package);
    TArray<FString> Outside;
    for (const FAssetIdentifier& Ref : Refs)
    {
        const FString RefPackage = Ref.PackageName.ToString();
        const bool bDeletedToo = DeleteSet.ContainsByPredicate([&RefPackage](const FString& D)
            { return RefPackage == D || RefPackage.StartsWith(D + TEXT("/")); });
        if (!RefPackage.IsEmpty() && !bDeletedToo) Outside.AddUnique(RefPackage);
    }
    return Outside;
}

// Takes out of DeleteSet every package something outside it still references, handing each to Kept(Index, Users)
// first. Until nothing changes: an asset used only by one this keeps is outside-referenced in turn, and one pass
// deleted it and left the kept asset pointing at nothing.
template <typename TKept>
void McpKeepOutsideReferenced(TArray<FString>& DeleteSet, TKept&& Kept)
{
    for (bool bKeptOne = true; bKeptOne;)
    {
        bKeptOne = false;
        for (int32 Index = DeleteSet.Num() - 1; Index >= 0; --Index)
        {
            TArray<FString> Users = McpOutsideReferencers(DeleteSet[Index], DeleteSet);
            if (Users.Num() > 0)
            {
                Kept(Index, MoveTemp(Users));
                DeleteSet.RemoveAt(Index);
                bKeptOne = true;
            }
        }
    }
}
