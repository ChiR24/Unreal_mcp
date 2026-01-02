// Copyright (c) 2025 MCP Automation Bridge Contributors
// SPDX-License-Identifier: MIT
//
// McpAutomationBridge_TextureHandlers.cpp
// Phase 9: Texture Generation & Processing
//
// Implements procedural texture creation, processing, and settings management.

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Factories/Texture2DFactoryNew.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "HAL/PlatformFileManager.h"
#include "EditorAssetLibrary.h"
// TextureCompressorModule removed in UE 5.7
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"

// Helper macro for error responses
#define TEXTURE_ERROR_RESPONSE(Msg) \
    Response->SetBoolField(TEXT("success"), false); \
    Response->SetStringField(TEXT("error"), Msg); \
    return Response;

// Use consolidated JSON helpers from McpAutomationBridgeHelpers.h
// Aliases for backward compatibility with existing code in this file
#define GetNumberFieldSafe GetJsonNumberField
#define GetBoolFieldSafe GetJsonBoolField
#define GetStringFieldSafe GetJsonStringField

// Helper to normalize asset path
static FString NormalizeTexturePath(const FString& Path)
{
    FString Normalized = Path;
    Normalized.ReplaceInline(TEXT("/Content"), TEXT("/Game"));
    Normalized.ReplaceInline(TEXT("\\"), TEXT("/"));
    
    // Remove trailing slashes
    while (Normalized.EndsWith(TEXT("/")))
    {
        Normalized.LeftChopInline(1);
    }
    
    return Normalized;
}

// Helper to save texture asset using EditorAssetLibrary (safer than low-level UPackage::SavePackage)
static bool SaveTextureAsset(UTexture2D* Texture)
{
    if (!Texture)
        return false;
    
    // UE 5.7+ Fix: Do not immediately save newly created assets to disk.
    // Saving immediately causes bulkdata corruption and crashes.
    // Instead, mark the package dirty and notify the asset registry.
    Texture->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(Texture);
    return true;
}

// Helper to create a texture with given dimensions
static UTexture2D* CreateEmptyTexture(const FString& PackagePath, const FString& TextureName, int32 Width, int32 Height, bool bHDR)
{
    FString FullPath = PackagePath / TextureName;
    FullPath = NormalizeTexturePath(FullPath);
    
    // Create package
    FString PackageFileName = FPackageName::LongPackageNameToFilename(FullPath, FPackageName::GetAssetPackageExtension());
    UPackage* Package = CreatePackage(*FullPath);
    if (!Package)
    {
        return nullptr;
    }
    
    // Create texture
    EPixelFormat Format = bHDR ? PF_FloatRGBA : PF_B8G8R8A8;
    UTexture2D* NewTexture = NewObject<UTexture2D>(Package, UTexture2D::StaticClass(), FName(*TextureName), RF_Public | RF_Standalone);
    
    // Initialize platform data
    NewTexture->SetPlatformData(new FTexturePlatformData());
    NewTexture->GetPlatformData()->SizeX = Width;
    NewTexture->GetPlatformData()->SizeY = Height;
    NewTexture->GetPlatformData()->PixelFormat = Format;
    
    // Add mip 0
    int32 NumBlocksX = Width / GPixelFormats[Format].BlockSizeX;
    int32 NumBlocksY = Height / GPixelFormats[Format].BlockSizeY;
    FTexture2DMipMap* Mip = new FTexture2DMipMap();
    NewTexture->GetPlatformData()->Mips.Add(Mip);
    Mip->SizeX = Width;
    Mip->SizeY = Height;
    
    // Allocate and initialize pixel data
    int32 BytesPerPixel = bHDR ? 16 : 4; // FloatRGBA = 16, BGRA8 = 4
    int32 DataSize = Width * Height * BytesPerPixel;
    Mip->BulkData.Lock(LOCK_READ_WRITE);
    void* TextureData = Mip->BulkData.Realloc(DataSize);
    FMemory::Memzero(TextureData, DataSize);
    Mip->BulkData.Unlock();
    
    NewTexture->Source.Init(Width, Height, 1, 1, bHDR ? TSF_RGBA16F : TSF_BGRA8);
    
    // Set properties
    NewTexture->SRGB = !bHDR;
    NewTexture->CompressionSettings = bHDR ? TC_HDR : TC_Default;
    NewTexture->MipGenSettings = TMGS_FromTextureGroup;
    NewTexture->LODGroup = TEXTUREGROUP_World;
    
    NewTexture->UpdateResource();
    Package->MarkPackageDirty();
    
    return NewTexture;
}

