// Copyright (c) 2024 EngineWire Contributors
//
// manage_asset caption_assets: captions and tags the calling agent wrote after looking at thumbnail_sheet, kept in the
// project's caption file (Saved/EngineWire/AssetCaptions.json) for find_assets_like; the assets themselves stay untouched.

#include "Domains/AssetQuery/McpAssetQueryPrivate.h"
#include "Domains/AssetQuery/Retrieval/McpAssetIndex.h"
#include "Foundation/BridgeHelpers/Responses/McpResponses.h"
#include "Misc/Paths.h"

namespace McpAssetQueryHandlers
{
bool HandleCaptionAssets(UEngineWireSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                         TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
    if (!Payload->TryGetArrayField(TEXT("captions"), Entries) || Entries->Num() == 0)
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("captions lists {assetPath, caption, tags} entries (1-64)."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    // Every entry is checked before any is kept: all or nothing.
    TArray<TPair<FString, McpAssetRetrieval::FCaption>> Kept;
    TArray<FString> Missing;
    for (const TSharedPtr<FJsonValue>& Value : *Entries)
    {
        const TSharedPtr<FJsonObject> Entry = Value.IsValid() ? Value->AsObject() : nullptr;
        const FString Raw = Entry.IsValid() ? GetJsonStringField(Entry, TEXT("assetPath")) : FString();
        const FString Path = FPackageName::ObjectPathToPackageName(SanitizeProjectRelativePath(Raw));
        if (Path.IsEmpty())
        {
            Bridge->SendAutomationError(Socket, RequestId, McpPathRefusalMessage(TEXT("assetPath"), Raw), TEXT("INVALID_PATH"));
            return true;
        }
        McpAssetRetrieval::FCaption Caption;
        Caption.Text = GetJsonStringField(Entry, TEXT("caption")).TrimStartAndEnd();
        Caption.Tags = McpHandlerUtils::GetStringArrayField(Entry, TEXT("tags"));
        for (FString& Tag : Caption.Tags)
        {
            Tag.TrimStartAndEndInline();
        }
        Caption.Tags.RemoveAll([](const FString& Tag) { return Tag.IsEmpty(); });
        TArray<FAssetData> Found;
        FAssetRegistryModule::GetRegistry().GetAssetsByPackageName(FName(*Path), Found);
        if (Found.Num() == 0 && !(Caption.Text.IsEmpty() && Caption.Tags.Num() == 0)) // a removal needs no asset: it may be gone
        {
            Missing.Add(Path);
            continue;
        }
        Kept.Add(TPair<FString, McpAssetRetrieval::FCaption>(Path, MoveTemp(Caption)));
    }
    if (Missing.Num() > 0)
    {
        TSharedPtr<FJsonObject> Details = McpHandlerUtils::CreateResultObject();
        Details->SetArrayField(TEXT("missing"), McpHandlerUtils::ToJsonStringArray(Missing));
        SendStandardErrorResponse(Bridge, Socket, RequestId, TEXT("ASSET_NOT_FOUND"),
                                  FString::Printf(TEXT("Not assets in the project: %s; no caption kept."), *FString::Join(Missing, TEXT(", "))), Details);
        return true;
    }
    McpAssetRetrieval::FIndex& Index = McpAssetRetrieval::TheIndex();
    TMap<FString, McpAssetRetrieval::FCaption> Next = Index.Captions; // kept only once the file holds it
    int32 Captioned = 0, Removed = 0;
    for (TPair<FString, McpAssetRetrieval::FCaption>& Pair : Kept)
    {
        if (Pair.Value.Text.IsEmpty() && Pair.Value.Tags.Num() == 0) // an empty entry takes the caption away
        {
            Removed += Next.Remove(Pair.Key);
            continue;
        }
        Next.Add(Pair.Key, MoveTemp(Pair.Value));
        ++Captioned;
    }
    const FString File = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("EngineWire/AssetCaptions.json"));
    if (!McpAssetRetrieval::SaveCaptions(Next))
    {
        Bridge->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Could not write %s; no caption kept."), *File), TEXT("WRITE_FAILED"));
        return true;
    }
    Index.Captions = MoveTemp(Next);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetNumberField(TEXT("captioned"), Captioned);
    Result->SetNumberField(TEXT("removed"), Removed);
    Result->SetNumberField(TEXT("totalCaptions"), Index.Captions.Num());
    Result->SetStringField(TEXT("file"), File);
    McpHandlerUtils::MarkNoAssetsChanged(Result);
    Bridge->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Kept %d caption(s)%s; %d in the project's caption file."), Captioned,
                        Removed > 0 ? *FString::Printf(TEXT(", removed %d"), Removed) : TEXT(""), Index.Captions.Num()), Result, FString());
    return true;
}
} // namespace McpAssetQueryHandlers
