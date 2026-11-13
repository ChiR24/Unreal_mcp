// Copyright (c) 2024 EngineWire Contributors

#pragma once

#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/Assets/McpAssetReferencers.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/BridgeHelpers/Security/McpAssetPathCanonical.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"

// Recompiles the loaded Blueprints among Packages, the users of an asset deleted with force, so their broken variables
// and pins show at once.
inline void McpRecompileLoadedBlueprints(const TArray<FString>& Packages)
{
    for (const FString& Package : Packages)
    {
        TArray<FAssetData> PackageAssets;
        FAssetRegistryModule::GetRegistry().GetAssetsByPackageName(FName(*Package), PackageAssets);
        for (const FAssetData& AssetData : PackageAssets)
        {
            if (UBlueprint* BP = AssetData.IsAssetLoaded() ? Cast<UBlueprint>(AssetData.GetAsset()) : nullptr)
            {
                McpCompileBlueprint(BP, nullptr, EBlueprintCompileOptions::None);
            }
        }
    }
}

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
        OutPaths.Append(McpHandlerUtils::GetStringArrayField(Payload, TEXT("assetPaths")));
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
        OutPaths.Add(MCP_ASSET_DATA_GET_SOFT_PATH(Asset));
    }
    return true;
}
