#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "EditorAssetLibrary.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "FileHelpers.h"
#include "Misc/PackageName.h"
#include "StaticMeshResources.h"
#include "TextureResource.h"
#include "UObject/SoftObjectPath.h"


#define TEXTURE_ERROR_RESPONSE(Msg) \
    do \
    { \
        Response->SetBoolField(TEXT("success"), false); \
        Response->SetStringField(TEXT("error"), Msg); \
        return Response; \
    } while (false)

namespace McpTextureHandlers
{

bool ValidateGeneratedTextureDimensions(double WidthValue, double HeightValue,
                                        const TCHAR* WidthName, const TCHAR* HeightName,
                                        int32& OutWidth, int32& OutHeight,
                                        FString& OutError);
bool ValidateTextureIterationCount(double Value, const TCHAR* Name,
                                   int32 MinValue, int32 MaxValue,
                                   int32& OutValue, FString& OutError);
FString NormalizeTexturePath(const FString& Path);
// RawPath as a texture whose source mip is 8-bit BGRA, the layout every pixel operation
// walks; null with OutError set otherwise.
UTexture2D* LoadSourceTexture(const FString& RawPath, const TCHAR* Field, FString& OutPath, FString& OutError);
// Where a generated texture goes: outputPath (a full asset path) when given, else path/name.
bool ResolveOutputTarget(const TSharedPtr<FJsonObject>& Params, const FString& DefaultPath, const FString& DefaultName,
                         FString& OutPath, FString& OutName, FString& OutError);
// BGRA byte offsets a channel param names: R|Red, G|Green, B|Blue, A|Alpha; anything else is RGB.
TArray<int32> ChannelOffsets(const FString& Channel);
FAssetData GetTextureAssetDataByObjectPath(const FString& ObjectPath);
UTexture2D* CreateEmptyTexture(const FString& PackagePath, const FString& TextureName, int32 Width, int32 Height, bool bHDR);
bool UpdateTextureBGRA8(UTexture2D* Texture, int32 Width, int32 Height, const TArray<uint8>& Pixels);
bool SaveTextureAsset(UTexture2D* Texture);
float FBMNoise(float X, float Y, int32 Octaves, float Persistence, float Lacunarity, int32 Seed);

TSharedPtr<FJsonObject> HandleCreateNoiseTexture(const TSharedPtr<FJsonObject>& Params);
TSharedPtr<FJsonObject> HandleCreateGradientTexture(const TSharedPtr<FJsonObject>& Params);
TSharedPtr<FJsonObject> HandleCreatePatternTexture(const TSharedPtr<FJsonObject>& Params);
TSharedPtr<FJsonObject> HandleCreateNormalFromHeight(const TSharedPtr<FJsonObject>& Params);
TSharedPtr<FJsonObject> HandleCreateAoFromMesh(const TSharedPtr<FJsonObject>& Params);
TSharedPtr<FJsonObject> HandleTextureSettingsAction(const FString& SubAction, const TSharedPtr<FJsonObject>& Params);
TSharedPtr<FJsonObject> HandleTextureInfoAction(const FString& SubAction, const TSharedPtr<FJsonObject>& Params);
TSharedPtr<FJsonObject> HandleResizeTexture(const TSharedPtr<FJsonObject>& Params);
TSharedPtr<FJsonObject> HandleTextureColorAction(const FString& SubAction, const TSharedPtr<FJsonObject>& Params);
TSharedPtr<FJsonObject> HandleTextureFilterAction(const FString& SubAction, const TSharedPtr<FJsonObject>& Params);
TSharedPtr<FJsonObject> HandleChannelPack(const TSharedPtr<FJsonObject>& Params);
TSharedPtr<FJsonObject> HandleCombineTextures(const TSharedPtr<FJsonObject>& Params);
TSharedPtr<FJsonObject> HandleAdjustCurves(const TSharedPtr<FJsonObject>& Params);
TSharedPtr<FJsonObject> HandleChannelExtract(const TSharedPtr<FJsonObject>& Params);
TSharedPtr<FJsonObject> HandleCreateRenderTarget(const TSharedPtr<FJsonObject>& Params);
}

