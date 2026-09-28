// Copyright (c) 2024 MCP Automation Bridge Contributors
//
// move / rename with a folder as sourcePath. The Content Browser renames folders; the MCP only moved
// one asset per call, so relocating a game's root folder (/Game/Mario held 207 assets) was out of reach.

#include "Domains/AssetWorkflow/Rename/McpAutomationBridge_AssetRenameGuard.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Safety/McpSafeOperations.h"
#include "Safety/McpSafeOperationsDeleteCompilation.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"

namespace McpAssetRename
{
namespace
{
void MoveFolderContents(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SourceFolder,
                        const FString& DestinationFolder, const FString& OpenLevel, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    // Redirectors from earlier renames are not assets to move; settle them first.
    int32 RedirectorsBefore = 0;
    int32 FixedBefore = 0;
    FixupRedirectorsIn(SourceFolder, RedirectorsBefore, FixedBefore);

    FARFilter Filter;
    Filter.PackagePaths.Add(FName(*SourceFolder));
    Filter.bRecursivePaths = true;
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    TArray<FAssetData> Assets;
    Registry.GetAssets(Filter, Assets);
    TArray<FAssetRenameData> RenameData;
    for (const FAssetData& Asset : Assets)
    {
        UObject* Object = Asset.IsRedirector() ? nullptr : Asset.GetAsset();
        if (Object)
        {
            RenameData.Emplace(Object, DestinationFolder + Asset.PackagePath.ToString().Mid(SourceFolder.Len()),
                               Asset.AssetName.ToString());
        }
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    FString Failure;
    const bool bMoved = RenameData.Num() > 0 && RenameWithSettingsFollow(RenameData, Result, Failure);
    int32 RedirectorsFound = 0;
    int32 RedirectorsFixed = 0;
    if (bMoved)
    {
        FixupRedirectorsIn(SourceFolder, RedirectorsFound, RedirectorsFixed);
        TArray<FAssetData> Left;
        Registry.GetAssets(Filter, Left);
        if (Left.Num() == 0)
        {
            McpSafeOperations::McpSafeDeleteFolder(SourceFolder);
        }
    }
    if (!OpenLevel.IsEmpty())
    {
        const FString Reopen = bMoved ? DestinationFolder + OpenLevel.Mid(SourceFolder.Len()) : OpenLevel;
        McpSafeOperations::McpSafeLoadMap(Reopen);
        Result->SetStringField(TEXT("reopenedLevel"), Reopen);
    }

    Result->SetStringField(TEXT("sourceFolder"), SourceFolder);
    Result->SetStringField(TEXT("destinationFolder"), DestinationFolder);
    Result->SetNumberField(TEXT("movedCount"), bMoved ? RenameData.Num() : 0);
    Result->SetNumberField(TEXT("redirectorsFixed"), FixedBefore + RedirectorsFixed);
    if (RenameData.Num() == 0)
    {
        Bridge->SendAutomationResponse(Socket, RequestId, false, FString::Printf(TEXT("No assets under %s"), *SourceFolder),
                                       Result, TEXT("ASSET_NOT_FOUND"));
        return;
    }
    Result->SetBoolField(TEXT("success"), bMoved);
    Bridge->SendAutomationResponse(
        Socket, RequestId, bMoved,
        bMoved ? FString::Printf(TEXT("Moved %d assets from %s to %s"), RenameData.Num(), *SourceFolder, *DestinationFolder)
               : Failure,
        Result, bMoved ? FString() : TEXT("RENAME_FAILED"));
}
} // namespace

bool HandleMoveFolder(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SourceFolder,
                      const FString& DestinationFolder, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    if (GEditor && GEditor->PlayWorld)
    {
        Bridge->SendAutomationError(Socket, RequestId,
                                    TEXT("Stop Play In Editor before moving a folder (control_editor play, control: stop)."),
                                    TEXT("PIE_ACTIVE"));
        return true;
    }
    if (DestinationFolder.Equals(SourceFolder, ESearchCase::IgnoreCase) ||
        DestinationFolder.StartsWith(SourceFolder + TEXT("/"), ESearchCase::IgnoreCase))
    {
        Bridge->SendAutomationError(Socket, RequestId,
                                    FString::Printf(TEXT("Cannot move %s into itself (%s)"), *SourceFolder, *DestinationFolder),
                                    TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // The editor cannot rename the level it has open: it waits on a blank map and comes back afterwards.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    const FString Current = World ? World->GetOutermost()->GetName() : FString();
    if (!Current.StartsWith(SourceFolder + TEXT("/"), ESearchCase::IgnoreCase))
    {
        MoveFolderContents(Bridge, RequestId, SourceFolder, DestinationFolder, FString(), Socket);
        return true;
    }
    if (World->GetOutermost()->IsDirty())
    {
        Bridge->SendAutomationError(Socket, RequestId,
                                    FString::Printf(TEXT("%s is open with unsaved changes; save it first (control_editor save_all)."), *Current),
                                    TEXT("UNSAVED_CHANGES"));
        return true;
    }
    GEditor->NewMap(false);
    McpSafeOperations::McpSafePostDeleteGC();
    MoveFolderContents(Bridge, RequestId, SourceFolder, DestinationFolder, Current, Socket);
    return true;
}
} // namespace McpAssetRename