// Simple Perlin noise implementation
static float Noise2D(float X, float Y, int32 Seed)
{
    // Simple gradient noise approximation
    int32 IntX = FMath::FloorToInt(X);
    int32 IntY = FMath::FloorToInt(Y);
    float FracX = X - IntX;
    float FracY = Y - IntY;
    
    // Hash function
    auto Hash = [Seed](int32 X, int32 Y) -> float {
        int32 N = X + Y * 57 + Seed * 131;
        N = (N << 13) ^ N;
        return (1.0f - ((N * (N * N * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);
    };
    
    // Bilinear interpolation
    float V00 = Hash(IntX, IntY);
    float V10 = Hash(IntX + 1, IntY);
    float V01 = Hash(IntX, IntY + 1);
    float V11 = Hash(IntX + 1, IntY + 1);
    
    // Smoothstep
    float SmoothX = FracX * FracX * (3.0f - 2.0f * FracX);
    float SmoothY = FracY * FracY * (3.0f - 2.0f * FracY);
    
    float I0 = FMath::Lerp(V00, V10, SmoothX);
    float I1 = FMath::Lerp(V01, V11, SmoothX);
    
    return FMath::Lerp(I0, I1, SmoothY);
}

// FBM noise for octaves
static float FBMNoise(float X, float Y, int32 Octaves, float Persistence, float Lacunarity, int32 Seed)
{
    float Total = 0.0f;
    float Amplitude = 1.0f;
    float Frequency = 1.0f;
    float MaxValue = 0.0f;
    
    for (int32 i = 0; i < Octaves; i++)
    {
        Total += Noise2D(X * Frequency, Y * Frequency, Seed + i) * Amplitude;
        MaxValue += Amplitude;
        Amplitude *= Persistence;
        Frequency *= Lacunarity;
    }
    
    return Total / MaxValue;
}

TSharedPtr<FJsonObject> UMcpAutomationBridgeSubsystem::HandleManageTextureAction(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    
    FString SubAction = GetStringFieldSafe(Params, TEXT("subAction"), TEXT(""));
    
    // ===== PROCEDURAL GENERATION =====
    
    if (SubAction == TEXT("create_noise_texture"))
    {
        FString Name = GetStringFieldSafe(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeTexturePath(GetStringFieldSafe(Params, TEXT("path"), TEXT("/Game/Textures")));
        FString NoiseType = GetStringFieldSafe(Params, TEXT("noiseType"), TEXT("Perlin"));
        int32 Width = static_cast<int32>(GetNumberFieldSafe(Params, TEXT("width"), 1024));
        int32 Height = static_cast<int32>(GetNumberFieldSafe(Params, TEXT("height"), 1024));
        float Scale = static_cast<float>(GetNumberFieldSafe(Params, TEXT("scale"), 1.0));
        int32 Octaves = static_cast<int32>(GetNumberFieldSafe(Params, TEXT("octaves"), 4));
        float Persistence = static_cast<float>(GetNumberFieldSafe(Params, TEXT("persistence"), 0.5));
        float Lacunarity = static_cast<float>(GetNumberFieldSafe(Params, TEXT("lacunarity"), 2.0));
        int32 Seed = static_cast<int32>(GetNumberFieldSafe(Params, TEXT("seed"), 0));
        bool bSeamless = GetBoolFieldSafe(Params, TEXT("seamless"), false);
        bool bHDR = GetBoolFieldSafe(Params, TEXT("hdr"), false);
        bool bSave = GetBoolFieldSafe(Params, TEXT("save"), true);
        
        if (Name.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Name is required"));
        }
        
        // Create texture
        UTexture2D* NewTexture = CreateEmptyTexture(Path, Name, Width, Height, bHDR);
        if (!NewTexture)
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Failed to create texture"));
        }
        
        // Lock source data and fill with noise
        uint8* MipData = NewTexture->Source.LockMip(0);
        if (!MipData)
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Failed to lock texture mip data"));
        }
        
        for (int32 Y = 0; Y < Height; Y++)
        {
            for (int32 X = 0; X < Width; X++)
            {
                float NX = static_cast<float>(X) / static_cast<float>(Width) * Scale;
                float NY = static_cast<float>(Y) / static_cast<float>(Height) * Scale;
                
                // Seamless tiling using domain wrapping
                float NoiseValue;
                if (bSeamless)
                {
                    float Angle1 = NX * PI * 2.0f;
                    float Angle2 = NY * PI * 2.0f;
                    float NX3D = FMath::Cos(Angle1);
                    float NY3D = FMath::Sin(Angle1);
                    float NZ3D = FMath::Cos(Angle2);
                    float NW3D = FMath::Sin(Angle2);
                    NoiseValue = FBMNoise(NX3D + NZ3D, NY3D + NW3D, Octaves, Persistence, Lacunarity, Seed);
                }
                else
                {
                    NoiseValue = FBMNoise(NX, NY, Octaves, Persistence, Lacunarity, Seed);
                }
                
                // Normalize to 0-1 range
                NoiseValue = (NoiseValue + 1.0f) * 0.5f;
                NoiseValue = FMath::Clamp(NoiseValue, 0.0f, 1.0f);
                
                // Write pixel data (BGRA8 format)
                int32 PixelIndex = (Y * Width + X) * 4;
                uint8 ByteValue = static_cast<uint8>(NoiseValue * 255.0f);
                MipData[PixelIndex + 0] = ByteValue; // B
                MipData[PixelIndex + 1] = ByteValue; // G
                MipData[PixelIndex + 2] = ByteValue; // R
                MipData[PixelIndex + 3] = 255;       // A
            }
        }
        
        NewTexture->Source.UnlockMip(0);
        NewTexture->UpdateResource();
        
        if (bSave)
        {
            FAssetRegistryModule::AssetCreated(NewTexture);
            SaveTextureAsset(NewTexture);
        }
        
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Noise texture '%s' created"), *Name));
        Response->SetStringField(TEXT("assetPath"), Path / Name);
        return Response;
    }
    
    if (SubAction == TEXT("create_gradient_texture"))
    {
        FString Name = GetStringFieldSafe(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeTexturePath(GetStringFieldSafe(Params, TEXT("path"), TEXT("/Game/Textures")));
        FString GradientType = GetStringFieldSafe(Params, TEXT("gradientType"), TEXT("Linear"));
        int32 Width = static_cast<int32>(GetNumberFieldSafe(Params, TEXT("width"), 1024));
        int32 Height = static_cast<int32>(GetNumberFieldSafe(Params, TEXT("height"), 1024));
        float Angle = static_cast<float>(GetNumberFieldSafe(Params, TEXT("angle"), 0.0));
        float CenterX = static_cast<float>(GetNumberFieldSafe(Params, TEXT("centerX"), 0.5));
        float CenterY = static_cast<float>(GetNumberFieldSafe(Params, TEXT("centerY"), 0.5));
        float Radius = static_cast<float>(GetNumberFieldSafe(Params, TEXT("radius"), 0.5));
        bool bHDR = GetBoolFieldSafe(Params, TEXT("hdr"), false);
        bool bSave = GetBoolFieldSafe(Params, TEXT("save"), true);
        
        // Get colors
        FLinearColor StartColor(0, 0, 0, 1);
        FLinearColor EndColor(1, 1, 1, 1);
        
        if (Params->HasField(TEXT("startColor")))
        {
            const TSharedPtr<FJsonObject>* StartColorObj;
            if (Params->TryGetObjectField(TEXT("startColor"), StartColorObj))
            {
                StartColor.R = static_cast<float>(GetNumberFieldSafe(*StartColorObj, TEXT("r"), 0.0));
                StartColor.G = static_cast<float>(GetNumberFieldSafe(*StartColorObj, TEXT("g"), 0.0));
                StartColor.B = static_cast<float>(GetNumberFieldSafe(*StartColorObj, TEXT("b"), 0.0));
                StartColor.A = static_cast<float>(GetNumberFieldSafe(*StartColorObj, TEXT("a"), 1.0));
            }
        }
        
        if (Params->HasField(TEXT("endColor")))
        {
            const TSharedPtr<FJsonObject>* EndColorObj;
            if (Params->TryGetObjectField(TEXT("endColor"), EndColorObj))
            {
                EndColor.R = static_cast<float>(GetNumberFieldSafe(*EndColorObj, TEXT("r"), 1.0));
                EndColor.G = static_cast<float>(GetNumberFieldSafe(*EndColorObj, TEXT("g"), 1.0));
                EndColor.B = static_cast<float>(GetNumberFieldSafe(*EndColorObj, TEXT("b"), 1.0));
                EndColor.A = static_cast<float>(GetNumberFieldSafe(*EndColorObj, TEXT("a"), 1.0));
            }
        }
        
        if (Name.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Name is required"));
        }
        
        UTexture2D* NewTexture = CreateEmptyTexture(Path, Name, Width, Height, bHDR);
        if (!NewTexture)
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Failed to create texture"));
        }
        
        uint8* MipData = NewTexture->Source.LockMip(0);
        if (!MipData)
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Failed to lock texture mip data"));
        }
        
        // Convert angle to radians for linear gradient
        float AngleRad = FMath::DegreesToRadians(Angle);
        FVector2D GradientDir(FMath::Cos(AngleRad), FMath::Sin(AngleRad));
        
        for (int32 Y = 0; Y < Height; Y++)
        {
            for (int32 X = 0; X < Width; X++)
            {
                float NX = static_cast<float>(X) / static_cast<float>(Width);
                float NY = static_cast<float>(Y) / static_cast<float>(Height);
                
                float T = 0.0f;
                
                if (GradientType == TEXT("Linear"))
                {
                    // Project onto gradient direction
                    T = NX * GradientDir.X + NY * GradientDir.Y;
                    T = FMath::Clamp(T, 0.0f, 1.0f);
                }
                else if (GradientType == TEXT("Radial"))
                {
                    float DX = NX - CenterX;
                    float DY = NY - CenterY;
                    float Dist = FMath::Sqrt(DX * DX + DY * DY);
                    T = FMath::Clamp(Dist / Radius, 0.0f, 1.0f);
                }
                else if (GradientType == TEXT("Angular"))
                {
                    float DX = NX - CenterX;
                    float DY = NY - CenterY;
                    float AngleVal = FMath::Atan2(DY, DX);
                    T = (AngleVal + PI) / (2.0f * PI);
                    T = FMath::Clamp(T, 0.0f, 1.0f);
                }
                
                // Interpolate color
                FLinearColor Color = FMath::Lerp(StartColor, EndColor, T);
                
                // Write pixel
                int32 PixelIndex = (Y * Width + X) * 4;
                MipData[PixelIndex + 0] = static_cast<uint8>(Color.B * 255.0f); // B
                MipData[PixelIndex + 1] = static_cast<uint8>(Color.G * 255.0f); // G
                MipData[PixelIndex + 2] = static_cast<uint8>(Color.R * 255.0f); // R
                MipData[PixelIndex + 3] = static_cast<uint8>(Color.A * 255.0f); // A
            }
        }
        
        NewTexture->Source.UnlockMip(0);
        NewTexture->UpdateResource();
        
        if (bSave)
        {
            FAssetRegistryModule::AssetCreated(NewTexture);
            SaveTextureAsset(NewTexture);
        }
        
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Gradient texture '%s' created"), *Name));
        Response->SetStringField(TEXT("assetPath"), Path / Name);
        return Response;
    }
    
    if (SubAction == TEXT("create_pattern_texture"))
    {
        FString Name = GetStringFieldSafe(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeTexturePath(GetStringFieldSafe(Params, TEXT("path"), TEXT("/Game/Textures")));
        FString PatternType = GetStringFieldSafe(Params, TEXT("patternType"), TEXT("Checker"));
        int32 Width = static_cast<int32>(GetNumberFieldSafe(Params, TEXT("width"), 1024));
        int32 Height = static_cast<int32>(GetNumberFieldSafe(Params, TEXT("height"), 1024));
        int32 TilesX = static_cast<int32>(GetNumberFieldSafe(Params, TEXT("tilesX"), 8));
        int32 TilesY = static_cast<int32>(GetNumberFieldSafe(Params, TEXT("tilesY"), 8));
        float LineWidth = static_cast<float>(GetNumberFieldSafe(Params, TEXT("lineWidth"), 0.02));
        float BrickRatio = static_cast<float>(GetNumberFieldSafe(Params, TEXT("brickRatio"), 2.0));
        float Offset = static_cast<float>(GetNumberFieldSafe(Params, TEXT("offset"), 0.5));
        bool bSave = GetBoolFieldSafe(Params, TEXT("save"), true);
        
        // Get colors
        FLinearColor PrimaryColor(1, 1, 1, 1);
        FLinearColor SecondaryColor(0, 0, 0, 1);
        
        if (Params->HasField(TEXT("primaryColor")))
        {
            const TSharedPtr<FJsonObject>* ColorObj;
            if (Params->TryGetObjectField(TEXT("primaryColor"), ColorObj))
            {
                PrimaryColor.R = static_cast<float>(GetNumberFieldSafe(*ColorObj, TEXT("r"), 1.0));
                PrimaryColor.G = static_cast<float>(GetNumberFieldSafe(*ColorObj, TEXT("g"), 1.0));
                PrimaryColor.B = static_cast<float>(GetNumberFieldSafe(*ColorObj, TEXT("b"), 1.0));
                PrimaryColor.A = static_cast<float>(GetNumberFieldSafe(*ColorObj, TEXT("a"), 1.0));
            }
        }
        
        if (Params->HasField(TEXT("secondaryColor")))
        {
            const TSharedPtr<FJsonObject>* ColorObj;
            if (Params->TryGetObjectField(TEXT("secondaryColor"), ColorObj))
            {
                SecondaryColor.R = static_cast<float>(GetNumberFieldSafe(*ColorObj, TEXT("r"), 0.0));
                SecondaryColor.G = static_cast<float>(GetNumberFieldSafe(*ColorObj, TEXT("g"), 0.0));
                SecondaryColor.B = static_cast<float>(GetNumberFieldSafe(*ColorObj, TEXT("b"), 0.0));
                SecondaryColor.A = static_cast<float>(GetNumberFieldSafe(*ColorObj, TEXT("a"), 1.0));
            }
        }
        
        if (Name.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Name is required"));
        }
        
        UTexture2D* NewTexture = CreateEmptyTexture(Path, Name, Width, Height, false);
        if (!NewTexture)
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Failed to create texture"));
        }
        
        uint8* MipData = NewTexture->Source.LockMip(0);
        if (!MipData)
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Failed to lock texture mip data"));
        }
        
        for (int32 Y = 0; Y < Height; Y++)
        {
            for (int32 X = 0; X < Width; X++)
            {
                float NX = static_cast<float>(X) / static_cast<float>(Width);
                float NY = static_cast<float>(Y) / static_cast<float>(Height);
                
                bool bUsePrimary = true;
                
                if (PatternType == TEXT("Checker"))
                {
                    int32 CellX = static_cast<int32>(NX * TilesX);
                    int32 CellY = static_cast<int32>(NY * TilesY);
                    bUsePrimary = ((CellX + CellY) % 2) == 0;
                }
                else if (PatternType == TEXT("Grid"))
                {
                    float CellWidth = 1.0f / TilesX;
                    float CellHeight = 1.0f / TilesY;
                    float LocalX = FMath::Fmod(NX, CellWidth) / CellWidth;
                    float LocalY = FMath::Fmod(NY, CellHeight) / CellHeight;
                    bUsePrimary = (LocalX > LineWidth && LocalX < (1.0f - LineWidth) &&
                                   LocalY > LineWidth && LocalY < (1.0f - LineWidth));
                }
                else if (PatternType == TEXT("Brick"))
                {
                    float BrickHeight = 1.0f / TilesY;
                    int32 Row = static_cast<int32>(NY * TilesY);
                    float RowOffset = (Row % 2 == 1) ? Offset / TilesX : 0.0f;
                    float AdjustedX = FMath::Fmod(NX + RowOffset, 1.0f);
                    
                    float BrickWidth = BrickRatio / TilesX;
                    float LocalX = FMath::Fmod(AdjustedX, BrickWidth) / BrickWidth;
                    float LocalY = FMath::Fmod(NY, BrickHeight) / BrickHeight;
                    
                    bUsePrimary = (LocalX > LineWidth && LocalX < (1.0f - LineWidth) &&
                                   LocalY > LineWidth && LocalY < (1.0f - LineWidth));
                }
                else if (PatternType == TEXT("Stripes"))
                {
                    int32 StripeIndex = static_cast<int32>(NX * TilesX);
                    bUsePrimary = (StripeIndex % 2) == 0;
                }
                else if (PatternType == TEXT("Dots"))
                {
                    float CellWidth = 1.0f / TilesX;
                    float CellHeight = 1.0f / TilesY;
                    float CenterLocalX = FMath::Fmod(NX, CellWidth) / CellWidth - 0.5f;
                    float CenterLocalY = FMath::Fmod(NY, CellHeight) / CellHeight - 0.5f;
                    float Dist = FMath::Sqrt(CenterLocalX * CenterLocalX + CenterLocalY * CenterLocalY);
                    bUsePrimary = Dist < 0.3f;
                }
                
                FLinearColor Color = bUsePrimary ? PrimaryColor : SecondaryColor;
                
                int32 PixelIndex = (Y * Width + X) * 4;
                MipData[PixelIndex + 0] = static_cast<uint8>(Color.B * 255.0f);
                MipData[PixelIndex + 1] = static_cast<uint8>(Color.G * 255.0f);
                MipData[PixelIndex + 2] = static_cast<uint8>(Color.R * 255.0f);
                MipData[PixelIndex + 3] = static_cast<uint8>(Color.A * 255.0f);
            }
        }
        
        NewTexture->Source.UnlockMip(0);
        NewTexture->UpdateResource();
        
        if (bSave)
        {
            FAssetRegistryModule::AssetCreated(NewTexture);
            SaveTextureAsset(NewTexture);
        }
        
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Pattern texture '%s' created"), *Name));
        Response->SetStringField(TEXT("assetPath"), Path / Name);
        return Response;
    }
    
    if (SubAction == TEXT("create_normal_from_height"))
    {
        FString SourceTexture = NormalizeTexturePath(GetStringFieldSafe(Params, TEXT("sourceTexture"), TEXT("")));
        FString Name = GetStringFieldSafe(Params, TEXT("name"), TEXT(""));
        FString Path = GetStringFieldSafe(Params, TEXT("path"), TEXT(""));
        float Strength = static_cast<float>(GetNumberFieldSafe(Params, TEXT("strength"), 1.0));
        FString Algorithm = GetStringFieldSafe(Params, TEXT("algorithm"), TEXT("Sobel"));
        bool bFlipY = GetBoolFieldSafe(Params, TEXT("flipY"), false);
        bool bSave = GetBoolFieldSafe(Params, TEXT("save"), true);
        
        if (SourceTexture.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("sourceTexture is required"));
        }
        
        // Load source texture
        UTexture2D* HeightMap = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *SourceTexture));
        if (!HeightMap)
        {
            TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Failed to load height map: %s"), *SourceTexture));
        }
        
        // Get dimensions from source
        int32 Width = HeightMap->GetSizeX();
        int32 Height = HeightMap->GetSizeY();
        
        // Generate output name and path if not specified
        if (Name.IsEmpty())
        {
            Name = FPaths::GetBaseFilename(SourceTexture) + TEXT("_N");
        }
        if (Path.IsEmpty())
        {
            Path = FPaths::GetPath(SourceTexture);
        }
        Path = NormalizeTexturePath(Path);
        
        // Create output texture
        UTexture2D* NormalMap = CreateEmptyTexture(Path, Name, Width, Height, false);
        if (!NormalMap)
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Failed to create normal map texture"));
        }
        
        // Set normal map properties
        NormalMap->SRGB = false;
        NormalMap->CompressionSettings = TC_Normalmap;
        
        // Read height data (simplified - assuming we can access texture data)
        TArray<float> HeightData;
        HeightData.SetNum(Width * Height);
        
        // Lock source texture for reading
        FTexture2DMipMap& HeightMip = HeightMap->GetPlatformData()->Mips[0];
        const uint8* HeightPixels = static_cast<const uint8*>(HeightMip.BulkData.LockReadOnly());
        
        for (int32 i = 0; i < Width * Height; i++)
        {
            // Assume grayscale or use luminance
            HeightData[i] = static_cast<float>(HeightPixels[i * 4 + 2]) / 255.0f; // R channel
        }
        HeightMip.BulkData.Unlock();
        
        // Generate normal map
        uint8* NormalData = NormalMap->Source.LockMip(0);
        
        for (int32 Y = 0; Y < Height; Y++)
        {
            for (int32 X = 0; X < Width; X++)
            {
                // Sample neighboring heights with wrap
                auto SampleHeight = [&](int32 SX, int32 SY) -> float {
                    SX = (SX + Width) % Width;
                    SY = (SY + Height) % Height;
                    return HeightData[SY * Width + SX];
                };
                
                float DX, DY;
                
                if (Algorithm == TEXT("Sobel"))
                {
                    // Sobel operator
                    DX = (SampleHeight(X - 1, Y - 1) * -1.0f + SampleHeight(X - 1, Y) * -2.0f + SampleHeight(X - 1, Y + 1) * -1.0f +
                          SampleHeight(X + 1, Y - 1) * 1.0f + SampleHeight(X + 1, Y) * 2.0f + SampleHeight(X + 1, Y + 1) * 1.0f);
                    DY = (SampleHeight(X - 1, Y - 1) * -1.0f + SampleHeight(X, Y - 1) * -2.0f + SampleHeight(X + 1, Y - 1) * -1.0f +
                          SampleHeight(X - 1, Y + 1) * 1.0f + SampleHeight(X, Y + 1) * 2.0f + SampleHeight(X + 1, Y + 1) * 1.0f);
                }
                else
                {
                    // Simple finite difference
                    DX = SampleHeight(X + 1, Y) - SampleHeight(X - 1, Y);
                    DY = SampleHeight(X, Y + 1) - SampleHeight(X, Y - 1);
                }
                
                // Apply strength
                DX *= Strength;
                DY *= Strength;
                
                // Flip Y if needed (DirectX vs OpenGL)
                if (bFlipY)
                {
                    DY = -DY;
                }
                
                // Create normal vector
                FVector Normal(-DX, -DY, 1.0f);
                Normal.Normalize();
                
                // Convert to 0-1 range
                int32 PixelIndex = (Y * Width + X) * 4;
                NormalData[PixelIndex + 0] = static_cast<uint8>((Normal.Z * 0.5f + 0.5f) * 255.0f); // B = Z
                NormalData[PixelIndex + 1] = static_cast<uint8>((Normal.Y * 0.5f + 0.5f) * 255.0f); // G = Y
                NormalData[PixelIndex + 2] = static_cast<uint8>((Normal.X * 0.5f + 0.5f) * 255.0f); // R = X
                NormalData[PixelIndex + 3] = 255;
            }
        }
        
        NormalMap->Source.UnlockMip(0);
        NormalMap->UpdateResource();
        
        if (bSave)
        {
            FAssetRegistryModule::AssetCreated(NormalMap);
            SaveTextureAsset(NormalMap);
        }
        
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), TEXT("Normal map created from height map"));
        Response->SetStringField(TEXT("assetPath"), Path / Name);
        return Response;
    }
    
    if (SubAction == TEXT("create_ao_from_mesh"))
    {
        // AO baking is complex and typically requires GPU rendering
        // This is a placeholder that would need proper implementation with scene capture
        FString MeshPath = NormalizeTexturePath(GetStringFieldSafe(Params, TEXT("meshPath"), TEXT("")));
        FString Name = GetStringFieldSafe(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeTexturePath(GetStringFieldSafe(Params, TEXT("path"), TEXT("/Game/Textures")));
        int32 Width = static_cast<int32>(GetNumberFieldSafe(Params, TEXT("width"), 1024));
        int32 Height = static_cast<int32>(GetNumberFieldSafe(Params, TEXT("height"), 1024));
        
        if (MeshPath.IsEmpty() || Name.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("meshPath and name are required"));
        }
        
        // Create a white texture as placeholder
        // Real implementation would use ray tracing or scene capture
        UTexture2D* AOTexture = CreateEmptyTexture(Path, Name, Width, Height, false);
        if (!AOTexture)
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Failed to create AO texture"));
        }
        
        uint8* MipData = AOTexture->Source.LockMip(0);
        FMemory::Memset(MipData, 255, Width * Height * 4); // White (no occlusion)
        AOTexture->Source.UnlockMip(0);
        AOTexture->UpdateResource();
        
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("AO texture '%s' created (placeholder - real AO baking requires GPU)"), *Name));
        Response->SetStringField(TEXT("assetPath"), Path / Name);
        return Response;
    }
    
    // ===== TEXTURE SETTINGS =====
    
    if (SubAction == TEXT("set_compression_settings"))
    {
        FString AssetPath = NormalizeTexturePath(GetStringFieldSafe(Params, TEXT("assetPath"), TEXT("")));
        FString CompressionSettingsStr = GetStringFieldSafe(Params, TEXT("compressionSettings"), TEXT("TC_Default"));
        bool bSave = GetBoolFieldSafe(Params, TEXT("save"), true);
        
        if (AssetPath.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("assetPath is required"));
        }
        
        UTexture2D* Texture = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *AssetPath));
        if (!Texture)
        {
            TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Failed to load texture: %s"), *AssetPath));
        }
        
        // Map string to enum
        TextureCompressionSettings NewSetting = TC_Default;
        if (CompressionSettingsStr == TEXT("TC_Normalmap")) NewSetting = TC_Normalmap;
        else if (CompressionSettingsStr == TEXT("TC_Masks")) NewSetting = TC_Masks;
        else if (CompressionSettingsStr == TEXT("TC_Grayscale")) NewSetting = TC_Grayscale;
        else if (CompressionSettingsStr == TEXT("TC_Displacementmap")) NewSetting = TC_Displacementmap;
        else if (CompressionSettingsStr == TEXT("TC_VectorDisplacementmap")) NewSetting = TC_VectorDisplacementmap;
        else if (CompressionSettingsStr == TEXT("TC_HDR")) NewSetting = TC_HDR;
        else if (CompressionSettingsStr == TEXT("TC_EditorIcon")) NewSetting = TC_EditorIcon;
        else if (CompressionSettingsStr == TEXT("TC_Alpha")) NewSetting = TC_Alpha;
        else if (CompressionSettingsStr == TEXT("TC_DistanceFieldFont")) NewSetting = TC_DistanceFieldFont;
        else if (CompressionSettingsStr == TEXT("TC_HDR_Compressed")) NewSetting = TC_HDR_Compressed;
        else if (CompressionSettingsStr == TEXT("TC_BC7")) NewSetting = TC_BC7;
        
        Texture->CompressionSettings = NewSetting;
        Texture->UpdateResource();
        Texture->MarkPackageDirty();
        
        if (bSave)
        {
            SaveTextureAsset(Texture);
        }
        
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Compression set to %s"), *CompressionSettingsStr));
        return Response;
    }
    
    if (SubAction == TEXT("set_texture_group"))
    {
        FString AssetPath = NormalizeTexturePath(GetStringFieldSafe(Params, TEXT("assetPath"), TEXT("")));
        FString TextureGroup = GetStringFieldSafe(Params, TEXT("textureGroup"), TEXT("TEXTUREGROUP_World"));
        bool bSave = GetBoolFieldSafe(Params, TEXT("save"), true);
        
        if (AssetPath.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("assetPath is required"));
        }
        
        UTexture2D* Texture = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *AssetPath));
        if (!Texture)
        {
            TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Failed to load texture: %s"), *AssetPath));
        }
        
        // Map common texture groups
        ::TextureGroup NewGroup = TEXTUREGROUP_World;
        if (TextureGroup.Contains(TEXT("Character"))) NewGroup = TEXTUREGROUP_Character;
        else if (TextureGroup.Contains(TEXT("Weapon"))) NewGroup = TEXTUREGROUP_Weapon;
        else if (TextureGroup.Contains(TEXT("Vehicle"))) NewGroup = TEXTUREGROUP_Vehicle;
        else if (TextureGroup.Contains(TEXT("Cinematic"))) NewGroup = TEXTUREGROUP_Cinematic;
        else if (TextureGroup.Contains(TEXT("Effects"))) NewGroup = TEXTUREGROUP_Effects;
        else if (TextureGroup.Contains(TEXT("Skybox"))) NewGroup = TEXTUREGROUP_Skybox;
        else if (TextureGroup.Contains(TEXT("UI"))) NewGroup = TEXTUREGROUP_UI;
        else if (TextureGroup.Contains(TEXT("Lightmap"))) NewGroup = TEXTUREGROUP_Lightmap;
        else if (TextureGroup.Contains(TEXT("RenderTarget"))) NewGroup = TEXTUREGROUP_RenderTarget;
        else if (TextureGroup.Contains(TEXT("Bokeh"))) NewGroup = TEXTUREGROUP_Bokeh;
        else if (TextureGroup.Contains(TEXT("Pixels2D"))) NewGroup = TEXTUREGROUP_Pixels2D;
        
        Texture->LODGroup = NewGroup;
        Texture->UpdateResource();
        Texture->MarkPackageDirty();
        
        if (bSave)
        {
            SaveTextureAsset(Texture);
        }
        
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Texture group set to %s"), *TextureGroup));
        return Response;
    }
    
    if (SubAction == TEXT("set_lod_bias"))
    {
        FString AssetPath = NormalizeTexturePath(GetStringFieldSafe(Params, TEXT("assetPath"), TEXT("")));
        int32 LODBias = static_cast<int32>(GetNumberFieldSafe(Params, TEXT("lodBias"), 0));
        bool bSave = GetBoolFieldSafe(Params, TEXT("save"), true);
        
        if (AssetPath.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("assetPath is required"));
        }
        
        UTexture2D* Texture = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *AssetPath));
        if (!Texture)
        {
            TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Failed to load texture: %s"), *AssetPath));
        }
        
        Texture->LODBias = LODBias;
        Texture->UpdateResource();
        Texture->MarkPackageDirty();
        
        if (bSave)
        {
            SaveTextureAsset(Texture);
        }
        
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("LOD bias set to %d"), LODBias));
        return Response;
    }
    
    if (SubAction == TEXT("configure_virtual_texture"))
    {
        FString AssetPath = NormalizeTexturePath(GetStringFieldSafe(Params, TEXT("assetPath"), TEXT("")));
        bool bVirtualTextureStreaming = GetBoolFieldSafe(Params, TEXT("virtualTextureStreaming"), false);
        bool bSave = GetBoolFieldSafe(Params, TEXT("save"), true);
        
        if (AssetPath.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("assetPath is required"));
        }
        
        UTexture2D* Texture = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *AssetPath));
        if (!Texture)
        {
            TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Failed to load texture: %s"), *AssetPath));
        }
        
        Texture->VirtualTextureStreaming = bVirtualTextureStreaming;
        Texture->UpdateResource();
        Texture->MarkPackageDirty();
        
        if (bSave)
        {
            SaveTextureAsset(Texture);
        }
        
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Virtual texture streaming %s"), bVirtualTextureStreaming ? TEXT("enabled") : TEXT("disabled")));
        return Response;
    }
    
    if (SubAction == TEXT("set_streaming_priority"))
    {
        FString AssetPath = NormalizeTexturePath(GetStringFieldSafe(Params, TEXT("assetPath"), TEXT("")));
        bool bNeverStream = GetBoolFieldSafe(Params, TEXT("neverStream"), false);
        bool bSave = GetBoolFieldSafe(Params, TEXT("save"), true);
        
        if (AssetPath.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("assetPath is required"));
        }
        
        UTexture2D* Texture = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *AssetPath));
        if (!Texture)
        {
            TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Failed to load texture: %s"), *AssetPath));
        }
        
        Texture->NeverStream = bNeverStream;
        Texture->UpdateResource();
        Texture->MarkPackageDirty();
        
        if (bSave)
        {
            SaveTextureAsset(Texture);
        }
        
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), TEXT("Streaming priority configured"));
        return Response;
    }
    
    if (SubAction == TEXT("get_texture_info"))
    {
        FString AssetPath = NormalizeTexturePath(GetStringFieldSafe(Params, TEXT("assetPath"), TEXT("")));
        
        if (AssetPath.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("assetPath is required"));
        }
        
        UTexture2D* Texture = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *AssetPath));
        if (!Texture)
        {
            TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Failed to load texture: %s"), *AssetPath));
        }
        
        TSharedPtr<FJsonObject> TextureInfo = MakeShared<FJsonObject>();
        TextureInfo->SetNumberField(TEXT("width"), Texture->GetSizeX());
        TextureInfo->SetNumberField(TEXT("height"), Texture->GetSizeY());
        TextureInfo->SetStringField(TEXT("format"), GPixelFormats[Texture->GetPixelFormat()].Name);
        TextureInfo->SetNumberField(TEXT("mipCount"), Texture->GetNumMips());
        TextureInfo->SetBoolField(TEXT("sRGB"), Texture->SRGB);
        TextureInfo->SetBoolField(TEXT("virtualTextureStreaming"), Texture->VirtualTextureStreaming);
        TextureInfo->SetBoolField(TEXT("neverStream"), Texture->NeverStream);
        TextureInfo->SetNumberField(TEXT("lodBias"), Texture->LODBias);
        
        // Compression settings as string
        FString CompressionStr;
        switch (Texture->CompressionSettings)
        {
            case TC_Default: CompressionStr = TEXT("TC_Default"); break;
            case TC_Normalmap: CompressionStr = TEXT("TC_Normalmap"); break;
            case TC_Masks: CompressionStr = TEXT("TC_Masks"); break;
            case TC_Grayscale: CompressionStr = TEXT("TC_Grayscale"); break;
            case TC_Displacementmap: CompressionStr = TEXT("TC_Displacementmap"); break;
            case TC_VectorDisplacementmap: CompressionStr = TEXT("TC_VectorDisplacementmap"); break;
            case TC_HDR: CompressionStr = TEXT("TC_HDR"); break;
            case TC_EditorIcon: CompressionStr = TEXT("TC_EditorIcon"); break;
            case TC_Alpha: CompressionStr = TEXT("TC_Alpha"); break;
            case TC_DistanceFieldFont: CompressionStr = TEXT("TC_DistanceFieldFont"); break;
            case TC_HDR_Compressed: CompressionStr = TEXT("TC_HDR_Compressed"); break;
            case TC_BC7: CompressionStr = TEXT("TC_BC7"); break;
            default: CompressionStr = TEXT("Unknown"); break;
        }
        TextureInfo->SetStringField(TEXT("compression"), CompressionStr);
        
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), TEXT("Texture info retrieved"));
        Response->SetObjectField(TEXT("textureInfo"), TextureInfo);
        return Response;
    }
    
    // ===== TEXTURE PROCESSING =====
    // These operations require pixel manipulation which is complex
    // Providing stubs with basic implementations
    
    if (SubAction == TEXT("resize_texture") || 
        SubAction == TEXT("adjust_levels") ||
        SubAction == TEXT("adjust_curves") ||
        SubAction == TEXT("blur") ||
        SubAction == TEXT("sharpen") ||
        SubAction == TEXT("invert") ||
        SubAction == TEXT("desaturate") ||
        SubAction == TEXT("channel_pack") ||
        SubAction == TEXT("channel_extract") ||
        SubAction == TEXT("combine_textures"))
    {
        // These require complex pixel processing
        // Real implementation would need to:
        // 1. Lock texture mip data
        // 2. Process pixels according to operation
        // 3. Update texture resource
        
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Action '%s' requires GPU-accelerated processing. Use Material Editor or external tools for best results."), *SubAction));
        Response->SetStringField(TEXT("suggestion"), TEXT("Consider using Substance or Photoshop for complex texture processing, then import the result."));
        return Response;
    }
    
    // Unknown action
    Response->SetBoolField(TEXT("success"), false);
    Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown texture action: %s"), *SubAction));
    return Response;
}

