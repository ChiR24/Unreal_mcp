#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
UTexture2D* ResolveGeometryTexture(const FString& TexturePath, FString& OutResolvedPath)
{
    FString SafePath = SanitizeProjectRelativePath(TexturePath);
    if (SafePath.IsEmpty()) return nullptr;
    // A bare package path names the asset after its package.
    if (!SafePath.Contains(TEXT(".")))
    {
        SafePath += TEXT(".") + FPackageName::GetLongPackageAssetName(SafePath);
    }
    UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *SafePath);
    if (Texture) OutResolvedPath = Texture->GetPathName();
    return Texture;
}

bool SampleTextureLuminance(UTexture2D* Texture, double U, double V, double& OutLuminance)
{
    if (!Texture || !Texture->Source.IsValid())
    {
        return false;
    }

    const int32 Width = Texture->Source.GetSizeX();
    const int32 Height = Texture->Source.GetSizeY();
    if (Width <= 0 || Height <= 0)
    {
        return false;
    }

    TArray64<uint8> MipData;
    if (!Texture->Source.GetMipData(MipData, 0))
    {
        return false;
    }

    const int32 X = FMath::Clamp(FMath::RoundToInt(FMath::Clamp(U, 0.0, 1.0) * static_cast<double>(Width - 1)), 0, Width - 1);
    const int32 Y = FMath::Clamp(FMath::RoundToInt(FMath::Clamp(V, 0.0, 1.0) * static_cast<double>(Height - 1)), 0, Height - 1);
    const int64 PixelIndex = static_cast<int64>(Y) * static_cast<int64>(Width) + static_cast<int64>(X);

    switch (Texture->Source.GetFormat())
    {
        case TSF_BGRA8:
        {
            const int64 Offset = PixelIndex * 4;
            if (MipData.Num() < Offset + 4) return false;
            const double B = static_cast<double>(MipData[Offset + 0]) / 255.0;
            const double G = static_cast<double>(MipData[Offset + 1]) / 255.0;
            const double R = static_cast<double>(MipData[Offset + 2]) / 255.0;
            OutLuminance = R * 0.2126 + G * 0.7152 + B * 0.0722;
            return true;
        }
        case TSF_G8:
        {
            if (MipData.Num() <= PixelIndex) return false;
            OutLuminance = static_cast<double>(MipData[PixelIndex]) / 255.0;
            return true;
        }
        default:
            return false;
    }
}
} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
