#include "Domains/Texture/McpAutomationBridge_TextureHandlersShared.h"

namespace McpTextureHandlers
{
TSharedPtr<FJsonObject> HandleCreateNormalFromHeight(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    const float Strength = static_cast<float>(GetJsonNumberField(Params, TEXT("strength"), 1.0));
    FString SourcePath;
    FString Error;
    UTexture2D* HeightMap = LoadSourceTexture(GetJsonStringField(Params, TEXT("sourceTexture")), TEXT("sourceTexture"), SourcePath, Error);
    FString Path;
    FString Name;
    if (!HeightMap ||
        !ResolveOutputTarget(Params, FPaths::GetPath(SourcePath), FPaths::GetBaseFilename(SourcePath) + TEXT("_N"), Path, Name, Error))
    {
        TEXTURE_ERROR_RESPONSE(Error);
    }

    const int32 Width = HeightMap->Source.GetSizeX();
    const int32 Height = HeightMap->Source.GetSizeY();
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

    const uint8* HeightPixels = HeightMap->Source.LockMipReadOnly(0);
    if (!HeightPixels)
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Failed to lock height map pixel data"));
    }

    // Height is the pixel's luminance.
    TArray<float> HeightData;
    HeightData.SetNum(Width * Height);
    for (int32 Index = 0; Index < Width * Height; ++Index)
    {
        const uint8* Pixel = HeightPixels + Index * 4;
        HeightData[Index] = (0.2126f * Pixel[2] + 0.7152f * Pixel[1] + 0.0722f * Pixel[0]) / 255.0f;
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
            // Sobel gradient.
            const float DX = SampleHeight(X - 1, Y - 1) * -1.0f + SampleHeight(X - 1, Y) * -2.0f +
                             SampleHeight(X - 1, Y + 1) * -1.0f + SampleHeight(X + 1, Y - 1) +
                             SampleHeight(X + 1, Y) * 2.0f + SampleHeight(X + 1, Y + 1);
            const float DY = SampleHeight(X - 1, Y - 1) * -1.0f + SampleHeight(X, Y - 1) * -2.0f +
                             SampleHeight(X + 1, Y - 1) * -1.0f + SampleHeight(X - 1, Y + 1) +
                             SampleHeight(X, Y + 1) * 2.0f + SampleHeight(X + 1, Y + 1);
            FVector Normal(-DX * Strength, -DY * Strength, 1.0f);
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
    FAssetRegistryModule::AssetCreated(NormalMap);
    McpSafeAssetSave(NormalMap);

    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("message"), TEXT("Normal map created from height map"));
    McpHandlerUtils::AddVerification(Response, NormalMap);
    return Response;
}
}
