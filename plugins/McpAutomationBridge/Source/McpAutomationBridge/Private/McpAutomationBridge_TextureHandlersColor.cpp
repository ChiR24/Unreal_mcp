#include "McpAutomationBridge_TextureHandlersShared.h"

namespace McpTextureHandlers
{
namespace
{
UTexture2D* LoadSourceTexture(const FString& AssetPath)
{
    return Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *AssetPath));
}

bool PrepareCopyTarget(const FString& AssetPath, FString& Name, FString& Path, int32 Width, int32 Height,
                       const TCHAR* Suffix, TSharedPtr<FJsonObject>& Response, UTexture2D*& TargetTexture)
{
    if (Name.IsEmpty()) Name = FPaths::GetBaseFilename(AssetPath) + Suffix;
    if (Path.IsEmpty()) Path = FPaths::GetPath(AssetPath);
    Path = SanitizeProjectRelativePath(Path);
    if (Path.IsEmpty())
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("Invalid path: contains traversal or invalid characters"));
        return false;
    }
    Name = SanitizeAssetName(Name);
    if (Name.IsEmpty())
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("Invalid name: contains invalid characters"));
        return false;
    }
    TargetTexture = CreateEmptyTexture(Path, Name, Width, Height, false);
    if (!TargetTexture)
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("Failed to create output texture"));
        return false;
    }
    return true;
}

void CopyPlatformMip(UTexture2D* SourceTexture, uint8* TargetData, int32 Width, int32 Height)
{
    FTexture2DMipMap& SrcMip = SourceTexture->GetPlatformData()->Mips[0];
    const uint8* SrcData = static_cast<const uint8*>(SrcMip.BulkData.LockReadOnly());
    FMemory::Memcpy(TargetData, SrcData, Width * Height * 4);
    SrcMip.BulkData.Unlock();
}
}

