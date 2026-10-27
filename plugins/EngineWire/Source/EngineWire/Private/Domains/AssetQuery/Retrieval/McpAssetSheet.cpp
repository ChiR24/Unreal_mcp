// Copyright (c) 2024 EngineWire Contributors
//
// manage_asset thumbnail_sheet: a page of a folder's assets as one numbered contact sheet of the thumbnails the editor
// keeps for them (none is rendered: rendering loads the asset), for the calling agent to look at and caption.

#include "Domains/AssetQuery/McpAssetQueryPrivate.h"
#include "Domains/AssetQuery/Retrieval/McpAssetIndex.h"
#include "Domains/ControlEditor/McpEditorScreenshotSupport.h"
#include "Domains/Perception/McpPerceptionSheet.h"
#include "ImageUtils.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace McpAssetQueryHandlers
{
namespace
{
constexpr int32 SheetTile = 192;
constexpr int32 SheetNameChars = 20; // with its number, what a 192 px label band shows uncut
const FColor SheetNone(70, 70, 74);

// The thumbnail fitted to a square tile, each tile pixel the mean of the thumbnail pixels it covers.
TArray<FColor> SheetTileOf(const TArray<uint8>& Bgra, int32 Width, int32 Height)
{
    TArray<FColor> Out;
    Out.Init(SheetNone, SheetTile * SheetTile);
    for (int32 Y = 0; Y < SheetTile; ++Y)
    {
        for (int32 X = 0; X < SheetTile; ++X)
        {
            const int32 X0 = X * Width / SheetTile, X1 = FMath::Max(X0 + 1, (X + 1) * Width / SheetTile);
            const int32 Y0 = Y * Height / SheetTile, Y1 = FMath::Max(Y0 + 1, (Y + 1) * Height / SheetTile);
            int32 Sum[3] = {0, 0, 0};
            for (int32 Sy = Y0; Sy < Y1; ++Sy)
            {
                for (int32 Sx = X0; Sx < X1; ++Sx)
                {
                    for (int32 C = 0; C < 3; ++C)
                    {
                        Sum[C] += Bgra[(Sy * Width + Sx) * 4 + C];
                    }
                }
            }
            const int32 Count = (X1 - X0) * (Y1 - Y0);
            Out[Y * SheetTile + X] = FColor(uint8(Sum[2] / Count), uint8(Sum[1] / Count), uint8(Sum[0] / Count), 255);
        }
    }
    return Out;
}
} // namespace

bool HandleThumbnailSheet(UEngineWireSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                          TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const FString FolderText = GetJsonStringField(Payload, TEXT("folder"), TEXT("/Game"));
    const FString Folder = SanitizeProjectRelativePath(FolderText);
    if (Folder.IsEmpty())
    {
        Bridge->SendAutomationError(Socket, RequestId, McpPathRefusalMessage(TEXT("folder"), FolderText), TEXT("INVALID_PATH"));
        return true;
    }
    FString Class = GetJsonStringField(Payload, TEXT("class"));
    int32 Dot = INDEX_NONE;
    Class = Class.FindLastChar(TEXT('.'), Dot) ? Class.Mid(Dot + 1) : Class;
    bool bUncaptioned = true;
    Payload->TryGetBoolField(TEXT("uncaptionedOnly"), bUncaptioned);
    const int32 Offset = FMath::Max(0, static_cast<int32>(GetJsonNumberField(Payload, TEXT("offset"), 0)));
    const int32 Limit = FMath::Clamp(static_cast<int32>(GetJsonNumberField(Payload, TEXT("limit"), 12)), 1, 16);
    const McpAssetRetrieval::FIndex& Index = McpAssetRetrieval::TheIndex();
    TArray<FAssetData> Assets = McpAssetRetrieval::AssetsUnder(Folder, Class);
    if (bUncaptioned)
    {
        Assets.RemoveAll([&Index](const FAssetData& Data) { return Index.Captions.Contains(Data.PackageName.ToString()); });
    }
    TArray<McpPerception::Sheet::FTile> Tiles;
    TArray<TSharedPtr<FJsonValue>> Rows;
    int32 Missing = 0;
    for (int32 At = Offset; At < Assets.Num() && At < Offset + Limit; ++At)
    {
        McpPerception::Sheet::FTile Tile;
        TArray<uint8> Bgra;
        int32 Width = 0, Height = 0;
        const bool bThumbnail = McpAssetRetrieval::ThumbnailOf(Assets[At], Bgra, Width, Height);
        Missing += bThumbnail ? 0 : 1;
        if (bThumbnail)
        {
            Tile.Pixels = SheetTileOf(Bgra, Width, Height);
        }
        else
        {
            Tile.Pixels.Init(SheetNone, SheetTile * SheetTile);
        }
        const FString Name = Assets[At].AssetName.ToString();
        Tile.Label = FString::Printf(TEXT("%d  %s"), At + 1, *(Name.Len() > SheetNameChars ? Name.Left(SheetNameChars - 2) + TEXT("..") : Name));
        Tiles.Add(MoveTemp(Tile));
        TSharedPtr<FJsonObject> Row = McpHandlerUtils::CreateResultObject();
        Row->SetNumberField(TEXT("number"), At + 1);
        Row->SetStringField(TEXT("assetPath"), Assets[At].PackageName.ToString());
        Row->SetStringField(TEXT("class"), McpAssetRetrieval::ClassOf(Assets[At]));
        Row->SetBoolField(TEXT("thumbnail"), bThumbnail);
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetArrayField(TEXT("assets"), Rows);
    Result->SetNumberField(TEXT("total"), Assets.Num());
    if (Offset + Limit < Assets.Num())
    {
        Result->SetNumberField(TEXT("nextOffset"), Offset + Limit);
    }
    const FString Which = bUncaptioned ? TEXT("uncaptioned asset(s)") : TEXT("asset(s)");
    if (Tiles.Num() == 0)
    {
        McpHandlerUtils::MarkNoAssetsChanged(Result);
        Bridge->SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("No %s under %s from %d (%d in all)."), *Which, *Folder, Offset + 1, Assets.Num()), Result, FString());
        return true;
    }
    const FString Title = FString::Printf(TEXT("%s   %d-%d of %d %s"), *Folder, Offset + 1, Offset + Tiles.Num(), Assets.Num(), *Which);
    int32 Width = 0, Height = 0;
    const TArray<FColor> Pixels = McpPerception::Sheet::Compose(Title, Tiles, SheetTile, Width, Height);
    TArray64<uint8> Png;
    FImageUtils::PNGCompressImageArray(Width, Height, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), Png);
    const FString File = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Captures") /
        FString::Printf(TEXT("ThumbnailSheet_%s.png"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"))));
    if (FFileHelper::SaveArrayToFile(Png, *File))
    {
        Result->SetStringField(TEXT("file"), File);
    }
    Result->SetNumberField(TEXT("width"), Width);
    Result->SetNumberField(TEXT("height"), Height);
    if (Png.Num() <= MaxScreenshotPngBytesForBase64ForMcp)
    {
        Result->SetStringField(TEXT("imageBase64"), FBase64::Encode(Png.GetData(), static_cast<uint32>(Png.Num())));
        Result->SetStringField(TEXT("mimeType"), TEXT("image/png"));
    }
    McpHandlerUtils::MarkNoAssetsChanged(Result);
    Bridge->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Sheet of %d %s under %s (%d-%d of %d)%s."), Tiles.Num(), *Which, *Folder, Offset + 1, Offset + Tiles.Num(), Assets.Num(),
                        Missing > 0 ? *FString::Printf(TEXT("; %d have no stored thumbnail"), Missing) : TEXT("")), Result, FString());
    return true;
}
} // namespace McpAssetQueryHandlers
