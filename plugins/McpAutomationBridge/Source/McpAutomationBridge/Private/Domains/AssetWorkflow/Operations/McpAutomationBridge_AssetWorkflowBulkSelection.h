// Copyright (c) 2024 MCP Automation Bridge Contributors

#pragma once

#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersAssetPathCanonical.h"

#include "AssetRegistry/AssetRegistryModule.h"

// bulk_delete / bulk_rename act on the explicit assetPaths, else on every asset under folderPath.
// Replies and returns false when neither is usable.
inline bool McpCollectBulkAssetPaths(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId,
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