TSharedPtr<FJsonObject> HandleTextureColorAction(const FString& SubAction, const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    FString AssetPath = GetStringFieldTextAuth(Params, TEXT("assetPath"), TEXT(""));
    AssetPath = SanitizeProjectRelativePath(AssetPath);
    if (AssetPath.IsEmpty())
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Invalid assetPath: contains traversal or invalid characters"));
    }

    UTexture2D* SourceTexture = LoadSourceTexture(AssetPath);
    if (!SourceTexture)
    {
        TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Failed to load texture: %s"), *AssetPath));
    }

    const int32 Width = SourceTexture->GetSizeX();
    const int32 Height = SourceTexture->GetSizeY();
    const bool bSave = GetBoolFieldTextAuth(Params, TEXT("save"), true);
    UTexture2D* TargetTexture = SourceTexture;
    FString Name = GetStringFieldTextAuth(Params, TEXT("name"), TEXT(""));
    FString Path = GetStringFieldTextAuth(Params, TEXT("path"), TEXT(""));
    const bool bInPlace = GetBoolFieldTextAuth(Params, TEXT("inPlace"), true);
    const bool bModifySource = bInPlace || SubAction == TEXT("adjust_levels");
    if (!bModifySource && !PrepareCopyTarget(AssetPath, Name, Path, Width, Height,
                                             SubAction == TEXT("invert") ? TEXT("_Inverted") : TEXT("_Desaturated"),
                                             Response, TargetTexture))
    {
        return Response;
    }

    uint8* MipData = TargetTexture->Source.LockMip(0);
    if (!MipData)
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Failed to lock texture mip data"));
    }
    if (!bModifySource)
    {
        CopyPlatformMip(SourceTexture, MipData, Width, Height);
    }

    if (SubAction == TEXT("invert"))
    {
        const bool bInvertAlpha = GetBoolFieldTextAuth(Params, TEXT("invertAlpha"), false);
        const FString Channel = GetStringFieldTextAuth(Params, TEXT("channel"), TEXT("All"));
        const bool bInvertR = Channel.Equals(TEXT("All"), ESearchCase::IgnoreCase) || Channel.Equals(TEXT("Red"), ESearchCase::IgnoreCase);
        const bool bInvertG = Channel.Equals(TEXT("All"), ESearchCase::IgnoreCase) || Channel.Equals(TEXT("Green"), ESearchCase::IgnoreCase);
        const bool bInvertB = Channel.Equals(TEXT("All"), ESearchCase::IgnoreCase) || Channel.Equals(TEXT("Blue"), ESearchCase::IgnoreCase);
        const bool bInvertA = bInvertAlpha && (Channel.Equals(TEXT("All"), ESearchCase::IgnoreCase) || Channel.Equals(TEXT("Alpha"), ESearchCase::IgnoreCase));
        for (int32 i = 0; i < Width * Height; ++i)
        {
            const int32 Idx = i * 4;
            if (bInvertB) MipData[Idx + 0] = 255 - MipData[Idx + 0];
            if (bInvertG) MipData[Idx + 1] = 255 - MipData[Idx + 1];
            if (bInvertR) MipData[Idx + 2] = 255 - MipData[Idx + 2];
            if (bInvertA) MipData[Idx + 3] = 255 - MipData[Idx + 3];
        }
        Response->SetStringField(TEXT("message"), TEXT("Texture colors inverted"));
    }
    else if (SubAction == TEXT("desaturate"))
    {
        const float Amount = FMath::Clamp(static_cast<float>(GetNumberFieldTextAuth(Params, TEXT("amount"), 1.0)), 0.0f, 1.0f);
        for (int32 i = 0; i < Width * Height; ++i)
        {
            const int32 Idx = i * 4;
            const uint8 Gray = static_cast<uint8>(0.2126f * MipData[Idx + 2] + 0.7152f * MipData[Idx + 1] + 0.0722f * MipData[Idx + 0]);
            MipData[Idx + 0] = static_cast<uint8>(FMath::Lerp((float)MipData[Idx + 0], (float)Gray, Amount));
            MipData[Idx + 1] = static_cast<uint8>(FMath::Lerp((float)MipData[Idx + 1], (float)Gray, Amount));
            MipData[Idx + 2] = static_cast<uint8>(FMath::Lerp((float)MipData[Idx + 2], (float)Gray, Amount));
        }
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Texture desaturated (amount: %.2f)"), Amount));
    }
    else if (SubAction == TEXT("adjust_levels"))
    {
        const float InBlack = FMath::Clamp(static_cast<float>(GetNumberFieldTextAuth(Params, TEXT("inBlack"), 0.0)), 0.0f, 1.0f);
        const float InWhite = FMath::Clamp(static_cast<float>(GetNumberFieldTextAuth(Params, TEXT("inWhite"), 1.0)), 0.0f, 1.0f);
        const float Gamma = FMath::Max(static_cast<float>(GetNumberFieldTextAuth(Params, TEXT("gamma"), 1.0)), 0.01f);
        const float OutBlack = FMath::Clamp(static_cast<float>(GetNumberFieldTextAuth(Params, TEXT("outBlack"), 0.0)), 0.0f, 1.0f);
        const float OutWhite = FMath::Clamp(static_cast<float>(GetNumberFieldTextAuth(Params, TEXT("outWhite"), 1.0)), 0.0f, 1.0f);
        const float InRange = FMath::Max(InWhite - InBlack, 0.001f);
        const float OutRange = OutWhite - OutBlack;
        const float InvGamma = 1.0f / Gamma;
        for (int32 i = 0; i < Width * Height; ++i)
        {
            const int32 Idx = i * 4;
            for (int32 c = 0; c < 3; ++c)
            {
                float Val = FMath::Clamp((MipData[Idx + c] / 255.0f - InBlack) / InRange, 0.0f, 1.0f);
                Val = OutBlack + FMath::Pow(Val, InvGamma) * OutRange;
                MipData[Idx + c] = static_cast<uint8>(FMath::Clamp(Val * 255.0f, 0.0f, 255.0f));
            }
        }
        Response->SetStringField(TEXT("message"), TEXT("Levels adjusted"));
    }

    TargetTexture->Source.UnlockMip(0);
    TargetTexture->UpdateResource();
    TargetTexture->MarkPackageDirty();
    if (bSave) McpSafeAssetSave(TargetTexture);
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("assetPath"), bModifySource ? AssetPath : (Path / Name));
    return Response;
}
}
