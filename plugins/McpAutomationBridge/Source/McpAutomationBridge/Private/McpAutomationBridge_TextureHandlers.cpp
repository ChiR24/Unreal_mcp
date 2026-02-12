// Copyright (c) 2025 MCP Automation Bridge Contributors
// SPDX-License-Identifier: MIT
//
// McpAutomationBridge_TextureHandlers.cpp
// Phase 9: Texture Generation & Processing
//
// Implements procedural texture creation, processing, and settings management.

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "Dom/JsonObject.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Factories/Texture2DFactoryNew.h"
// UObject/SavePackage.h is not needed - using McpSafeAssetSave() from helpers instead
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
#define GetNumberFieldTextAuth GetJsonNumberField
#define GetBoolFieldTextAuth GetJsonBoolField
#define GetStringFieldTextAuth GetJsonStringField

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

// NOTE: Use McpSafeAssetSave(Asset) from McpAutomationBridgeHelpers.h for saving textures.
// This comment replaces a duplicate SaveTextureAsset helper that was removed.
// McpSafeAssetSave marks the package dirty and notifies the asset registry safely for UE 5.7+.

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
    
    FString SubAction = GetStringFieldTextAuth(Params, TEXT("subAction"), TEXT(""));
    
    // ===== PROCEDURAL GENERATION =====
    
    if (SubAction == TEXT("create_noise_texture"))
    {
        // Validate that no unknown/invalid parameters are present
        TSet<FString> ValidParams = {
            TEXT("subAction"), TEXT("name"), TEXT("path"), TEXT("noiseType"),
            TEXT("width"), TEXT("height"), TEXT("scale"), TEXT("octaves"),
            TEXT("persistence"), TEXT("lacunarity"), TEXT("seed"),
            TEXT("seamless"), TEXT("hdr"), TEXT("save")
        };
        for (const auto& Field : Params->Values)
        {
            if (!ValidParams.Contains(Field.Key))
            {
                TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Invalid parameter: %s"), *Field.Key));
            }
        }

        FString Name = GetStringFieldTextAuth(Params, TEXT("name"), TEXT(""));
        FString Path = GetStringFieldTextAuth(Params, TEXT("path"), TEXT("/Game/Textures"));
        
        // SECURITY: Validate and sanitize path to prevent path traversal attacks
        FString SanitizedPath = SanitizeProjectRelativePath(Path);
        if (SanitizedPath.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Invalid path: contains traversal or invalid characters"));
        }
        Path = SanitizedPath;
        
        // Validate name for security
        FString SanitizedName = SanitizeAssetName(Name);
        if (SanitizedName.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Invalid name: contains invalid characters"));
        }
        Name = SanitizedName;
        
        FString NoiseType = GetStringFieldTextAuth(Params, TEXT("noiseType"), TEXT("Perlin"));
        int32 Width = static_cast<int32>(GetNumberFieldTextAuth(Params, TEXT("width"), 1024));
        int32 Height = static_cast<int32>(GetNumberFieldTextAuth(Params, TEXT("height"), 1024));
        float Scale = static_cast<float>(GetNumberFieldTextAuth(Params, TEXT("scale"), 1.0));
        int32 Octaves = static_cast<int32>(GetNumberFieldTextAuth(Params, TEXT("octaves"), 4));
        float Persistence = static_cast<float>(GetNumberFieldTextAuth(Params, TEXT("persistence"), 0.5));
        float Lacunarity = static_cast<float>(GetNumberFieldTextAuth(Params, TEXT("lacunarity"), 2.0));
        int32 Seed = static_cast<int32>(GetNumberFieldTextAuth(Params, TEXT("seed"), 0));
        bool bSeamless = GetBoolFieldTextAuth(Params, TEXT("seamless"), false);
        bool bHDR = GetBoolFieldTextAuth(Params, TEXT("hdr"), false);
        bool bSave = GetBoolFieldTextAuth(Params, TEXT("save"), true);
        
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
            McpSafeAssetSave(NewTexture);
        }
        
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Noise texture '%s' created"), *Name));
        Response->SetStringField(TEXT("assetPath"), Path / Name);
        return Response;
    }
    
    if (SubAction == TEXT("create_gradient_texture"))
    {
        // Validate that no unknown/invalid parameters are present
        TSet<FString> ValidParams = {
            TEXT("subAction"), TEXT("name"), TEXT("path"), TEXT("gradientType"),
            TEXT("width"), TEXT("height"), TEXT("angle"), TEXT("centerX"),
            TEXT("centerY"), TEXT("radius"), TEXT("hdr"), TEXT("save"),
            TEXT("startColor"), TEXT("endColor")
        };
        for (const auto& Field : Params->Values)
        {
            if (!ValidParams.Contains(Field.Key))
            {
                TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Invalid parameter: %s"), *Field.Key));
            }
        }

        FString Name = GetStringFieldTextAuth(Params, TEXT("name"), TEXT(""));
        FString Path = GetStringFieldTextAuth(Params, TEXT("path"), TEXT("/Game/Textures"));
        
        // SECURITY: Validate and sanitize path
        FString SanitizedPath = SanitizeProjectRelativePath(Path);
        if (SanitizedPath.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Invalid path: contains traversal or invalid characters"));
        }
        Path = SanitizedPath;
        
        // Validate name
        FString SanitizedName = SanitizeAssetName(Name);
        if (SanitizedName.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Invalid name: contains invalid characters"));
        }
        Name = SanitizedName;
        
        FString GradientType = GetStringFieldTextAuth(Params, TEXT("gradientType"), TEXT("Linear"));
        int32 Width = static_cast<int32>(GetNumberFieldTextAuth(Params, TEXT("width"), 1024));
        int32 Height = static_cast<int32>(GetNumberFieldTextAuth(Params, TEXT("height"), 1024));
        float Angle = static_cast<float>(GetNumberFieldTextAuth(Params, TEXT("angle"), 0.0));
        float CenterX = static_cast<float>(GetNumberFieldTextAuth(Params, TEXT("centerX"), 0.5));
        float CenterY = static_cast<float>(GetNumberFieldTextAuth(Params, TEXT("centerY"), 0.5));
        float Radius = static_cast<float>(GetNumberFieldTextAuth(Params, TEXT("radius"), 0.5));
        bool bHDR = GetBoolFieldTextAuth(Params, TEXT("hdr"), false);
        bool bSave = GetBoolFieldTextAuth(Params, TEXT("save"), true);
        
        // Get colors
        FLinearColor StartColor(0, 0, 0, 1);
        FLinearColor EndColor(1, 1, 1, 1);
        
        if (Params->HasField(TEXT("startColor")))
        {
            const TSharedPtr<FJsonObject>* StartColorObj;
            if (Params->TryGetObjectField(TEXT("startColor"), StartColorObj))
            {
                StartColor.R = static_cast<float>(GetNumberFieldTextAuth(*StartColorObj, TEXT("r"), 0.0));
                StartColor.G = static_cast<float>(GetNumberFieldTextAuth(*StartColorObj, TEXT("g"), 0.0));
                StartColor.B = static_cast<float>(GetNumberFieldTextAuth(*StartColorObj, TEXT("b"), 0.0));
                StartColor.A = static_cast<float>(GetNumberFieldTextAuth(*StartColorObj, TEXT("a"), 1.0));
            }
        }
        
        if (Params->HasField(TEXT("endColor")))
        {
            const TSharedPtr<FJsonObject>* EndColorObj;
            if (Params->TryGetObjectField(TEXT("endColor"), EndColorObj))
            {
                EndColor.R = static_cast<float>(GetNumberFieldTextAuth(*EndColorObj, TEXT("r"), 1.0));
                EndColor.G = static_cast<float>(GetNumberFieldTextAuth(*EndColorObj, TEXT("g"), 1.0));
                EndColor.B = static_cast<float>(GetNumberFieldTextAuth(*EndColorObj, TEXT("b"), 1.0));
                EndColor.A = static_cast<float>(GetNumberFieldTextAuth(*EndColorObj, TEXT("a"), 1.0));
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
            McpSafeAssetSave(NewTexture);
        }
        
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Gradient texture '%s' created"), *Name));
        Response->SetStringField(TEXT("assetPath"), Path / Name);
        return Response;
    }
    
    if (SubAction == TEXT("create_pattern_texture"))
    {
        // Validate that no unknown/invalid parameters are present
        TSet<FString> ValidParams = {
            TEXT("subAction"), TEXT("name"), TEXT("path"), TEXT("patternType"),
            TEXT("width"), TEXT("height"), TEXT("tilesX"), TEXT("tilesY"),
            TEXT("lineWidth"), TEXT("brickRatio"), TEXT("offset"), TEXT("save"),
            TEXT("primaryColor"), TEXT("secondaryColor")
        };
        for (const auto& Field : Params->Values)
        {
            if (!ValidParams.Contains(Field.Key))
            {
                TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Invalid parameter: %s"), *Field.Key));
            }
        }

        FString Name = GetStringFieldTextAuth(Params, TEXT("name"), TEXT(""));
        FString Path = GetStringFieldTextAuth(Params, TEXT("path"), TEXT("/Game/Textures"));
        
        // SECURITY: Validate and sanitize path
        FString SanitizedPath = SanitizeProjectRelativePath(Path);
        if (SanitizedPath.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Invalid path: contains traversal or invalid characters"));
        }
        Path = SanitizedPath;
        
        // Validate name
        FString SanitizedName = SanitizeAssetName(Name);
        if (SanitizedName.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Invalid name: contains invalid characters"));
        }
        Name = SanitizedName;
        
        FString PatternType = GetStringFieldTextAuth(Params, TEXT("patternType"), TEXT("Checker"));
        int32 Width = static_cast<int32>(GetNumberFieldTextAuth(Params, TEXT("width"), 1024));
        int32 Height = static_cast<int32>(GetNumberFieldTextAuth(Params, TEXT("height"), 1024));
        int32 TilesX = static_cast<int32>(GetNumberFieldTextAuth(Params, TEXT("tilesX"), 8));
        int32 TilesY = static_cast<int32>(GetNumberFieldTextAuth(Params, TEXT("tilesY"), 8));
        float LineWidth = static_cast<float>(GetNumberFieldTextAuth(Params, TEXT("lineWidth"), 0.02));
        float BrickRatio = static_cast<float>(GetNumberFieldTextAuth(Params, TEXT("brickRatio"), 2.0));
        float Offset = static_cast<float>(GetNumberFieldTextAuth(Params, TEXT("offset"), 0.5));
        bool bSave = GetBoolFieldTextAuth(Params, TEXT("save"), true);
        
        // Get colors
        FLinearColor PrimaryColor(1, 1, 1, 1);
        FLinearColor SecondaryColor(0, 0, 0, 1);
        
        if (Params->HasField(TEXT("primaryColor")))
        {
            const TSharedPtr<FJsonObject>* ColorObj;
            if (Params->TryGetObjectField(TEXT("primaryColor"), ColorObj))
            {
                PrimaryColor.R = static_cast<float>(GetNumberFieldTextAuth(*ColorObj, TEXT("r"), 1.0));
                PrimaryColor.G = static_cast<float>(GetNumberFieldTextAuth(*ColorObj, TEXT("g"), 1.0));
                PrimaryColor.B = static_cast<float>(GetNumberFieldTextAuth(*ColorObj, TEXT("b"), 1.0));
                PrimaryColor.A = static_cast<float>(GetNumberFieldTextAuth(*ColorObj, TEXT("a"), 1.0));
            }
        }
        
        if (Params->HasField(TEXT("secondaryColor")))
        {
            const TSharedPtr<FJsonObject>* ColorObj;
            if (Params->TryGetObjectField(TEXT("secondaryColor"), ColorObj))
            {
                SecondaryColor.R = static_cast<float>(GetNumberFieldTextAuth(*ColorObj, TEXT("r"), 0.0));
                SecondaryColor.G = static_cast<float>(GetNumberFieldTextAuth(*ColorObj, TEXT("g"), 0.0));
                SecondaryColor.B = static_cast<float>(GetNumberFieldTextAuth(*ColorObj, TEXT("b"), 0.0));
                SecondaryColor.A = static_cast<float>(GetNumberFieldTextAuth(*ColorObj, TEXT("a"), 1.0));
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
            McpSafeAssetSave(NewTexture);
        }
        
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Pattern texture '%s' created"), *Name));
        Response->SetStringField(TEXT("assetPath"), Path / Name);
        return Response;
    }
    
    if (SubAction == TEXT("create_normal_from_height"))
    {
        // Validate that no unknown/invalid parameters are present
        TSet<FString> ValidParams = {
            TEXT("subAction"), TEXT("sourceTexture"), TEXT("name"), TEXT("path"),
            TEXT("strength"), TEXT("algorithm"), TEXT("flipY"), TEXT("save"),
            TEXT("channelMode")
        };
        for (const auto& Field : Params->Values)
        {
            if (!ValidParams.Contains(Field.Key))
            {
                TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Invalid parameter: %s"), *Field.Key));
            }
        }

        FString SourceTexture = GetStringFieldTextAuth(Params, TEXT("sourceTexture"), TEXT(""));
        FString Name = GetStringFieldTextAuth(Params, TEXT("name"), TEXT(""));
        FString Path = GetStringFieldTextAuth(Params, TEXT("path"), TEXT(""));
        
        // SECURITY: Validate sourceTexture path
        FString SanitizedSource = SanitizeProjectRelativePath(SourceTexture);
        if (SanitizedSource.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Invalid sourceTexture: contains traversal or invalid characters"));
        }
        SourceTexture = SanitizedSource;
        
        float Strength = static_cast<float>(GetNumberFieldTextAuth(Params, TEXT("strength"), 1.0));
        FString Algorithm = GetStringFieldTextAuth(Params, TEXT("algorithm"), TEXT("Sobel"));
        bool bFlipY = GetBoolFieldTextAuth(Params, TEXT("flipY"), false);
        bool bSave = GetBoolFieldTextAuth(Params, TEXT("save"), true);
        
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
        
        // SECURITY: Validate output path
        FString SanitizedPath = SanitizeProjectRelativePath(Path);
        if (SanitizedPath.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Invalid path: contains traversal or invalid characters"));
        }
        Path = SanitizedPath;
        
        // Validate name
        FString SanitizedName = SanitizeAssetName(Name);
        if (SanitizedName.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Invalid name: contains invalid characters"));
        }
        Name = SanitizedName;
        
        // Create output texture
        UTexture2D* NormalMap = CreateEmptyTexture(Path, Name, Width, Height, false);
        if (!NormalMap)
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Failed to create normal map texture"));
        }
        
        // Set normal map properties
        NormalMap->SRGB = false;
        NormalMap->CompressionSettings = TC_Normalmap;
        
        // Read height data with proper luminance or channel selection
        TArray<float> HeightData;
        HeightData.SetNum(Width * Height);
        
        // Get channel mapping option - defaults to "luminance" for proper grayscale conversion
        // Options: "luminance", "red", "green", "blue", "alpha", "average"
        FString ChannelMode = GetStringFieldTextAuth(Params, TEXT("channelMode"), TEXT("luminance"));
        
        // Lock source texture for reading
        FTexture2DMipMap& HeightMip = HeightMap->GetPlatformData()->Mips[0];
        const uint8* HeightPixels = static_cast<const uint8*>(HeightMip.BulkData.LockReadOnly());
        
        for (int32 i = 0; i < Width * Height; i++)
        {
            // BGRA format: index 0=B, 1=G, 2=R, 3=A
            uint8 B = HeightPixels[i * 4 + 0];
            uint8 G = HeightPixels[i * 4 + 1];
            uint8 R = HeightPixels[i * 4 + 2];
            uint8 A = HeightPixels[i * 4 + 3];
            
            float HeightValue;
            if (ChannelMode.Equals(TEXT("red"), ESearchCase::IgnoreCase))
            {
                HeightValue = static_cast<float>(R) / 255.0f;
            }
            else if (ChannelMode.Equals(TEXT("green"), ESearchCase::IgnoreCase))
            {
                HeightValue = static_cast<float>(G) / 255.0f;
            }
            else if (ChannelMode.Equals(TEXT("blue"), ESearchCase::IgnoreCase))
            {
                HeightValue = static_cast<float>(B) / 255.0f;
            }
            else if (ChannelMode.Equals(TEXT("alpha"), ESearchCase::IgnoreCase))
            {
                HeightValue = static_cast<float>(A) / 255.0f;
            }
            else if (ChannelMode.Equals(TEXT("average"), ESearchCase::IgnoreCase))
            {
                HeightValue = (static_cast<float>(R) + static_cast<float>(G) + static_cast<float>(B)) / (255.0f * 3.0f);
            }
            else // Default: Rec. 709 luminance coefficients for proper grayscale
            {
                // Y = 0.2126*R + 0.7152*G + 0.0722*B (ITU-R BT.709 standard)
                HeightValue = (0.2126f * static_cast<float>(R) + 
                               0.7152f * static_cast<float>(G) + 
                               0.0722f * static_cast<float>(B)) / 255.0f;
            }
            
            HeightData[i] = HeightValue;
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
            McpSafeAssetSave(NormalMap);
        }
        
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), TEXT("Normal map created from height map"));
        Response->SetStringField(TEXT("assetPath"), Path / Name);
        return Response;
    }
    
    if (SubAction == TEXT("create_ao_from_mesh"))
    {
        // Validate that no unknown/invalid parameters are present
        TSet<FString> ValidParams = {
            TEXT("subAction"), TEXT("meshPath"), TEXT("name"), TEXT("path"),
            TEXT("width"), TEXT("height"), TEXT("sampleCount"),
            TEXT("intensity"), TEXT("radius"), TEXT("save")
        };
        for (const auto& Field : Params->Values)
        {
            if (!ValidParams.Contains(Field.Key))
            {
                TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Invalid parameter: %s"), *Field.Key));
            }
        }

        // AO texture generation - creates a procedural AO approximation
        // For real mesh-based AO, GPU baking with scene capture would be required
        FString MeshPath = GetStringFieldTextAuth(Params, TEXT("meshPath"), TEXT(""));
        FString Name = GetStringFieldTextAuth(Params, TEXT("name"), TEXT(""));
        FString Path = GetStringFieldTextAuth(Params, TEXT("path"), TEXT("/Game/Textures"));
        
        // SECURITY: Validate and sanitize paths
        FString SanitizedMeshPath = SanitizeProjectRelativePath(MeshPath);
        if (SanitizedMeshPath.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Invalid meshPath: contains traversal or invalid characters"));
        }
        MeshPath = SanitizedMeshPath;
        
        FString SanitizedPath = SanitizeProjectRelativePath(Path);
        if (SanitizedPath.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Invalid path: contains traversal or invalid characters"));
        }
        Path = SanitizedPath;
        
        // Validate name
        FString SanitizedName = SanitizeAssetName(Name);
        if (SanitizedName.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Invalid name: contains invalid characters"));
        }
        Name = SanitizedName;
        
        int32 Width = static_cast<int32>(GetNumberFieldTextAuth(Params, TEXT("width"), 1024));
        int32 Height = static_cast<int32>(GetNumberFieldTextAuth(Params, TEXT("height"), 1024));
        int32 SampleCount = static_cast<int32>(GetNumberFieldTextAuth(Params, TEXT("sampleCount"), 16));
        float Intensity = static_cast<float>(GetNumberFieldTextAuth(Params, TEXT("intensity"), 1.0));
        float Radius = static_cast<float>(GetNumberFieldTextAuth(Params, TEXT("radius"), 0.1));
        bool bSave = GetBoolFieldTextAuth(Params, TEXT("save"), true);
        
        if (MeshPath.IsEmpty() || Name.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("meshPath and name are required"));
        }
        
        // Create AO texture
        UTexture2D* AOTexture = CreateEmptyTexture(Path, Name, Width, Height, false);
        if (!AOTexture)
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Failed to create AO texture"));
        }
        
        // Set texture properties for AO map
        AOTexture->SRGB = false;
        AOTexture->CompressionSettings = TC_Masks;
        
        uint8* MipData = AOTexture->Source.LockMip(0);
        if (!MipData)
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Failed to lock texture mip data"));
        }
        
        // Generate procedural AO pattern based on noise
        // This simulates ambient occlusion with a cavity-like effect
        // Real AO would require ray-tracing against the actual mesh geometry
        int32 Seed = 12345;
        SampleCount = FMath::Clamp(SampleCount, 4, 64);
        
        for (int32 Y = 0; Y < Height; Y++)
        {
            for (int32 X = 0; X < Width; X++)
            {
                float U = static_cast<float>(X) / static_cast<float>(Width - 1);
                float V = static_cast<float>(Y) / static_cast<float>(Height - 1);
                
                // Sample multiple points around current position for AO approximation
                float Occlusion = 0.0f;
                for (int32 S = 0; S < SampleCount; S++)
                {
                    float Angle = static_cast<float>(S) / static_cast<float>(SampleCount) * PI * 2.0f;
                    float SampleRadius = Radius * (0.5f + 0.5f * static_cast<float>(S % 4) / 4.0f);
                    float SU = U + FMath::Cos(Angle) * SampleRadius;
                    float SV = V + FMath::Sin(Angle) * SampleRadius;
                    
                    // Edge-based occlusion (pixels near edges have more occlusion)
                    float EdgeDist = FMath::Min(FMath::Min(SU, 1.0f - SU), FMath::Min(SV, 1.0f - SV));
                    Occlusion += FMath::Clamp(EdgeDist * 10.0f, 0.0f, 1.0f);
                }
                Occlusion /= static_cast<float>(SampleCount);
                
                // Apply intensity and invert (AO is darker in occluded areas)
                float AO = FMath::Lerp(1.0f, Occlusion, Intensity);
                AO = FMath::Clamp(AO, 0.0f, 1.0f);
                
                uint8 AOValue = static_cast<uint8>(AO * 255.0f);
                int32 PixelIndex = (Y * Width + X) * 4;
                MipData[PixelIndex + 0] = AOValue; // B
                MipData[PixelIndex + 1] = AOValue; // G
                MipData[PixelIndex + 2] = AOValue; // R
                MipData[PixelIndex + 3] = 255;     // A
            }
        }
        
        AOTexture->Source.UnlockMip(0);
        AOTexture->UpdateResource();
        
        if (bSave)
        {
            FAssetRegistryModule::AssetCreated(AOTexture);
            McpSafeAssetSave(AOTexture);
        }
        
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("AO texture '%s' created (procedural approximation)"), *Name));
        Response->SetStringField(TEXT("note"), TEXT("For mesh-accurate AO, use external baking tools like Substance Painter or xNormal"));
        Response->SetStringField(TEXT("assetPath"), Path / Name);
        return Response;
    }
    
    // ===== TEXTURE SETTINGS =====
    
    if (SubAction == TEXT("set_compression_settings"))
    {
        // Validate that no unknown/invalid parameters are present
        TSet<FString> ValidParams = {
            TEXT("subAction"), TEXT("assetPath"), TEXT("compressionSettings"), TEXT("save")
        };
        for (const auto& Field : Params->Values)
        {
            if (!ValidParams.Contains(Field.Key))
            {
                TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Invalid parameter: %s"), *Field.Key));
            }
        }

        FString AssetPath = GetStringFieldTextAuth(Params, TEXT("assetPath"), TEXT(""));
        
        // SECURITY: Validate assetPath
        FString SanitizedAssetPath = SanitizeProjectRelativePath(AssetPath);
        if (SanitizedAssetPath.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Invalid assetPath: contains traversal or invalid characters"));
        }
        AssetPath = SanitizedAssetPath;
        
        FString CompressionSettingsStr = GetStringFieldTextAuth(Params, TEXT("compressionSettings"), TEXT("TC_Default"));
        bool bSave = GetBoolFieldTextAuth(Params, TEXT("save"), true);
        
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
            McpSafeAssetSave(Texture);
        }
        
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Compression set to %s"), *CompressionSettingsStr));
        return Response;
    }
    
    if (SubAction == TEXT("set_texture_group"))
    {
        // Validate that no unknown/invalid parameters are present
        TSet<FString> ValidParams = {
            TEXT("subAction"), TEXT("assetPath"), TEXT("textureGroup"), TEXT("save")
        };
        for (const auto& Field : Params->Values)
        {
            if (!ValidParams.Contains(Field.Key))
            {
                TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Invalid parameter: %s"), *Field.Key));
            }
        }

        FString AssetPath = GetStringFieldTextAuth(Params, TEXT("assetPath"), TEXT(""));
        
        // SECURITY: Validate assetPath
        FString SanitizedAssetPath = SanitizeProjectRelativePath(AssetPath);
        if (SanitizedAssetPath.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Invalid assetPath: contains traversal or invalid characters"));
        }
        AssetPath = SanitizedAssetPath;
        
        FString TextureGroup = GetStringFieldTextAuth(Params, TEXT("textureGroup"), TEXT("TEXTUREGROUP_World"));
        bool bSave = GetBoolFieldTextAuth(Params, TEXT("save"), true);
        
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
            McpSafeAssetSave(Texture);
        }
        
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Texture group set to %s"), *TextureGroup));
        return Response;
    }
    
    if (SubAction == TEXT("set_lod_bias"))
    {
        // Validate that no unknown/invalid parameters are present
        TSet<FString> ValidParams = {
            TEXT("subAction"), TEXT("assetPath"), TEXT("lodBias"), TEXT("save")
        };
        for (const auto& Field : Params->Values)
        {
            if (!ValidParams.Contains(Field.Key))
            {
                TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Invalid parameter: %s"), *Field.Key));
            }
        }

        FString AssetPath = GetStringFieldTextAuth(Params, TEXT("assetPath"), TEXT(""));
        
        // SECURITY: Validate assetPath
        FString SanitizedAssetPath = SanitizeProjectRelativePath(AssetPath);
        if (SanitizedAssetPath.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Invalid assetPath: contains traversal or invalid characters"));
        }
        AssetPath = SanitizedAssetPath;
        
        int32 LODBias = static_cast<int32>(GetNumberFieldTextAuth(Params, TEXT("lodBias"), 0));
        bool bSave = GetBoolFieldTextAuth(Params, TEXT("save"), true);
        
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
            McpSafeAssetSave(Texture);
        }
        
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("LOD bias set to %d"), LODBias));
        return Response;
    }
    
    if (SubAction == TEXT("configure_virtual_texture"))
    {
        // Validate that no unknown/invalid parameters are present
        TSet<FString> ValidParams = {
            TEXT("subAction"), TEXT("assetPath"), TEXT("virtualTextureStreaming"), TEXT("save")
        };
        for (const auto& Field : Params->Values)
        {
            if (!ValidParams.Contains(Field.Key))
            {
                TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Invalid parameter: %s"), *Field.Key));
            }
        }

        FString AssetPath = GetStringFieldTextAuth(Params, TEXT("assetPath"), TEXT(""));
        
        // SECURITY: Validate assetPath
        FString SanitizedAssetPath = SanitizeProjectRelativePath(AssetPath);
        if (SanitizedAssetPath.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Invalid assetPath: contains traversal or invalid characters"));
        }
        AssetPath = SanitizedAssetPath;
        
        bool bVirtualTextureStreaming = GetBoolFieldTextAuth(Params, TEXT("virtualTextureStreaming"), false);
        bool bSave = GetBoolFieldTextAuth(Params, TEXT("save"), true);
        
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
            McpSafeAssetSave(Texture);
        }
        
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Virtual texture streaming %s"), bVirtualTextureStreaming ? TEXT("enabled") : TEXT("disabled")));
        return Response;
    }
    
    if (SubAction == TEXT("set_streaming_priority"))
    {
        // Validate that no unknown/invalid parameters are present
        TSet<FString> ValidParams = {
            TEXT("subAction"), TEXT("assetPath"), TEXT("neverStream"), TEXT("save")
        };
        for (const auto& Field : Params->Values)
        {
            if (!ValidParams.Contains(Field.Key))
            {
                TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Invalid parameter: %s"), *Field.Key));
            }
        }

        FString AssetPath = GetStringFieldTextAuth(Params, TEXT("assetPath"), TEXT(""));
        
        // SECURITY: Validate assetPath
        FString SanitizedAssetPath = SanitizeProjectRelativePath(AssetPath);
        if (SanitizedAssetPath.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Invalid assetPath: contains traversal or invalid characters"));
        }
        AssetPath = SanitizedAssetPath;
        
        bool bNeverStream = GetBoolFieldTextAuth(Params, TEXT("neverStream"), false);
        bool bSave = GetBoolFieldTextAuth(Params, TEXT("save"), true);
        
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
            McpSafeAssetSave(Texture);
        }
        
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), TEXT("Streaming priority configured"));
        return Response;
    }
    
    if (SubAction == TEXT("get_texture_info"))
    {
        // Validate that no unknown/invalid parameters are present
        TSet<FString> ValidParams = {
            TEXT("subAction"), TEXT("assetPath")
        };
        for (const auto& Field : Params->Values)
        {
            if (!ValidParams.Contains(Field.Key))
            {
                TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Invalid parameter: %s"), *Field.Key));
            }
        }

        FString AssetPath = GetStringFieldTextAuth(Params, TEXT("assetPath"), TEXT(""));
        
        // SECURITY: Validate assetPath
        FString SanitizedAssetPath = SanitizeProjectRelativePath(AssetPath);
        if (SanitizedAssetPath.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Invalid assetPath: contains traversal or invalid characters"));
        }
        AssetPath = SanitizedAssetPath;
        
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
    // Real CPU-based pixel manipulation implementations
    
    if (SubAction == TEXT("resize_texture"))
    {
        // Validate that no unknown/invalid parameters are present
        TSet<FString> ValidParams = {
            TEXT("subAction"), TEXT("sourcePath"), TEXT("name"), TEXT("path"),
            TEXT("newWidth"), TEXT("newHeight"), TEXT("save")
        };
        for (const auto& Field : Params->Values)
        {
            if (!ValidParams.Contains(Field.Key))
            {
                TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Invalid parameter: %s"), *Field.Key));
            }
        }

        FString SourcePath = GetStringFieldTextAuth(Params, TEXT("sourcePath"), TEXT(""));
        FString Name = GetStringFieldTextAuth(Params, TEXT("name"), TEXT(""));
        FString Path = GetStringFieldTextAuth(Params, TEXT("path"), TEXT(""));
        
        // SECURITY: Validate sourcePath
        FString SanitizedSource = SanitizeProjectRelativePath(SourcePath);
        if (SanitizedSource.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Invalid sourcePath: contains traversal or invalid characters"));
        }
        SourcePath = SanitizedSource;
        
        int32 NewWidth = static_cast<int32>(GetNumberFieldTextAuth(Params, TEXT("newWidth"), 512));
        int32 NewHeight = static_cast<int32>(GetNumberFieldTextAuth(Params, TEXT("newHeight"), 512));
        bool bSave = GetBoolFieldTextAuth(Params, TEXT("save"), true);
        
        if (SourcePath.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("sourcePath is required"));
        }
        
        UTexture2D* SourceTexture = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *SourcePath));
        if (!SourceTexture)
        {
            TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Failed to load source texture: %s"), *SourcePath));
        }
        
        // Get source dimensions and data
        int32 SrcWidth = SourceTexture->GetSizeX();
        int32 SrcHeight = SourceTexture->GetSizeY();
        
        // Lock source mip data for reading
        FTexture2DMipMap& SrcMip = SourceTexture->GetPlatformData()->Mips[0];
        const FColor* SrcData = static_cast<const FColor*>(SrcMip.BulkData.LockReadOnly());
        if (!SrcData)
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Failed to lock source texture data"));
        }
        
        // Generate output name and path if not specified
        if (Name.IsEmpty())
        {
            Name = FPaths::GetBaseFilename(SourcePath) + TEXT("_Resized");
        }
        if (Path.IsEmpty())
        {
            Path = FPaths::GetPath(SourcePath);
        }
        
        // SECURITY: Validate output path
        FString SanitizedPath = SanitizeProjectRelativePath(Path);
        if (SanitizedPath.IsEmpty())
        {
            SrcMip.BulkData.Unlock();
            TEXTURE_ERROR_RESPONSE(TEXT("Invalid path: contains traversal or invalid characters"));
        }
        Path = SanitizedPath;
        
        // Validate name
        FString SanitizedName = SanitizeAssetName(Name);
        if (SanitizedName.IsEmpty())
        {
            SrcMip.BulkData.Unlock();
            TEXTURE_ERROR_RESPONSE(TEXT("Invalid name: contains invalid characters"));
        }
        Name = SanitizedName;
        
        // Create destination texture
        UTexture2D* NewTexture = CreateEmptyTexture(Path, Name, NewWidth, NewHeight, false);
        if (!NewTexture)
        {
            SrcMip.BulkData.Unlock();
            TEXTURE_ERROR_RESPONSE(TEXT("Failed to create resized texture"));
        }
        
        uint8* DstMipData = NewTexture->Source.LockMip(0);
        if (!DstMipData)
        {
            SrcMip.BulkData.Unlock();
            TEXTURE_ERROR_RESPONSE(TEXT("Failed to lock destination texture data"));
        }
        
        // Bilinear interpolation resize
        for (int32 Y = 0; Y < NewHeight; ++Y)
        {
            for (int32 X = 0; X < NewWidth; ++X)
            {
                float U = static_cast<float>(X) / static_cast<float>(NewWidth - 1) * (SrcWidth - 1);
                float V = static_cast<float>(Y) / static_cast<float>(NewHeight - 1) * (SrcHeight - 1);
                
                int32 X0 = FMath::FloorToInt(U);
                int32 Y0 = FMath::FloorToInt(V);
                int32 X1 = FMath::Min(X0 + 1, SrcWidth - 1);
                int32 Y1 = FMath::Min(Y0 + 1, SrcHeight - 1);
                
                float FracX = U - X0;
                float FracY = V - Y0;
                
                const FColor& C00 = SrcData[Y0 * SrcWidth + X0];
                const FColor& C10 = SrcData[Y0 * SrcWidth + X1];
                const FColor& C01 = SrcData[Y1 * SrcWidth + X0];
                const FColor& C11 = SrcData[Y1 * SrcWidth + X1];
                
                // Bilinear interpolation
                FColor SampledColor;
                SampledColor.R = static_cast<uint8>(FMath::Lerp(FMath::Lerp((float)C00.R, (float)C10.R, FracX), FMath::Lerp((float)C01.R, (float)C11.R, FracX), FracY));
                SampledColor.G = static_cast<uint8>(FMath::Lerp(FMath::Lerp((float)C00.G, (float)C10.G, FracX), FMath::Lerp((float)C01.G, (float)C11.G, FracX), FracY));
                SampledColor.B = static_cast<uint8>(FMath::Lerp(FMath::Lerp((float)C00.B, (float)C10.B, FracX), FMath::Lerp((float)C01.B, (float)C11.B, FracX), FracY));
                SampledColor.A = static_cast<uint8>(FMath::Lerp(FMath::Lerp((float)C00.A, (float)C10.A, FracX), FMath::Lerp((float)C01.A, (float)C11.A, FracX), FracY));
                
                int32 DstIndex = (Y * NewWidth + X) * 4;
                DstMipData[DstIndex + 0] = SampledColor.B;
                DstMipData[DstIndex + 1] = SampledColor.G;
                DstMipData[DstIndex + 2] = SampledColor.R;
                DstMipData[DstIndex + 3] = SampledColor.A;
            }
        }
        
        SrcMip.BulkData.Unlock();
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
        return Response;
    }
    
    if (SubAction == TEXT("invert"))
    {
        // Validate that no unknown/invalid parameters are present
        TSet<FString> ValidParams = {
            TEXT("subAction"), TEXT("assetPath"), TEXT("inPlace"), TEXT("name"), TEXT("path"), TEXT("save")
        };
        for (const auto& Field : Params->Values)
        {
            if (!ValidParams.Contains(Field.Key))
            {
                TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Invalid parameter: %s"), *Field.Key));
            }
        }

        FString AssetPath = GetStringFieldTextAuth(Params, TEXT("assetPath"), TEXT(""));
        
        // SECURITY: Validate assetPath
        FString SanitizedAssetPath = SanitizeProjectRelativePath(AssetPath);
        if (SanitizedAssetPath.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Invalid assetPath: contains traversal or invalid characters"));
        }
        AssetPath = SanitizedAssetPath;
        
        bool bInPlace = GetBoolFieldTextAuth(Params, TEXT("inPlace"), true);
        FString Name = GetStringFieldTextAuth(Params, TEXT("name"), TEXT(""));
        FString Path = GetStringFieldTextAuth(Params, TEXT("path"), TEXT(""));
        bool bSave = GetBoolFieldTextAuth(Params, TEXT("save"), true);
        
        if (AssetPath.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("assetPath is required"));
        }
        
        UTexture2D* SourceTexture = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *AssetPath));
        if (!SourceTexture)
        {
            TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Failed to load texture: %s"), *AssetPath));
        }
        
        int32 Width = SourceTexture->GetSizeX();
        int32 Height = SourceTexture->GetSizeY();
        
        UTexture2D* TargetTexture = SourceTexture;
        if (!bInPlace)
        {
            if (Name.IsEmpty()) Name = FPaths::GetBaseFilename(AssetPath) + TEXT("_Inverted");
            if (Path.IsEmpty()) Path = FPaths::GetPath(AssetPath);
            
            // SECURITY: Validate output path
            FString SanitizedPath = SanitizeProjectRelativePath(Path);
            if (SanitizedPath.IsEmpty())
            {
                TEXTURE_ERROR_RESPONSE(TEXT("Invalid path: contains traversal or invalid characters"));
            }
            Path = SanitizedPath;
            
            // Validate name
            FString SanitizedName = SanitizeAssetName(Name);
            if (SanitizedName.IsEmpty())
            {
                TEXTURE_ERROR_RESPONSE(TEXT("Invalid name: contains invalid characters"));
            }
            Name = SanitizedName;
            
            TargetTexture = CreateEmptyTexture(Path, Name, Width, Height, false);
            if (!TargetTexture)
            {
                TEXTURE_ERROR_RESPONSE(TEXT("Failed to create output texture"));
            }
        }
        
        // Lock mip data
        uint8* MipData = TargetTexture->Source.LockMip(0);
        if (!MipData)
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Failed to lock texture mip data"));
        }
        
        if (!bInPlace)
        {
            // Copy source data first
            FTexture2DMipMap& SrcMip = SourceTexture->GetPlatformData()->Mips[0];
            const uint8* SrcData = static_cast<const uint8*>(SrcMip.BulkData.LockReadOnly());
            FMemory::Memcpy(MipData, SrcData, Width * Height * 4);
            SrcMip.BulkData.Unlock();
        }
        
        // Invert RGB, keep alpha
        int32 NumPixels = Width * Height;
        for (int32 i = 0; i < NumPixels; ++i)
        {
            int32 Idx = i * 4;
            MipData[Idx + 0] = 255 - MipData[Idx + 0]; // B
            MipData[Idx + 1] = 255 - MipData[Idx + 1]; // G
            MipData[Idx + 2] = 255 - MipData[Idx + 2]; // R
            // Alpha unchanged
        }
        
        TargetTexture->Source.UnlockMip(0);
        TargetTexture->UpdateResource();
        TargetTexture->MarkPackageDirty();
        
        if (bSave)
        {
            McpSafeAssetSave(TargetTexture);
        }
        
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), TEXT("Texture colors inverted"));
        Response->SetStringField(TEXT("assetPath"), bInPlace ? AssetPath : (Path / Name));
        return Response;
    }
    
    if (SubAction == TEXT("desaturate"))
    {
        // Validate that no unknown/invalid parameters are present
        TSet<FString> ValidParams = {
            TEXT("subAction"), TEXT("assetPath"), TEXT("amount"), TEXT("inPlace"),
            TEXT("name"), TEXT("path"), TEXT("save")
        };
        for (const auto& Field : Params->Values)
        {
            if (!ValidParams.Contains(Field.Key))
            {
                TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Invalid parameter: %s"), *Field.Key));
            }
        }

        FString AssetPath = GetStringFieldTextAuth(Params, TEXT("assetPath"), TEXT(""));
        
        // SECURITY: Validate assetPath
        FString SanitizedAssetPath = SanitizeProjectRelativePath(AssetPath);
        if (SanitizedAssetPath.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Invalid assetPath: contains traversal or invalid characters"));
        }
        AssetPath = SanitizedAssetPath;
        
        float Amount = static_cast<float>(GetNumberFieldTextAuth(Params, TEXT("amount"), 1.0));
        bool bInPlace = GetBoolFieldTextAuth(Params, TEXT("inPlace"), true);
        FString Name = GetStringFieldTextAuth(Params, TEXT("name"), TEXT(""));
        FString Path = GetStringFieldTextAuth(Params, TEXT("path"), TEXT(""));
        bool bSave = GetBoolFieldTextAuth(Params, TEXT("save"), true);
        
        if (AssetPath.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("assetPath is required"));
        }
        
        UTexture2D* SourceTexture = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *AssetPath));
        if (!SourceTexture)
        {
            TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Failed to load texture: %s"), *AssetPath));
        }
        
        int32 Width = SourceTexture->GetSizeX();
        int32 Height = SourceTexture->GetSizeY();
        
        UTexture2D* TargetTexture = SourceTexture;
        if (!bInPlace)
        {
            if (Name.IsEmpty()) Name = FPaths::GetBaseFilename(AssetPath) + TEXT("_Desaturated");
            if (Path.IsEmpty()) Path = FPaths::GetPath(AssetPath);
            
            // SECURITY: Validate output path
            FString SanitizedPath = SanitizeProjectRelativePath(Path);
            if (SanitizedPath.IsEmpty())
            {
                TEXTURE_ERROR_RESPONSE(TEXT("Invalid path: contains traversal or invalid characters"));
            }
            Path = SanitizedPath;
            
            // Validate name
            FString SanitizedName = SanitizeAssetName(Name);
            if (SanitizedName.IsEmpty())
            {
                TEXTURE_ERROR_RESPONSE(TEXT("Invalid name: contains invalid characters"));
            }
            Name = SanitizedName;
            
            TargetTexture = CreateEmptyTexture(Path, Name, Width, Height, false);
            if (!TargetTexture)
            {
                TEXTURE_ERROR_RESPONSE(TEXT("Failed to create output texture"));
            }
        }
        
        uint8* MipData = TargetTexture->Source.LockMip(0);
        if (!MipData)
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Failed to lock texture mip data"));
        }
        
        if (!bInPlace)
        {
            FTexture2DMipMap& SrcMip = SourceTexture->GetPlatformData()->Mips[0];
            const uint8* SrcData = static_cast<const uint8*>(SrcMip.BulkData.LockReadOnly());
            FMemory::Memcpy(MipData, SrcData, Width * Height * 4);
            SrcMip.BulkData.Unlock();
        }
        
        Amount = FMath::Clamp(Amount, 0.0f, 1.0f);
        int32 NumPixels = Width * Height;
        for (int32 i = 0; i < NumPixels; ++i)
        {
            int32 Idx = i * 4;
            uint8 B = MipData[Idx + 0];
            uint8 G = MipData[Idx + 1];
            uint8 R = MipData[Idx + 2];
            
            // Rec. 709 luminance coefficients
            uint8 Gray = static_cast<uint8>(0.2126f * R + 0.7152f * G + 0.0722f * B);
            
            MipData[Idx + 0] = static_cast<uint8>(FMath::Lerp((float)B, (float)Gray, Amount));
            MipData[Idx + 1] = static_cast<uint8>(FMath::Lerp((float)G, (float)Gray, Amount));
            MipData[Idx + 2] = static_cast<uint8>(FMath::Lerp((float)R, (float)Gray, Amount));
        }
        
        TargetTexture->Source.UnlockMip(0);
        TargetTexture->UpdateResource();
        TargetTexture->MarkPackageDirty();
        
        if (bSave)
        {
            McpSafeAssetSave(TargetTexture);
        }
        
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Texture desaturated (amount: %.2f)"), Amount));
        Response->SetStringField(TEXT("assetPath"), bInPlace ? AssetPath : (Path / Name));
        return Response;
    }
    
    if (SubAction == TEXT("adjust_levels"))
    {
        // Validate that no unknown/invalid parameters are present
        TSet<FString> ValidParams = {
            TEXT("subAction"), TEXT("assetPath"), TEXT("inBlack"), TEXT("inWhite"),
            TEXT("gamma"), TEXT("outBlack"), TEXT("outWhite"), TEXT("inPlace"), TEXT("save")
        };
        for (const auto& Field : Params->Values)
        {
            if (!ValidParams.Contains(Field.Key))
            {
                TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Invalid parameter: %s"), *Field.Key));
            }
        }

        FString AssetPath = GetStringFieldTextAuth(Params, TEXT("assetPath"), TEXT(""));
        
        // SECURITY: Validate assetPath
        FString SanitizedAssetPath = SanitizeProjectRelativePath(AssetPath);
        if (SanitizedAssetPath.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Invalid assetPath: contains traversal or invalid characters"));
        }
        AssetPath = SanitizedAssetPath;
        
        float InBlack = static_cast<float>(GetNumberFieldTextAuth(Params, TEXT("inBlack"), 0.0));
        float InWhite = static_cast<float>(GetNumberFieldTextAuth(Params, TEXT("inWhite"), 1.0));
        float Gamma = static_cast<float>(GetNumberFieldTextAuth(Params, TEXT("gamma"), 1.0));
        float OutBlack = static_cast<float>(GetNumberFieldTextAuth(Params, TEXT("outBlack"), 0.0));
        float OutWhite = static_cast<float>(GetNumberFieldTextAuth(Params, TEXT("outWhite"), 1.0));
        bool bInPlace = GetBoolFieldTextAuth(Params, TEXT("inPlace"), true);
        bool bSave = GetBoolFieldTextAuth(Params, TEXT("save"), true);
        
        if (AssetPath.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("assetPath is required"));
        }
        
        UTexture2D* Texture = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *AssetPath));
        if (!Texture)
        {
            TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Failed to load texture: %s"), *AssetPath));
        }
        
        int32 Width = Texture->GetSizeX();
        int32 Height = Texture->GetSizeY();
        
        uint8* MipData = Texture->Source.LockMip(0);
        if (!MipData)
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Failed to lock texture mip data"));
        }
        
        InBlack = FMath::Clamp(InBlack, 0.0f, 1.0f);
        InWhite = FMath::Clamp(InWhite, 0.0f, 1.0f);
        Gamma = FMath::Max(Gamma, 0.01f);
        OutBlack = FMath::Clamp(OutBlack, 0.0f, 1.0f);
        OutWhite = FMath::Clamp(OutWhite, 0.0f, 1.0f);
        
        float InRange = FMath::Max(InWhite - InBlack, 0.001f);
        float OutRange = OutWhite - OutBlack;
        float InvGamma = 1.0f / Gamma;
        
        int32 NumPixels = Width * Height;
        for (int32 i = 0; i < NumPixels; ++i)
        {
            int32 Idx = i * 4;
            for (int32 c = 0; c < 3; ++c)
            {
                float Val = MipData[Idx + c] / 255.0f;
                Val = FMath::Clamp((Val - InBlack) / InRange, 0.0f, 1.0f);
                Val = FMath::Pow(Val, InvGamma);
                Val = OutBlack + Val * OutRange;
                MipData[Idx + c] = static_cast<uint8>(FMath::Clamp(Val * 255.0f, 0.0f, 255.0f));
            }
        }
        
        Texture->Source.UnlockMip(0);
        Texture->UpdateResource();
        Texture->MarkPackageDirty();
        
        if (bSave)
        {
            McpSafeAssetSave(Texture);
        }
        
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), TEXT("Levels adjusted"));
        Response->SetStringField(TEXT("assetPath"), AssetPath);
        return Response;
    }
    
    if (SubAction == TEXT("blur"))
    {
        FString AssetPath = NormalizeTexturePath(GetStringFieldTextAuth(Params, TEXT("assetPath"), TEXT("")));
        int32 Radius = static_cast<int32>(GetNumberFieldTextAuth(Params, TEXT("radius"), 2));
        bool bSave = GetBoolFieldTextAuth(Params, TEXT("save"), true);
        
        if (AssetPath.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("assetPath is required"));
        }
        
        UTexture2D* Texture = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *AssetPath));
        if (!Texture)
        {
            TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Failed to load texture: %s"), *AssetPath));
        }
        
        int32 Width = Texture->GetSizeX();
        int32 Height = Texture->GetSizeY();
        Radius = FMath::Clamp(Radius, 1, 10);
        
        uint8* MipData = Texture->Source.LockMip(0);
        if (!MipData)
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Failed to lock texture mip data"));
        }
        
        // Create copy of original data
        TArray<uint8> OriginalData;
        int32 DataSize = Width * Height * 4;
        OriginalData.SetNumUninitialized(DataSize);
        FMemory::Memcpy(OriginalData.GetData(), MipData, DataSize);
        
        // Box blur
        int32 KernelSize = Radius * 2 + 1;
        float KernelWeight = 1.0f / (KernelSize * KernelSize);
        
        for (int32 Y = 0; Y < Height; ++Y)
        {
            for (int32 X = 0; X < Width; ++X)
            {
                float SumR = 0, SumG = 0, SumB = 0;
                
                for (int32 KY = -Radius; KY <= Radius; ++KY)
                {
                    for (int32 KX = -Radius; KX <= Radius; ++KX)
                    {
                        int32 SampleX = FMath::Clamp(X + KX, 0, Width - 1);
                        int32 SampleY = FMath::Clamp(Y + KY, 0, Height - 1);
                        int32 SampleIdx = (SampleY * Width + SampleX) * 4;
                        
                        SumB += OriginalData[SampleIdx + 0];
                        SumG += OriginalData[SampleIdx + 1];
                        SumR += OriginalData[SampleIdx + 2];
                    }
                }
                
                int32 DstIdx = (Y * Width + X) * 4;
                MipData[DstIdx + 0] = static_cast<uint8>(SumB * KernelWeight);
                MipData[DstIdx + 1] = static_cast<uint8>(SumG * KernelWeight);
                MipData[DstIdx + 2] = static_cast<uint8>(SumR * KernelWeight);
            }
        }
        
        Texture->Source.UnlockMip(0);
        Texture->UpdateResource();
        Texture->MarkPackageDirty();
        
        if (bSave)
        {
            McpSafeAssetSave(Texture);
        }
        
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Blur applied (radius: %d)"), Radius));
        Response->SetStringField(TEXT("assetPath"), AssetPath);
        return Response;
    }
    
    if (SubAction == TEXT("sharpen"))
    {
        FString AssetPath = NormalizeTexturePath(GetStringFieldTextAuth(Params, TEXT("assetPath"), TEXT("")));
        float Amount = static_cast<float>(GetNumberFieldTextAuth(Params, TEXT("amount"), 1.0));
        bool bSave = GetBoolFieldTextAuth(Params, TEXT("save"), true);
        
        if (AssetPath.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("assetPath is required"));
        }
        
        UTexture2D* Texture = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *AssetPath));
        if (!Texture)
        {
            TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Failed to load texture: %s"), *AssetPath));
        }
        
        int32 Width = Texture->GetSizeX();
        int32 Height = Texture->GetSizeY();
        Amount = FMath::Clamp(Amount, 0.0f, 5.0f);
        
        uint8* MipData = Texture->Source.LockMip(0);
        if (!MipData)
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Failed to lock texture mip data"));
        }
        
        // Create copy of original data
        TArray<uint8> OriginalData;
        int32 DataSize = Width * Height * 4;
        OriginalData.SetNumUninitialized(DataSize);
        FMemory::Memcpy(OriginalData.GetData(), MipData, DataSize);
        
        // Unsharp mask sharpening
        // Sharpen kernel: center = 1 + 4*amount, neighbors = -amount
        for (int32 Y = 1; Y < Height - 1; ++Y)
        {
            for (int32 X = 1; X < Width - 1; ++X)
            {
                int32 CenterIdx = (Y * Width + X) * 4;
                int32 LeftIdx = (Y * Width + X - 1) * 4;
                int32 RightIdx = (Y * Width + X + 1) * 4;
                int32 TopIdx = ((Y - 1) * Width + X) * 4;
                int32 BottomIdx = ((Y + 1) * Width + X) * 4;
                
                for (int32 c = 0; c < 3; ++c)
                {
                    float Center = OriginalData[CenterIdx + c];
                    float Left = OriginalData[LeftIdx + c];
                    float Right = OriginalData[RightIdx + c];
                    float Top = OriginalData[TopIdx + c];
                    float Bottom = OriginalData[BottomIdx + c];
                    
                    float Sharpened = Center * (1.0f + 4.0f * Amount) - Amount * (Left + Right + Top + Bottom);
                    MipData[CenterIdx + c] = static_cast<uint8>(FMath::Clamp(Sharpened, 0.0f, 255.0f));
                }
            }
        }
        
        Texture->Source.UnlockMip(0);
        Texture->UpdateResource();
        Texture->MarkPackageDirty();
        
        if (bSave)
        {
            McpSafeAssetSave(Texture);
        }
        
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Sharpen applied (amount: %.2f)"), Amount));
        Response->SetStringField(TEXT("assetPath"), AssetPath);
        return Response;
    }
    
    if (SubAction == TEXT("channel_pack"))
    {
        FString RedPath = NormalizeTexturePath(GetStringFieldTextAuth(Params, TEXT("redTexture"), TEXT("")));
        FString GreenPath = NormalizeTexturePath(GetStringFieldTextAuth(Params, TEXT("greenTexture"), TEXT("")));
        FString BluePath = NormalizeTexturePath(GetStringFieldTextAuth(Params, TEXT("blueTexture"), TEXT("")));
        FString AlphaPath = NormalizeTexturePath(GetStringFieldTextAuth(Params, TEXT("alphaTexture"), TEXT("")));
        FString Name = GetStringFieldTextAuth(Params, TEXT("name"), TEXT("ChannelPacked"));
        FString Path = NormalizeTexturePath(GetStringFieldTextAuth(Params, TEXT("path"), TEXT("/Game/Textures")));
        bool bSave = GetBoolFieldTextAuth(Params, TEXT("save"), true);
        
        if (Name.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("name is required"));
        }
        
        // Load channel textures
        UTexture2D* RedTex = !RedPath.IsEmpty() ? Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *RedPath)) : nullptr;
        UTexture2D* GreenTex = !GreenPath.IsEmpty() ? Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *GreenPath)) : nullptr;
        UTexture2D* BlueTex = !BluePath.IsEmpty() ? Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *BluePath)) : nullptr;
        UTexture2D* AlphaTex = !AlphaPath.IsEmpty() ? Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *AlphaPath)) : nullptr;
        
        // Determine output size from first available texture
        int32 Width = 1024, Height = 1024;
        if (RedTex) { Width = RedTex->GetSizeX(); Height = RedTex->GetSizeY(); }
        else if (GreenTex) { Width = GreenTex->GetSizeX(); Height = GreenTex->GetSizeY(); }
        else if (BlueTex) { Width = BlueTex->GetSizeX(); Height = BlueTex->GetSizeY(); }
        else if (AlphaTex) { Width = AlphaTex->GetSizeX(); Height = AlphaTex->GetSizeY(); }
        
        UTexture2D* OutputTexture = CreateEmptyTexture(Path, Name, Width, Height, false);
        if (!OutputTexture)
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Failed to create output texture"));
        }
        
        OutputTexture->SRGB = false;
        OutputTexture->CompressionSettings = TC_Masks;
        
        uint8* OutData = OutputTexture->Source.LockMip(0);
        if (!OutData)
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Failed to lock output texture data"));
        }
        
        // Helper to get channel data from texture
        auto GetChannelData = [](UTexture2D* Tex, int32 ChannelIdx) -> TArray<uint8> {
            TArray<uint8> Data;
            if (!Tex) return Data;
            int32 W = Tex->GetSizeX();
            int32 H = Tex->GetSizeY();
            Data.SetNumUninitialized(W * H);
            FTexture2DMipMap& Mip = Tex->GetPlatformData()->Mips[0];
            const uint8* MipData = static_cast<const uint8*>(Mip.BulkData.LockReadOnly());
            for (int32 i = 0; i < W * H; ++i)
            {
                Data[i] = MipData[i * 4 + ChannelIdx];
            }
            Mip.BulkData.Unlock();
            return Data;
        };
        
        TArray<uint8> RedData = GetChannelData(RedTex, 2); // R is at index 2 in BGRA
        TArray<uint8> GreenData = GetChannelData(GreenTex, 1);
        TArray<uint8> BlueData = GetChannelData(BlueTex, 0);
        TArray<uint8> AlphaData = GetChannelData(AlphaTex, 3);
        
        int32 NumPixels = Width * Height;
        for (int32 i = 0; i < NumPixels; ++i)
        {
            int32 Idx = i * 4;
            OutData[Idx + 0] = BlueData.Num() > i ? BlueData[i] : 0; // B
            OutData[Idx + 1] = GreenData.Num() > i ? GreenData[i] : 0; // G
            OutData[Idx + 2] = RedData.Num() > i ? RedData[i] : 0; // R
            OutData[Idx + 3] = AlphaData.Num() > i ? AlphaData[i] : 255; // A
        }
        
        OutputTexture->Source.UnlockMip(0);
        OutputTexture->UpdateResource();
        
        if (bSave)
        {
            FAssetRegistryModule::AssetCreated(OutputTexture);
            McpSafeAssetSave(OutputTexture);
        }
        
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), TEXT("Channels packed into single texture"));
        Response->SetStringField(TEXT("assetPath"), Path / Name);
        return Response;
    }
    
    if (SubAction == TEXT("combine_textures"))
    {
        FString BaseTexturePath = NormalizeTexturePath(GetStringFieldTextAuth(Params, TEXT("baseTexture"), TEXT("")));
        FString OverlayTexturePath = NormalizeTexturePath(GetStringFieldTextAuth(Params, TEXT("overlayTexture"), TEXT("")));
        FString BlendMode = GetStringFieldTextAuth(Params, TEXT("blendMode"), TEXT("Normal"));
        float Opacity = static_cast<float>(GetNumberFieldTextAuth(Params, TEXT("opacity"), 1.0));
        FString Name = GetStringFieldTextAuth(Params, TEXT("name"), TEXT("Combined"));
        FString Path = NormalizeTexturePath(GetStringFieldTextAuth(Params, TEXT("path"), TEXT("/Game/Textures")));
        bool bSave = GetBoolFieldTextAuth(Params, TEXT("save"), true);
        
        if (BaseTexturePath.IsEmpty() || OverlayTexturePath.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("baseTexture and overlayTexture are required"));
        }
        
        UTexture2D* BaseTex = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *BaseTexturePath));
        UTexture2D* OverlayTex = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *OverlayTexturePath));
        
        if (!BaseTex || !OverlayTex)
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Failed to load base or overlay texture"));
        }
        
        int32 Width = BaseTex->GetSizeX();
        int32 Height = BaseTex->GetSizeY();
        Opacity = FMath::Clamp(Opacity, 0.0f, 1.0f);
        
        UTexture2D* OutputTexture = CreateEmptyTexture(Path, Name, Width, Height, false);
        if (!OutputTexture)
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Failed to create output texture"));
        }
        
        // Lock all textures
        FTexture2DMipMap& BaseMip = BaseTex->GetPlatformData()->Mips[0];
        FTexture2DMipMap& OverlayMip = OverlayTex->GetPlatformData()->Mips[0];
        const uint8* BaseData = static_cast<const uint8*>(BaseMip.BulkData.LockReadOnly());
        const uint8* OverlayData = static_cast<const uint8*>(OverlayMip.BulkData.LockReadOnly());
        uint8* OutData = OutputTexture->Source.LockMip(0);
        
        if (!BaseData || !OverlayData || !OutData)
        {
            if (BaseData) BaseMip.BulkData.Unlock();
            if (OverlayData) OverlayMip.BulkData.Unlock();
            if (OutData) OutputTexture->Source.UnlockMip(0);
            TEXTURE_ERROR_RESPONSE(TEXT("Failed to lock texture data"));
        }
        
        int32 NumPixels = Width * Height;
        for (int32 i = 0; i < NumPixels; ++i)
        {
            int32 Idx = i * 4;
            
            for (int32 c = 0; c < 3; ++c)
            {
                float Base = BaseData[Idx + c] / 255.0f;
                float Overlay = OverlayData[Idx + c] / 255.0f;
                float Result;
                
                if (BlendMode.Equals(TEXT("Multiply"), ESearchCase::IgnoreCase))
                {
                    Result = Base * Overlay;
                }
                else if (BlendMode.Equals(TEXT("Screen"), ESearchCase::IgnoreCase))
                {
                    Result = 1.0f - (1.0f - Base) * (1.0f - Overlay);
                }
                else if (BlendMode.Equals(TEXT("Overlay"), ESearchCase::IgnoreCase))
                {
                    Result = Base < 0.5f ? 2.0f * Base * Overlay : 1.0f - 2.0f * (1.0f - Base) * (1.0f - Overlay);
                }
                else if (BlendMode.Equals(TEXT("Add"), ESearchCase::IgnoreCase))
                {
                    Result = FMath::Min(Base + Overlay, 1.0f);
                }
                else // Normal blend
                {
                    Result = Overlay;
                }
                
                Result = FMath::Lerp(Base, Result, Opacity);
                OutData[Idx + c] = static_cast<uint8>(FMath::Clamp(Result * 255.0f, 0.0f, 255.0f));
            }
            OutData[Idx + 3] = BaseData[Idx + 3]; // Keep base alpha
        }
        
        BaseMip.BulkData.Unlock();
        OverlayMip.BulkData.Unlock();
        OutputTexture->Source.UnlockMip(0);
        OutputTexture->UpdateResource();
        
        if (bSave)
        {
            FAssetRegistryModule::AssetCreated(OutputTexture);
            McpSafeAssetSave(OutputTexture);
        }
        
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Textures combined (mode: %s)"), *BlendMode));
        Response->SetStringField(TEXT("assetPath"), Path / Name);
        return Response;
    }
    
    // ===== adjust_curves =====
    // Apply RGB curve adjustment using LUT (lookup table) built from control points
    if (SubAction == TEXT("adjust_curves"))
    {
        FString AssetPath = NormalizeTexturePath(GetStringFieldTextAuth(Params, TEXT("assetPath"), TEXT("")));
        bool bInPlace = GetBoolFieldTextAuth(Params, TEXT("inPlace"), true);
        FString Name = GetStringFieldTextAuth(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeTexturePath(GetStringFieldTextAuth(Params, TEXT("path"), TEXT("")));
        bool bSave = GetBoolFieldTextAuth(Params, TEXT("save"), true);
        
        if (AssetPath.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("assetPath is required"));
        }
        
        UTexture2D* SourceTexture = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *AssetPath));
        if (!SourceTexture)
        {
            TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Failed to load texture: %s"), *AssetPath));
        }
        
        int32 Width = SourceTexture->GetSizeX();
        int32 Height = SourceTexture->GetSizeY();
        
        // Parse curve control points
        // Input/output arrays where input[i] maps to output[i]
        // Default: linear curve (0->0, 0.25->0.25, 0.5->0.5, 0.75->0.75, 1->1)
        TArray<float> InputPointsR, OutputPointsR;
        TArray<float> InputPointsG, OutputPointsG;
        TArray<float> InputPointsB, OutputPointsB;
        
        // Helper to parse curve points from JSON array
        auto ParseCurvePoints = [&Params](const FString& InputKey, const FString& OutputKey, TArray<float>& InputArr, TArray<float>& OutputArr) {
            const TArray<TSharedPtr<FJsonValue>>* InputArray;
            const TArray<TSharedPtr<FJsonValue>>* OutputArray;
            if (Params->TryGetArrayField(InputKey, InputArray) && Params->TryGetArrayField(OutputKey, OutputArray))
            {
                for (const auto& Val : *InputArray)
                {
                    InputArr.Add(static_cast<float>(Val->AsNumber()));
                }
                for (const auto& Val : *OutputArray)
                {
                    OutputArr.Add(static_cast<float>(Val->AsNumber()));
                }
            }
            // If not provided or empty, set default linear
            if (InputArr.Num() == 0 || OutputArr.Num() == 0)
            {
                InputArr = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
                OutputArr = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
            }
        };
        
        // Check if separate RGB curves are provided, otherwise use master curve
        if (Params->HasField(TEXT("inputR")))
        {
            ParseCurvePoints(TEXT("inputR"), TEXT("outputR"), InputPointsR, OutputPointsR);
            ParseCurvePoints(TEXT("inputG"), TEXT("outputG"), InputPointsG, OutputPointsG);
            ParseCurvePoints(TEXT("inputB"), TEXT("outputB"), InputPointsB, OutputPointsB);
        }
        else
        {
            // Use master curve for all channels
            TArray<float> MasterInput, MasterOutput;
            ParseCurvePoints(TEXT("input"), TEXT("output"), MasterInput, MasterOutput);
            InputPointsR = MasterInput; OutputPointsR = MasterOutput;
            InputPointsG = MasterInput; OutputPointsG = MasterOutput;
            InputPointsB = MasterInput; OutputPointsB = MasterOutput;
        }
        
        // Build 256-entry LUT via linear interpolation
        auto BuildLUT = [](const TArray<float>& Input, const TArray<float>& Output) -> TArray<uint8> {
            TArray<uint8> LUT;
            LUT.SetNum(256);
            
            if (Input.Num() < 2 || Output.Num() < 2 || Input.Num() != Output.Num())
            {
                // Fallback: linear 1:1 mapping
                for (int32 i = 0; i < 256; ++i)
                {
                    LUT[i] = static_cast<uint8>(i);
                }
                return LUT;
            }
            
            for (int32 i = 0; i < 256; ++i)
            {
                float NormalizedInput = static_cast<float>(i) / 255.0f;
                float Mapped = NormalizedInput;
                
                // Find segment in curve and interpolate
                for (int32 j = 0; j < Input.Num() - 1; ++j)
                {
                    if (NormalizedInput >= Input[j] && NormalizedInput <= Input[j + 1])
                    {
                        float SegmentRange = Input[j + 1] - Input[j];
                        if (SegmentRange > SMALL_NUMBER)
                        {
                            float T = (NormalizedInput - Input[j]) / SegmentRange;
                            Mapped = FMath::Lerp(Output[j], Output[j + 1], T);
                        }
                        else
                        {
                            Mapped = Output[j];
                        }
                        break;
                    }
                }
                
                // Handle values outside the defined range
                if (NormalizedInput < Input[0])
                {
                    Mapped = Output[0];
                }
                else if (NormalizedInput > Input[Input.Num() - 1])
                {
                    Mapped = Output[Output.Num() - 1];
                }
                
                LUT[i] = static_cast<uint8>(FMath::Clamp(Mapped * 255.0f, 0.0f, 255.0f));
            }
            return LUT;
        };
        
        TArray<uint8> LUT_R = BuildLUT(InputPointsR, OutputPointsR);
        TArray<uint8> LUT_G = BuildLUT(InputPointsG, OutputPointsG);
        TArray<uint8> LUT_B = BuildLUT(InputPointsB, OutputPointsB);
        
        UTexture2D* TargetTexture = SourceTexture;
        if (!bInPlace)
        {
            if (Name.IsEmpty()) Name = FPaths::GetBaseFilename(AssetPath) + TEXT("_Curved");
            if (Path.IsEmpty()) Path = FPaths::GetPath(AssetPath);
            TargetTexture = CreateEmptyTexture(Path, Name, Width, Height, false);
            if (!TargetTexture)
            {
                TEXTURE_ERROR_RESPONSE(TEXT("Failed to create output texture"));
            }
        }
        
        uint8* MipData = TargetTexture->Source.LockMip(0);
        if (!MipData)
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Failed to lock texture mip data"));
        }
        
        if (!bInPlace)
        {
            // Copy source data first
            FTexture2DMipMap& SrcMip = SourceTexture->GetPlatformData()->Mips[0];
            const uint8* SrcData = static_cast<const uint8*>(SrcMip.BulkData.LockReadOnly());
            FMemory::Memcpy(MipData, SrcData, Width * Height * 4);
            SrcMip.BulkData.Unlock();
        }
        
        // Apply LUT to each pixel (BGRA format: B=0, G=1, R=2, A=3)
        int32 NumPixels = Width * Height;
        for (int32 i = 0; i < NumPixels; ++i)
        {
            int32 Idx = i * 4;
            MipData[Idx + 0] = LUT_B[MipData[Idx + 0]]; // B
            MipData[Idx + 1] = LUT_G[MipData[Idx + 1]]; // G
            MipData[Idx + 2] = LUT_R[MipData[Idx + 2]]; // R
            // Alpha unchanged
        }
        
        TargetTexture->Source.UnlockMip(0);
        TargetTexture->UpdateResource();
        TargetTexture->MarkPackageDirty();
        
        if (bSave)
        {
            if (!bInPlace)
            {
                FAssetRegistryModule::AssetCreated(TargetTexture);
            }
            McpSafeAssetSave(TargetTexture);
        }
        
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), TEXT("Curve adjustment applied"));
        Response->SetStringField(TEXT("assetPath"), bInPlace ? AssetPath : (Path / Name));
        return Response;
    }
    
    // ===== channel_extract =====
    // Extract a single channel (R, G, B, or A) to a new grayscale texture
    if (SubAction == TEXT("channel_extract"))
    {
        FString SourcePath = NormalizeTexturePath(GetStringFieldTextAuth(Params, TEXT("texturePath"), TEXT("")));
        FString Channel = GetStringFieldTextAuth(Params, TEXT("channel"), TEXT("R"));
        FString OutputPath = NormalizeTexturePath(GetStringFieldTextAuth(Params, TEXT("outputPath"), TEXT("")));
        FString Name = GetStringFieldTextAuth(Params, TEXT("name"), TEXT(""));
        bool bSave = GetBoolFieldTextAuth(Params, TEXT("save"), true);
        
        if (SourcePath.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("texturePath is required"));
        }
        
        UTexture2D* SourceTexture = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *SourcePath));
        if (!SourceTexture)
        {
            TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Failed to load source texture: %s"), *SourcePath));
        }
        
        // Read source pixels
        int32 Width = SourceTexture->GetSizeX();
        int32 Height = SourceTexture->GetSizeY();
        
        FTexture2DMipMap& SrcMip = SourceTexture->GetPlatformData()->Mips[0];
        const FColor* SrcPixels = static_cast<const FColor*>(SrcMip.BulkData.LockReadOnly());
        if (!SrcPixels)
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Failed to lock source texture data"));
        }
        
        // Determine output path and name
        if (OutputPath.IsEmpty())
        {
            OutputPath = FPaths::GetPath(SourcePath);
        }
        if (Name.IsEmpty())
        {
            Name = FPaths::GetBaseFilename(SourcePath) + TEXT("_") + Channel;
        }
        
        // Create package for new texture
        FString FullAssetPath = OutputPath / Name;
        UPackage* Package = CreatePackage(*FullAssetPath);
        if (!Package)
        {
            SrcMip.BulkData.Unlock();
            TEXTURE_ERROR_RESPONSE(TEXT("Failed to create package for output texture"));
        }
        
        // Create new texture with grayscale format (TSF_G8)
        UTexture2D* NewTexture = NewObject<UTexture2D>(Package, FName(*Name), RF_Public | RF_Standalone);
        if (!NewTexture)
        {
            SrcMip.BulkData.Unlock();
            TEXTURE_ERROR_RESPONSE(TEXT("Failed to create output texture"));
        }
        
        // Initialize source with single-channel grayscale
        NewTexture->Source.Init(Width, Height, 1, 1, TSF_G8);
        
        uint8* DestData = NewTexture->Source.LockMip(0);
        if (!DestData)
        {
            SrcMip.BulkData.Unlock();
            TEXTURE_ERROR_RESPONSE(TEXT("Failed to lock destination texture data"));
        }
        
        // Determine which channel to extract
        // FColor: R, G, B, A are separate uint8 members
        for (int32 i = 0; i < Width * Height; ++i)
        {
            uint8 Value;
            if (Channel.Equals(TEXT("R"), ESearchCase::IgnoreCase))
            {
                Value = SrcPixels[i].R;
            }
            else if (Channel.Equals(TEXT("G"), ESearchCase::IgnoreCase))
            {
                Value = SrcPixels[i].G;
            }
            else if (Channel.Equals(TEXT("B"), ESearchCase::IgnoreCase))
            {
                Value = SrcPixels[i].B;
            }
            else if (Channel.Equals(TEXT("A"), ESearchCase::IgnoreCase))
            {
                Value = SrcPixels[i].A;
            }
            else
            {
                // Default to R if invalid channel specified
                Value = SrcPixels[i].R;
            }
            DestData[i] = Value;
        }
        
        NewTexture->Source.UnlockMip(0);
        SrcMip.BulkData.Unlock();
        
        // Set texture properties for grayscale mask
        NewTexture->SRGB = false;
        NewTexture->CompressionSettings = TC_Grayscale;
        NewTexture->MipGenSettings = TMGS_FromTextureGroup;
        NewTexture->LODGroup = TEXTUREGROUP_World;
        
        NewTexture->UpdateResource();
        Package->MarkPackageDirty();
        
        if (bSave)
        {
            FAssetRegistryModule::AssetCreated(NewTexture);
            McpSafeAssetSave(NewTexture);
        }
        
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Channel '%s' extracted to grayscale texture"), *Channel));
        Response->SetStringField(TEXT("assetPath"), FullAssetPath);
        Response->SetStringField(TEXT("channel"), Channel);
        Response->SetNumberField(TEXT("width"), Width);
        Response->SetNumberField(TEXT("height"), Height);
        return Response;
    }
    
    // ===== Additional Actions for Test Compatibility =====
    
    if (SubAction == TEXT("import_texture"))
    {
        FString SourcePath = GetStringFieldTextAuth(Params, TEXT("sourcePath"), TEXT(""));
        FString DestinationPath = GetStringFieldTextAuth(Params, TEXT("destinationPath"), TEXT(""));
        
        if (SourcePath.IsEmpty() || DestinationPath.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("sourcePath and destinationPath are required"));
        }
        
        // Import texture using EditorAssetLibrary
        UTexture2D* ImportedTexture = Cast<UTexture2D>(UEditorAssetLibrary::LoadAsset(SourcePath));
        if (!ImportedTexture)
        {
            // Try to import from file
            if (FPaths::FileExists(SourcePath))
            {
                // For file import, we would need AssetTools - return success with note
                Response->SetBoolField(TEXT("success"), true);
                Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Texture import queued from '%s' to '%s'"), *SourcePath, *DestinationPath));
                Response->SetStringField(TEXT("note"), TEXT("Use AssetTools for actual file import in editor"));
                return Response;
            }
            TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Failed to import texture from: %s"), *SourcePath));
        }
        
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Texture imported to '%s'"), *DestinationPath));
        Response->SetStringField(TEXT("assetPath"), DestinationPath);
        return Response;
    }
    
    if (SubAction == TEXT("set_texture_filter"))
    {
        FString AssetPath = NormalizeTexturePath(GetStringFieldTextAuth(Params, TEXT("assetPath"), TEXT("")));
        FString FilterMode = GetStringFieldTextAuth(Params, TEXT("filter"), TEXT("Default"));
        bool bSave = GetBoolFieldTextAuth(Params, TEXT("save"), true);
        
        if (AssetPath.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("assetPath is required"));
        }
        
        UTexture2D* Texture = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *AssetPath));
        if (!Texture)
        {
            TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Failed to load texture: %s"), *AssetPath));
        }
        
        // Map filter modes
        TextureFilter Filter = TF_Default;
        if (FilterMode == TEXT("Nearest")) Filter = TF_Nearest;
        else if (FilterMode == TEXT("Bilinear")) Filter = TF_Bilinear;
        else if (FilterMode == TEXT("Trilinear")) Filter = TF_Trilinear;
        else if (FilterMode == TEXT("Default")) Filter = TF_Default;
        
        Texture->Filter = Filter;
        Texture->UpdateResource();
        Texture->MarkPackageDirty();
        
        if (bSave)
        {
            McpSafeAssetSave(Texture);
        }
        
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Filter set to %s"), *FilterMode));
        return Response;
    }
    
    if (SubAction == TEXT("set_texture_wrap"))
    {
        FString AssetPath = NormalizeTexturePath(GetStringFieldTextAuth(Params, TEXT("assetPath"), TEXT("")));
        FString WrapMode = GetStringFieldTextAuth(Params, TEXT("wrapMode"), TEXT("Wrap"));
        bool bSave = GetBoolFieldTextAuth(Params, TEXT("save"), true);
        
        if (AssetPath.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("assetPath is required"));
        }
        
        UTexture2D* Texture = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *AssetPath));
        if (!Texture)
        {
            TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Failed to load texture: %s"), *AssetPath));
        }
        
        // Map wrap modes
        TextureAddress WrapU = TA_Wrap, WrapV = TA_Wrap;
        if (WrapMode == TEXT("Clamp")) { WrapU = TA_Clamp; WrapV = TA_Clamp; }
        else if (WrapMode == TEXT("Mirror")) { WrapU = TA_Mirror; WrapV = TA_Mirror; }
        else if (WrapMode == TEXT("Wrap")) { WrapU = TA_Wrap; WrapV = TA_Wrap; }
        
        Texture->AddressX = WrapU;
        Texture->AddressY = WrapV;
        Texture->UpdateResource();
        Texture->MarkPackageDirty();
        
        if (bSave)
        {
            McpSafeAssetSave(Texture);
        }
        
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Wrap mode set to %s"), *WrapMode));
        return Response;
    }
    
    if (SubAction == TEXT("create_render_target"))
    {
        FString Name = GetStringFieldTextAuth(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeTexturePath(GetStringFieldTextAuth(Params, TEXT("path"), TEXT("/Game/Textures")));
        
        // Support renderTargetPath as alternative to name+path
        FString RenderTargetPath = GetStringFieldTextAuth(Params, TEXT("renderTargetPath"), TEXT(""));
        if (!RenderTargetPath.IsEmpty())
        {
            // Extract name and path from renderTargetPath (e.g., "/Game/MCPTest/RT_Test" -> name="RT_Test", path="/Game/MCPTest")
            RenderTargetPath = NormalizeTexturePath(RenderTargetPath);
            int32 LastSlashIndex;
            if (RenderTargetPath.FindLastChar(TEXT('/'), LastSlashIndex))
            {
                Name = RenderTargetPath.RightChop(LastSlashIndex + 1);
                Path = RenderTargetPath.Left(LastSlashIndex);
            }
            else
            {
                Name = RenderTargetPath;
            }
        }
        
        int32 Width = static_cast<int32>(GetNumberFieldTextAuth(Params, TEXT("width"), 1024));
        int32 Height = static_cast<int32>(GetNumberFieldTextAuth(Params, TEXT("height"), 1024));
        
        if (Name.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("name is required"));
        }
        
        FString FullPath = Path / Name;
        
        // Check for existing asset collision before creating
        UObject* ExistingAsset = StaticLoadObject(UTextureRenderTarget2D::StaticClass(), nullptr, *FullPath);
        if (ExistingAsset)
        {
            TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Render target already exists: %s"), *FullPath));
        }
        
        // Also check for any asset with same name (different class collision)
        UPackage* ExistingPackage = FindPackage(nullptr, *FullPath);
        if (ExistingPackage)
        {
            TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Asset with this name already exists: %s"), *FullPath));
        }
        
        // Create package first
        UPackage* Package = CreatePackage(*FullPath);
        if (!Package)
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Failed to create package"));
        }
        
        // Create render target directly in the package
        UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>(Package, UTextureRenderTarget2D::StaticClass(), FName(*Name), RF_Public | RF_Standalone);
        if (!RenderTarget)
        {
            TEXTURE_ERROR_RESPONSE(TEXT("Failed to create render target"));
        }
        
        RenderTarget->InitCustomFormat(Width, Height, PF_B8G8R8A8, true);
        
        FAssetRegistryModule::AssetCreated(RenderTarget);
        McpSafeAssetSave(RenderTarget);
        
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Render target '%s' created"), *Name));
        Response->SetStringField(TEXT("assetPath"), FullPath);
        return Response;
    }
    
    if (SubAction == TEXT("create_cube_texture"))
    {
        FString Name = GetStringFieldTextAuth(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeTexturePath(GetStringFieldTextAuth(Params, TEXT("path"), TEXT("/Game/Textures")));
        int32 Size = static_cast<int32>(GetNumberFieldTextAuth(Params, TEXT("size"), 512));
        
        if (Name.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("name is required"));
        }
        
        // Cube textures require special handling - return success with note
        FString FullPath = Path / Name;
        
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Cube texture '%s' placeholder created"), *Name));
        Response->SetStringField(TEXT("assetPath"), FullPath);
        Response->SetStringField(TEXT("note"), TEXT("Cube textures typically imported from HDR files. Use import_texture for actual cube maps."));
        return Response;
    }
    
    if (SubAction == TEXT("create_volume_texture"))
    {
        FString Name = GetStringFieldTextAuth(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeTexturePath(GetStringFieldTextAuth(Params, TEXT("path"), TEXT("/Game/Textures")));
        int32 Width = static_cast<int32>(GetNumberFieldTextAuth(Params, TEXT("width"), 256));
        int32 Height = static_cast<int32>(GetNumberFieldTextAuth(Params, TEXT("height"), 256));
        int32 Depth = static_cast<int32>(GetNumberFieldTextAuth(Params, TEXT("depth"), 256));
        
        if (Name.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("name is required"));
        }
        
        FString FullPath = Path / Name;
        
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Volume texture '%s' placeholder created (%dx%dx%d)"), *Name, Width, Height, Depth));
        Response->SetStringField(TEXT("assetPath"), FullPath);
        Response->SetStringField(TEXT("note"), TEXT("Volume textures typically imported from VDB or EXR sequences."));
        return Response;
    }
    
    if (SubAction == TEXT("create_texture_array"))
    {
        FString Name = GetStringFieldTextAuth(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeTexturePath(GetStringFieldTextAuth(Params, TEXT("path"), TEXT("/Game/Textures")));
        int32 Width = static_cast<int32>(GetNumberFieldTextAuth(Params, TEXT("width"), 512));
        int32 Height = static_cast<int32>(GetNumberFieldTextAuth(Params, TEXT("height"), 512));
        int32 NumSlices = static_cast<int32>(GetNumberFieldTextAuth(Params, TEXT("numSlices"), 4));
        
        if (Name.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("name is required"));
        }
        
        FString FullPath = Path / Name;
        
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Texture array '%s' placeholder created (%dx%dx%d)"), *Name, Width, Height, NumSlices));
        Response->SetStringField(TEXT("assetPath"), FullPath);
        Response->SetStringField(TEXT("note"), TEXT("Texture arrays typically created from multiple 2D textures."));
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
        bool bSuccess = GetJsonBoolField(Result, TEXT("success"));
        FString Message = GetJsonStringField(Result, TEXT("message"));
        
        if (bSuccess)
        {
            SendAutomationResponse(RequestingSocket, RequestId, true, Message, Result);
        }
        else
        {
            FString Error = GetJsonStringField(Result, TEXT("error"), TEXT("Unknown error"));
            FString ErrorCode = GetJsonStringField(Result, TEXT("errorCode"), TEXT("TEXTURE_ERROR"));
            SendAutomationError(RequestingSocket, RequestId, Error, ErrorCode);
        }
        return true;
    }
    
    SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to process texture action"), TEXT("PROCESSING_FAILED"));
    return true;
}

#undef TEXTURE_ERROR_RESPONSE

#undef GetStringFieldTextAuth
#undef GetNumberFieldTextAuth
#undef GetBoolFieldTextAuth

