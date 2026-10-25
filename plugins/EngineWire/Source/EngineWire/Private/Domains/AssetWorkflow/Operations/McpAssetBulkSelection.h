// Copyright (c) 2024 EngineWire Contributors

#pragma once

#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/BridgeHelpers/Security/McpAssetPathCanonical.h"

#include "AssetRegistry/AssetRegistryModule.h"

// bulk_delete / bulk_rename act on the explicit assetPaths, else on every asset under folderPath.
// Replies and returns false when neither is usable.
inline bool McpCollectBulkAssetPaths(UEngineWireSubsystem& Bridge, const FString& RequestId,
                                     TSharedPtr<FMcpBridgeWebSocket> Socket, const TSharedPtr<FJsonObject>& Payload,
                                     TArray<FString>& OutPaths)
{
    const TArray<TSharedPtr<FJsonValue>>* Explicit = nullptr;
    if (Payload->TryGetArrayField(TEXT("assetPaths"), Explicit) && Explicit->Num() > 0)
    {
        for (const TSharedPtr<FJsonValue>& Value : *Explicit)
        {
            if (Value.IsValid() && Value->Type == EJson::String) OutPaths.Add(Value->AsString());
        }
        return true;
    }
    const FString FolderPath = GetJsonStringField(Payload, TEXT("folderPath"));
    if (FolderPath.IsEmpty())
    {
        Bridge.SendAutomationError(Socket, RequestId, TEXT("Either assetPaths array or folderPath is required"), TEXT("INVALID_ARGUMENT"));
        return false;
    }
    FString Folder = FolderPath;
    McpAssetPathCanonical::MapContentRootInline(Folder);
    Folder = SanitizeProjectRelativePath(Folder);
    if (Folder.IsEmpty())
    {
        Bridge.SendAutomationError(Socket, RequestId, McpPathRefusalMessage(TEXT("folderPath"), FolderPath), TEXT("SECURITY_VIOLATION"));
        return false;
    }
    // Cached registry data only (no synchronous scan on the game thread): assets the
    // background scanner has not indexed yet are not listed.
    FARFilter Filter;
    Filter.PackagePaths.Add(FName(*Folder));
    Filter.bRecursivePaths = true;
    TArray<FAssetData> Assets;
    FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().GetAssets(Filter, Assets);
    for (const FAssetData& Asset : Assets)
    {
        OutPaths.Add(Asset.ToSoftObjectPath().ToString());
    }
    return true;
}

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
