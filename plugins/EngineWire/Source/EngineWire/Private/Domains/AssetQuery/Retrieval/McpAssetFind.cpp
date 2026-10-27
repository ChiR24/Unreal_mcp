// Copyright (c) 2024 EngineWire Contributors
//
// manage_asset find_assets_like: assets ranked by words, by likeness to an example asset, by colour and by size, from the
// retrieval index (McpAssetIndex), brought up to date first within a time budget.

#include "Domains/AssetQuery/McpAssetQueryPrivate.h"
#include "Domains/AssetQuery/Retrieval/McpAssetRank.h"
#include "Domains/Perception/McpPerception.h"
#include "Foundation/BridgeHelpers/Responses/McpResponses.h"
#include "HAL/PlatformTime.h"

namespace McpAssetQueryHandlers
{
namespace
{
constexpr double FindRefreshSeconds = 10.0;
constexpr int32 FindDefaultLimit = 10;
constexpr int32 FindMaxLimit = 50;

// "#RRGGBB" or a colour's name, as OKLab; unset when it is neither.
TOptional<FVector3f> FindColour(const FString& Text)
{
    static const TMap<FString, FColor> Names = {
        {TEXT("red"), FColor(200, 30, 30)}, {TEXT("orange"), FColor(230, 120, 30)}, {TEXT("yellow"), FColor(230, 210, 40)},
        {TEXT("green"), FColor(50, 160, 50)}, {TEXT("cyan"), FColor(40, 200, 210)}, {TEXT("blue"), FColor(40, 80, 200)},
        {TEXT("purple"), FColor(130, 50, 170)}, {TEXT("pink"), FColor(240, 130, 180)}, {TEXT("brown"), FColor(120, 75, 40)},
        {TEXT("black"), FColor(15, 15, 15)}, {TEXT("white"), FColor(240, 240, 240)}, {TEXT("grey"), FColor(128, 128, 128)},
        {TEXT("gray"), FColor(128, 128, 128)}};
    FColor Colour;
    const FString Hex = Text.StartsWith(TEXT("#")) ? Text.RightChop(1) : FString();
    bool bHex = Hex.Len() == 6;
    for (const TCHAR Char : Hex)
    {
        bHex &= FChar::IsHexDigit(Char);
    }
    if (bHex)
    {
        Colour = FColor::FromHex(Hex);
    }
    else if (const FColor* Named = Names.Find(Text.ToLower()))
    {
        Colour = *Named;
    }
    else
    {
        return TOptional<FVector3f>();
    }
    return McpPerception::OkLab(FLinearColor(Colour.R / 255.f, Colour.G / 255.f, Colour.B / 255.f));
}

TSharedPtr<FJsonObject> FindRow(const McpAssetRetrieval::FIndex& Index, const McpAssetRetrieval::FRanked& Ranked)
{
    const McpAssetRetrieval::FIndexed& Entry = Index.Assets[Ranked.Path];
    TSharedPtr<FJsonObject> Row = McpHandlerUtils::CreateResultObject();
    Row->SetStringField(TEXT("assetPath"), Ranked.Path);
    Row->SetStringField(TEXT("class"), Entry.Class);
    Row->SetNumberField(TEXT("score"), FMath::RoundToDouble(Ranked.Score * 1e4) / 1e4);
    Row->SetArrayField(TEXT("why"), McpHandlerUtils::ToJsonStringArray(Ranked.Why));
    if (const McpAssetRetrieval::FCaption* Caption = Index.Captions.Find(Ranked.Path))
    {
        Row->SetStringField(TEXT("caption"), Caption->Text);
    }
    if (!Entry.Size.IsZero())
    {
        TArray<TSharedPtr<FJsonValue>> Size;
        for (const double Axis : {Entry.Size.X, Entry.Size.Y, Entry.Size.Z})
        {
            Size.Add(MakeShared<FJsonValueNumber>(FMath::RoundToDouble(Axis)));
        }
        Row->SetArrayField(TEXT("sizeCm"), Size);
    }
    return Row;
}
} // namespace

bool HandleFindAssetsLike(UEngineWireSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                          TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const auto Refuse = [&](const FString& Message, const TCHAR* Code) { Bridge->SendAutomationError(Socket, RequestId, Message, Code); return true; };
    McpAssetRetrieval::FIndex& Index = McpAssetRetrieval::TheIndex();
    McpAssetRetrieval::FAsk Ask;
    Ask.Words = McpAssetRetrieval::WordsOf(GetJsonStringField(Payload, TEXT("query")));
    Ask.SizeCm = GetJsonNumberField(Payload, TEXT("sizeCm"), 0.0);
    const FString ColourText = GetJsonStringField(Payload, TEXT("colour"));
    if (!ColourText.IsEmpty())
    {
        Ask.Colour = FindColour(ColourText);
        if (!Ask.Colour.IsSet())
        {
            return Refuse(FString::Printf(TEXT("colour '%s' is neither #RRGGBB nor a colour name (red, orange, yellow, green, cyan, blue, purple, pink, brown, black, white, grey)."), *ColourText), TEXT("INVALID_ARGUMENT"));
        }
    }
    const FString FolderText = GetJsonStringField(Payload, TEXT("folder"), TEXT("/Game"));
    const FString Folder = SanitizeProjectRelativePath(FolderText);
    if (Folder.IsEmpty())
    {
        return Refuse(McpPathRefusalMessage(TEXT("folder"), FolderText), TEXT("INVALID_PATH"));
    }
    FString Class = GetJsonStringField(Payload, TEXT("class"));
    int32 Dot = INDEX_NONE;
    Class = Class.FindLastChar(TEXT('.'), Dot) ? Class.Mid(Dot + 1) : Class;
    const FString LikeText = GetJsonStringField(Payload, TEXT("likeAsset"));
    if (!LikeText.IsEmpty())
    {
        const FString Like = SanitizeProjectRelativePath(LikeText);
        if (Like.IsEmpty())
        {
            return Refuse(McpPathRefusalMessage(TEXT("likeAsset"), LikeText), TEXT("INVALID_PATH"));
        }
        Ask.Like = FPackageName::ObjectPathToPackageName(Like);
        TArray<FAssetData> Found;
        FAssetRegistryModule::GetRegistry().GetAssetsByPackageName(FName(*Ask.Like), Found);
        if (Found.Num() == 0 || !McpAssetRetrieval::Update(Index, Found[0]))
        {
            return Refuse(FString::Printf(TEXT("likeAsset %s is not an asset saved in the project."), *Ask.Like), TEXT("ASSET_NOT_FOUND"));
        }
        Class = Class.IsEmpty() ? Index.Assets[Ask.Like].Class : Class;
    }
    if (Ask.Words.Num() == 0 && Ask.Like.IsEmpty() && !Ask.Colour.IsSet() && Ask.SizeCm <= 0.0)
    {
        return Refuse(TEXT("Name what to look for: query (words), likeAsset (an example asset), colour or sizeCm."), TEXT("INVALID_ARGUMENT"));
    }
    TArray<FString> Paths;
    const int32 Pending = McpAssetRetrieval::Refresh(Index, Folder, Class, FPlatformTime::Seconds() + FindRefreshSeconds, Paths);
    if (Index.bDirty)
    {
        McpAssetRetrieval::SaveIndex(Index);
    }
    const TArray<McpAssetRetrieval::FRanked> Ranked = McpAssetRetrieval::Rank(Index, Paths, Ask);
    const int32 Limit = FMath::Clamp(static_cast<int32>(GetJsonNumberField(Payload, TEXT("limit"), FindDefaultLimit)), 1, FindMaxLimit);
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (int32 At = 0; At < Ranked.Num() && At < Limit; ++At)
    {
        Rows.Add(MakeShared<FJsonValueObject>(FindRow(Index, Ranked[At])));
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetArrayField(TEXT("assets"), Rows);
    Result->SetNumberField(TEXT("matched"), Ranked.Num());
    Result->SetNumberField(TEXT("considered"), Paths.Num());
    Result->SetNumberField(TEXT("pending"), Pending);
    const McpAssetRetrieval::FIndexed* LikeEntry = Ask.Like.IsEmpty() ? nullptr : Index.Assets.Find(Ask.Like);
    FString Message = Ranked.Num() == 0
        ? FString::Printf(TEXT("Nothing under %s matches (%d asset(s) considered)."), *Folder, Paths.Num())
        : FString::Printf(TEXT("%d of %d asset(s) under %s match; best %s (%s)."), Ranked.Num(), Paths.Num(), *Folder, *Ranked[0].Path,
                          *FString::Join(Ranked[0].Why, TEXT(", ")));
    if (LikeEntry && !LikeEntry->bHasLook)
    {
        Message += TEXT(" likeAsset has no stored thumbnail, so it was compared by name and size only.");
    }
    if (Pending > 0)
    {
        Message += FString::Printf(TEXT(" %d asset(s) not read yet (the index reads for %.0f s a call): call again to include them."), Pending, FindRefreshSeconds);
    }
    McpHandlerUtils::MarkNoAssetsChanged(Result);
    Bridge->SendAutomationResponse(Socket, RequestId, true, Message, Result, FString());
    return true;
}
} // namespace McpAssetQueryHandlers
