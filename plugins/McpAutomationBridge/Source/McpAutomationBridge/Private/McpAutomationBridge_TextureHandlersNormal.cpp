#include "McpAutomationBridge_TextureHandlersShared.h"

namespace McpTextureHandlers
{
namespace
{
float ReadHeightValue(const uint8* Pixels, int32 Index, const FString& ChannelMode)
{
    const uint8 B = Pixels[Index * 4 + 0];
    const uint8 G = Pixels[Index * 4 + 1];
    const uint8 R = Pixels[Index * 4 + 2];
    const uint8 A = Pixels[Index * 4 + 3];
    if (ChannelMode.Equals(TEXT("red"), ESearchCase::IgnoreCase)) return static_cast<float>(R) / 255.0f;
    if (ChannelMode.Equals(TEXT("green"), ESearchCase::IgnoreCase)) return static_cast<float>(G) / 255.0f;
    if (ChannelMode.Equals(TEXT("blue"), ESearchCase::IgnoreCase)) return static_cast<float>(B) / 255.0f;
    if (ChannelMode.Equals(TEXT("alpha"), ESearchCase::IgnoreCase)) return static_cast<float>(A) / 255.0f;
    if (ChannelMode.Equals(TEXT("average"), ESearchCase::IgnoreCase))
    {
        return (static_cast<float>(R) + static_cast<float>(G) + static_cast<float>(B)) / (255.0f * 3.0f);
    }
    return (0.2126f * static_cast<float>(R) + 0.7152f * static_cast<float>(G) + 0.0722f * static_cast<float>(B)) / 255.0f;
}
}

TSharedPtr<FJsonObject> HandleCreateNormalFromHeight(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    TSet<FString> ValidParams = {
        TEXT("subAction"), TEXT("sourceTexture"), TEXT("name"), TEXT("path"),
        TEXT("strength"), TEXT("algorithm"), TEXT("flipY"), TEXT("save"), TEXT("channelMode")
    };
    for (const auto& Field : Params->Values)
    {
        if (!ValidParams.Contains(FString(*Field.Key)))
        {
            TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Invalid parameter: %s"), *Field.Key));
        }
    }

    FString SourceTexture = GetStringFieldTextAuth(Params, TEXT("sourceTexture"), TEXT(""));
    FString Name = GetStringFieldTextAuth(Params, TEXT("name"), TEXT(""));
    FString Path = GetStringFieldTextAuth(Params, TEXT("path"), TEXT(""));
    FString SanitizedSource = SanitizeProjectRelativePath(SourceTexture);
    if (SanitizedSource.IsEmpty())
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Invalid sourceTexture: contains traversal or invalid characters"));
    }
    SourceTexture = SanitizedSource;

    const float Strength = static_cast<float>(GetNumberFieldTextAuth(Params, TEXT("strength"), 1.0));
    const FString Algorithm = GetStringFieldTextAuth(Params, TEXT("algorithm"), TEXT("Sobel"));
    const bool bFlipY = GetBoolFieldTextAuth(Params, TEXT("flipY"), false);
    const bool bSave = GetBoolFieldTextAuth(Params, TEXT("save"), true);
    if (SourceTexture.IsEmpty())
    {
        TEXTURE_ERROR_RESPONSE(TEXT("sourceTexture is required"));
    }