// Wrapper handler that follows the standard signature pattern
bool UMcpAutomationBridgeSubsystem::HandleManageTextureAction(
    const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    // Check if this is a texture action
    if (Action != TEXT("manage_texture"))
    {
        return false; // Not handled
    }
    
    // Call the internal processing function
    TSharedPtr<FJsonObject> Result = HandleManageTextureAction(Payload);
    
    // Send response
    if (Result.IsValid())
    {
        bool bSuccess = Result->HasField(TEXT("success")) && Result->GetBoolField(TEXT("success"));
        FString Message = Result->HasField(TEXT("message")) ? Result->GetStringField(TEXT("message")) : TEXT("");
        
        if (bSuccess)
        {
            SendAutomationResponse(RequestingSocket, RequestId, true, Message, Result);
        }
        else
        {
            FString Error = Result->HasField(TEXT("error")) ? Result->GetStringField(TEXT("error")) : TEXT("Unknown error");
            FString ErrorCode = Result->HasField(TEXT("errorCode")) ? Result->GetStringField(TEXT("errorCode")) : TEXT("TEXTURE_ERROR");
            SendAutomationError(RequestingSocket, RequestId, Error, ErrorCode);
        }
        return true;
    }
    
    SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to process texture action"), TEXT("PROCESSING_FAILED"));
    return true;
}

#undef TEXTURE_ERROR_RESPONSE
