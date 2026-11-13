#include "Domains/Geometry/McpGeometryHandlers.h"

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

FMcpLuminanceSampler::FMcpLuminanceSampler(UTexture2D* Texture)
{
    if (!Texture || !Texture->Source.IsValid())
    {
        return;
    }
    const ETextureSourceFormat Format = Texture->Source.GetFormat();
    const int32 Bytes = Format == TSF_BGRA8 ? 4 : (Format == TSF_G8 ? 1 : 0);
    Width = Texture->Source.GetSizeX();
    Height = Texture->Source.GetSizeY();
    if (Bytes > 0 && Width > 0 && Height > 0 && Texture->Source.GetMipData(MipData, 0))
    {
        BytesPerPixel = Bytes;
    }
}

bool FMcpLuminanceSampler::Sample(double U, double V, double& OutLuminance) const
{
    if (BytesPerPixel == 0)
    {
        return false;
    }
    const int32 X = FMath::Clamp(FMath::RoundToInt(FMath::Clamp(U, 0.0, 1.0) * static_cast<double>(Width - 1)), 0, Width - 1);
    const int32 Y = FMath::Clamp(FMath::RoundToInt(FMath::Clamp(V, 0.0, 1.0) * static_cast<double>(Height - 1)), 0, Height - 1);
    const int64 Offset = (static_cast<int64>(Y) * static_cast<int64>(Width) + static_cast<int64>(X)) * BytesPerPixel;
    if (MipData.Num() < Offset + BytesPerPixel)
    {
        return false;
    }
    if (BytesPerPixel == 1)
    {
        OutLuminance = static_cast<double>(MipData[Offset]) / 255.0;
        return true;
    }
    const double B = static_cast<double>(MipData[Offset + 0]) / 255.0;
    const double G = static_cast<double>(MipData[Offset + 1]) / 255.0;
    const double R = static_cast<double>(MipData[Offset + 2]) / 255.0;
    OutLuminance = R * 0.2126 + G * 0.7152 + B * 0.0722;
    return true;
}
} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