    UTexture2D* HeightMap = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *SourceTexture));
    if (!HeightMap)
    {
        TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Failed to load height map: %s"), *SourceTexture));
    }

    const int32 Width = HeightMap->GetSizeX();
    const int32 Height = HeightMap->GetSizeY();
    if (Name.IsEmpty()) Name = FPaths::GetBaseFilename(SourceTexture) + TEXT("_N");
    if (Path.IsEmpty()) Path = FPaths::GetPath(SourceTexture);

    FString SanitizedPath = SanitizeProjectRelativePath(Path);
    if (SanitizedPath.IsEmpty())
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Invalid path: contains traversal or invalid characters"));
    }
    Path = SanitizedPath;
    FString SanitizedName = SanitizeAssetName(Name);
    if (SanitizedName.IsEmpty())
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Invalid name: contains invalid characters"));
    }
    Name = SanitizedName;

    UTexture2D* NormalMap = CreateEmptyTexture(Path, Name, Width, Height, false);
    if (!NormalMap)
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Failed to create normal map texture"));
    }
    NormalMap->PreEditChange(nullptr);
    NormalMap->SRGB = false;
    NormalMap->CompressionSettings = TC_Normalmap;
    NormalMap->PostEditChange();
    NormalMap->UpdateResource();

    if (!HeightMap->Source.IsValid())
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Height map has no source data - texture may be compressed or not fully loaded"));
    }
    if (HeightMap->IsStreamable())
    {
        HeightMap->SetForceMipLevelsToBeResident(30.0f);
    }

    const uint8* HeightPixels = HeightMap->Source.LockMipReadOnly(0);
    if (!HeightPixels)
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Failed to lock height map pixel data - texture may be compressed or streaming"));
    }

    const FString ChannelMode = GetStringFieldTextAuth(Params, TEXT("channelMode"), TEXT("luminance"));
    TArray<float> HeightData;
    HeightData.SetNum(Width * Height);
    for (int32 Index = 0; Index < Width * Height; ++Index)
    {
        HeightData[Index] = ReadHeightValue(HeightPixels, Index, ChannelMode);
    }
    HeightMap->Source.UnlockMip(0);

    uint8* NormalData = NormalMap->Source.LockMip(0);
    for (int32 Y = 0; Y < Height; ++Y)
    {
        for (int32 X = 0; X < Width; ++X)
        {
            auto SampleHeight = [&](int32 SX, int32 SY) -> float
            {
                return HeightData[((SY + Height) % Height) * Width + ((SX + Width) % Width)];
            };
            float DX = 0.0f;
            float DY = 0.0f;
            if (Algorithm == TEXT("Sobel"))
            {
                DX = SampleHeight(X - 1, Y - 1) * -1.0f + SampleHeight(X - 1, Y) * -2.0f +
                     SampleHeight(X - 1, Y + 1) * -1.0f + SampleHeight(X + 1, Y - 1) +
                     SampleHeight(X + 1, Y) * 2.0f + SampleHeight(X + 1, Y + 1);
                DY = SampleHeight(X - 1, Y - 1) * -1.0f + SampleHeight(X, Y - 1) * -2.0f +
                     SampleHeight(X + 1, Y - 1) * -1.0f + SampleHeight(X - 1, Y + 1) +
                     SampleHeight(X, Y + 1) * 2.0f + SampleHeight(X + 1, Y + 1);
            }
            else
            {
                DX = SampleHeight(X + 1, Y) - SampleHeight(X - 1, Y);
                DY = SampleHeight(X, Y + 1) - SampleHeight(X, Y - 1);
            }

            FVector Normal(-DX * Strength, bFlipY ? DY * Strength : -DY * Strength, 1.0f);
            Normal.Normalize();
            const int32 PixelIndex = (Y * Width + X) * 4;
            NormalData[PixelIndex + 0] = static_cast<uint8>((Normal.Z * 0.5f + 0.5f) * 255.0f);
            NormalData[PixelIndex + 1] = static_cast<uint8>((Normal.Y * 0.5f + 0.5f) * 255.0f);
            NormalData[PixelIndex + 2] = static_cast<uint8>((Normal.X * 0.5f + 0.5f) * 255.0f);
            NormalData[PixelIndex + 3] = 255;
        }
    }

    NormalMap->Source.UnlockMip(0);
    NormalMap->UpdateResource();
    if (bSave)
    {
        FAssetRegistryModule::AssetCreated(NormalMap);
        McpSafeAssetSave(NormalMap);
    }

    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("message"), TEXT("Normal map created from height map"));
    McpHandlerUtils::AddVerification(Response, NormalMap);
    return Response;
}
}
