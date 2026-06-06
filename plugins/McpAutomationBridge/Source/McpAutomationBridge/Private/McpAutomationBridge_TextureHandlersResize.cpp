#include "McpAutomationBridge_TextureHandlersShared.h"

namespace McpTextureHandlers
{
namespace
{
uint8 ClampColorChannel(double Value)
{
    return static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(Value), 0, 255));
}

double CubicWeight(double X)
{
    X = FMath::Abs(X);
    if (X <= 1.0) return (1.5 * X * X * X) - (2.5 * X * X) + 1.0;
    if (X < 2.0) return (-0.5 * X * X * X) + (2.5 * X * X) - (4.0 * X) + 2.0;
    return 0.0;
}

double LanczosWeight(double X)
{
    X = FMath::Abs(X);
    if (X < KINDA_SMALL_NUMBER) return 1.0;
    if (X >= 3.0) return 0.0;
    const double PiX = PI * X;
    return (FMath::Sin(PiX) / PiX) * (FMath::Sin(PiX / 3.0) / (PiX / 3.0));
}
}

TSharedPtr<FJsonObject> HandleResizeTexture(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    TSet<FString> ValidParams = {
        TEXT("subAction"), TEXT("sourcePath"), TEXT("name"), TEXT("path"),
        TEXT("newWidth"), TEXT("newHeight"), TEXT("filterMethod"), TEXT("save")
    };
    for (const auto& Field : Params->Values)
    {
        if (!ValidParams.Contains(FString(*Field.Key)))
        {
            TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Invalid parameter: %s"), *Field.Key));
        }
    }

    FString SourcePath = GetStringFieldTextAuth(Params, TEXT("sourcePath"), TEXT(""));
    FString Name = GetStringFieldTextAuth(Params, TEXT("name"), TEXT(""));
    FString Path = GetStringFieldTextAuth(Params, TEXT("path"), TEXT(""));
    const FString SanitizedSource = SanitizeProjectRelativePath(SourcePath);
    if (SanitizedSource.IsEmpty())
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Invalid sourcePath: contains traversal or invalid characters"));
    }
    SourcePath = SanitizedSource;

    int32 NewWidth = 0;
    int32 NewHeight = 0;
    FString ValidationError;
    if (!ValidateGeneratedTextureDimensions(GetNumberFieldTextAuth(Params, TEXT("newWidth"), 512),
                                            GetNumberFieldTextAuth(Params, TEXT("newHeight"), 512),
                                            TEXT("newWidth"), TEXT("newHeight"),
                                            NewWidth, NewHeight, ValidationError))
    {
        TEXTURE_ERROR_RESPONSE(ValidationError);
    }
    const FString FilterMethod = GetStringFieldTextAuth(Params, TEXT("filterMethod"), TEXT("Bilinear"));
    const FString FilterMethodLower = FilterMethod.ToLower();
    const bool bSave = GetBoolFieldTextAuth(Params, TEXT("save"), true);
    if (FilterMethodLower != TEXT("nearest") && FilterMethodLower != TEXT("bilinear") &&
        FilterMethodLower != TEXT("bicubic") && FilterMethodLower != TEXT("lanczos"))
    {
        TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Unsupported filterMethod: %s"), *FilterMethod));
    }
    if (SourcePath.IsEmpty())
    {
        TEXTURE_ERROR_RESPONSE(TEXT("sourcePath is required"));
    }

    UTexture2D* SourceTexture = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *SourcePath));
    if (!SourceTexture)
    {
        TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Failed to load source texture: %s"), *SourcePath));
    }
    if (!SourceTexture->Source.IsValid())
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Source texture has no source data - may be compressed or not fully loaded"));
    }
    if (SourceTexture->IsStreamable())
    {
        SourceTexture->SetForceMipLevelsToBeResident(30.0f);
    }

    const int32 SrcWidth = SourceTexture->GetSizeX();
    const int32 SrcHeight = SourceTexture->GetSizeY();
    const uint8* SrcData = SourceTexture->Source.LockMip(0);
    if (!SrcData)
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Failed to lock source texture data - texture may be compressed or streaming"));
    }

    if (Name.IsEmpty()) Name = FPaths::GetBaseFilename(SourcePath) + TEXT("_Resized");
    if (Path.IsEmpty()) Path = FPaths::GetPath(SourcePath);
    const FString SanitizedPath = SanitizeProjectRelativePath(Path);
    if (SanitizedPath.IsEmpty())
    {
        SourceTexture->Source.UnlockMip(0);
        TEXTURE_ERROR_RESPONSE(TEXT("Invalid path: contains traversal or invalid characters"));
    }
    Path = SanitizedPath;
    const FString SanitizedName = SanitizeAssetName(Name);
    if (SanitizedName.IsEmpty())
    {
        SourceTexture->Source.UnlockMip(0);
        TEXTURE_ERROR_RESPONSE(TEXT("Invalid name: contains invalid characters"));
    }
    Name = SanitizedName;

    UTexture2D* NewTexture = CreateEmptyTexture(Path, Name, NewWidth, NewHeight, false);
    if (!NewTexture)
    {
        SourceTexture->Source.UnlockMip(0);
        TEXTURE_ERROR_RESPONSE(TEXT("Failed to create resized texture"));
    }
    uint8* DstMipData = NewTexture->Source.LockMip(0);
    if (!DstMipData)
    {
        SourceTexture->Source.UnlockMip(0);
        TEXTURE_ERROR_RESPONSE(TEXT("Failed to lock destination texture data"));
    }

    auto GetPixelBGRA = [&](int32 PX, int32 PY) -> FColor
    {
        PX = FMath::Clamp(PX, 0, SrcWidth - 1);
        PY = FMath::Clamp(PY, 0, SrcHeight - 1);
        const int32 Idx = (PY * SrcWidth + PX) * 4;
        return FColor(SrcData[Idx + 2], SrcData[Idx + 1], SrcData[Idx + 0], SrcData[Idx + 3]);
    };

    for (int32 Y = 0; Y < NewHeight; ++Y)
    {
        for (int32 X = 0; X < NewWidth; ++X)
        {
            const float U = NewWidth > 1 ? static_cast<float>(X) / static_cast<float>(NewWidth - 1) * (SrcWidth - 1) : 0.0f;
            const float V = NewHeight > 1 ? static_cast<float>(Y) / static_cast<float>(NewHeight - 1) * (SrcHeight - 1) : 0.0f;
            const int32 X0 = FMath::FloorToInt(U);
            const int32 Y0 = FMath::FloorToInt(V);
            const int32 X1 = FMath::Min(X0 + 1, SrcWidth - 1);
            const int32 Y1 = FMath::Min(Y0 + 1, SrcHeight - 1);
            const float FracX = U - X0;
            const float FracY = V - Y0;
            FColor SampledColor;
            if (FilterMethodLower == TEXT("nearest"))
            {
                SampledColor = GetPixelBGRA(FMath::RoundToInt(U), FMath::RoundToInt(V));
            }
            else if (FilterMethodLower == TEXT("bicubic") || FilterMethodLower == TEXT("lanczos"))
            {
                const bool bLanczos = FilterMethodLower == TEXT("lanczos");
                const int32 Radius = bLanczos ? 3 : 2;
                double SumR = 0.0, SumG = 0.0, SumB = 0.0, SumA = 0.0, SumW = 0.0;
                for (int32 KY = -Radius + 1; KY <= Radius; ++KY)
                {
                    for (int32 KX = -Radius + 1; KX <= Radius; ++KX)
                    {
                        const int32 SX = X0 + KX;
                        const int32 SY = Y0 + KY;
                        const double WX = bLanczos ? LanczosWeight(static_cast<double>(U) - SX) : CubicWeight(static_cast<double>(U) - SX);
                        const double WY = bLanczos ? LanczosWeight(static_cast<double>(V) - SY) : CubicWeight(static_cast<double>(V) - SY);
                        const double W = WX * WY;
                        if (FMath::IsNearlyZero(W)) continue;
                        const FColor C = GetPixelBGRA(SX, SY);
                        SumR += C.R * W; SumG += C.G * W; SumB += C.B * W; SumA += C.A * W; SumW += W;
                    }
                }
                SampledColor = FMath::IsNearlyZero(SumW) ? GetPixelBGRA(X0, Y0) :
                    FColor(ClampColorChannel(SumR / SumW), ClampColorChannel(SumG / SumW),
                           ClampColorChannel(SumB / SumW), ClampColorChannel(SumA / SumW));
            }
            else
            {
                const FColor C00 = GetPixelBGRA(X0, Y0);
                const FColor C10 = GetPixelBGRA(X1, Y0);
                const FColor C01 = GetPixelBGRA(X0, Y1);
                const FColor C11 = GetPixelBGRA(X1, Y1);
                SampledColor.R = static_cast<uint8>(FMath::Lerp(FMath::Lerp((float)C00.R, (float)C10.R, FracX), FMath::Lerp((float)C01.R, (float)C11.R, FracX), FracY));
                SampledColor.G = static_cast<uint8>(FMath::Lerp(FMath::Lerp((float)C00.G, (float)C10.G, FracX), FMath::Lerp((float)C01.G, (float)C11.G, FracX), FracY));
                SampledColor.B = static_cast<uint8>(FMath::Lerp(FMath::Lerp((float)C00.B, (float)C10.B, FracX), FMath::Lerp((float)C01.B, (float)C11.B, FracX), FracY));
                SampledColor.A = static_cast<uint8>(FMath::Lerp(FMath::Lerp((float)C00.A, (float)C10.A, FracX), FMath::Lerp((float)C01.A, (float)C11.A, FracX), FracY));
            }
            const int32 DstIndex = (Y * NewWidth + X) * 4;
            DstMipData[DstIndex + 0] = SampledColor.B;
            DstMipData[DstIndex + 1] = SampledColor.G;
            DstMipData[DstIndex + 2] = SampledColor.R;
            DstMipData[DstIndex + 3] = SampledColor.A;
        }
    }

    SourceTexture->Source.UnlockMip(0);
    NewTexture->Source.UnlockMip(0);
    NewTexture->UpdateResource();
    if (bSave)
    {
        FAssetRegistryModule::AssetCreated(NewTexture);
        McpSafeAssetSave(NewTexture);
    }
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Texture resized to %dx%d"), NewWidth, NewHeight));
    Response->SetStringField(TEXT("assetPath"), Path / Name);
    Response->SetStringField(TEXT("filterMethod"), FilterMethod);
    return Response;
}
}
