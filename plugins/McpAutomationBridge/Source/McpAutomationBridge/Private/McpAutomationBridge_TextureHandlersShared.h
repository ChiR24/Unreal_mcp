#pragma once

#include "McpVersionCompatibility.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "EditorAssetLibrary.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "FileHelpers.h"
#include "HAL/PlatformFileManager.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Misc/PackageName.h"
#include "StaticMeshResources.h"
#include "TextureResource.h"
#include "UObject/SoftObjectPath.h"

#if __has_include("Factories/Texture2dFactoryNew.h")
#include "Factories/Texture2dFactoryNew.h"
#else
#include "Factories/Texture2DFactoryNew.h"
#endif

#define TEXTURE_ERROR_RESPONSE(Msg) \
    do \
    { \
        Response->SetBoolField(TEXT("success"), false); \
        Response->SetStringField(TEXT("error"), Msg); \
        return Response; \
    } while (false)

namespace McpTextureHandlers
{
FString GetStringField(const TSharedPtr<FJsonObject>& Obj, const FString& FieldName, const FString& Default = TEXT(""));
double GetNumberField(const TSharedPtr<FJsonObject>& Obj, const FString& FieldName, double Default = 0.0);
bool GetBoolField(const TSharedPtr<FJsonObject>& Obj, const FString& FieldName, bool Default = false);

bool ValidateGeneratedTextureDimensions(double WidthValue, double HeightValue,
                                        const TCHAR* WidthName, const TCHAR* HeightName,
                                        int32& OutWidth, int32& OutHeight,
                                        FString& OutError);
bool ValidateTextureIterationCount(double Value, const TCHAR* Name,
                                   int32 MinValue, int32 MaxValue,
                                   int32& OutValue, FString& OutError);
FString NormalizeTexturePath(const FString& Path);
FAssetData GetTextureAssetDataByObjectPath(const FString& ObjectPath);
UTexture2D* CreateEmptyTexture(const FString& PackagePath, const FString& TextureName, int32 Width, int32 Height, bool bHDR);
bool UpdateTextureBGRA8(UTexture2D* Texture, int32 Width, int32 Height, const TArray<uint8>& Pixels);
bool SaveTextureAsset(UTexture2D* Texture);
float FBMNoise(float X, float Y, int32 Octaves, float Persistence, float Lacunarity, int32 Seed);

TSharedPtr<FJsonObject> HandleCreateNoiseTexture(const TSharedPtr<FJsonObject>& Params);
TSharedPtr<FJsonObject> HandleCreateGradientTexture(const TSharedPtr<FJsonObject>& Params);
TSharedPtr<FJsonObject> HandleCreatePatternTexture(const TSharedPtr<FJsonObject>& Params);
TSharedPtr<FJsonObject> HandleCreateNormalFromHeight(const TSharedPtr<FJsonObject>& Params);
TSharedPtr<FJsonObject> HandleTextureSettingsAction(const FString& SubAction, const TSharedPtr<FJsonObject>& Params);
TSharedPtr<FJsonObject> HandleTextureInfoAction(const FString& SubAction, const TSharedPtr<FJsonObject>& Params);
TSharedPtr<FJsonObject> HandleResizeTexture(const TSharedPtr<FJsonObject>& Params);
TSharedPtr<FJsonObject> HandleTextureColorAction(const FString& SubAction, const TSharedPtr<FJsonObject>& Params);
TSharedPtr<FJsonObject> HandleTextureFilterAction(const FString& SubAction, const TSharedPtr<FJsonObject>& Params);
TSharedPtr<FJsonObject> HandleChannelPack(const TSharedPtr<FJsonObject>& Params);
TSharedPtr<FJsonObject> HandleCombineTextures(const TSharedPtr<FJsonObject>& Params);
TSharedPtr<FJsonObject> HandleAdjustCurves(const TSharedPtr<FJsonObject>& Params);
TSharedPtr<FJsonObject> HandleChannelExtract(const TSharedPtr<FJsonObject>& Params);
TSharedPtr<FJsonObject> HandleTextureImportAndSamplerAction(const FString& SubAction, const TSharedPtr<FJsonObject>& Params);
TSharedPtr<FJsonObject> HandleCreateRenderTarget(const TSharedPtr<FJsonObject>& Params);
TSharedPtr<FJsonObject> HandleTexturePlaceholderAction(const FString& SubAction, const TSharedPtr<FJsonObject>& Params);
TSharedPtr<FJsonObject> HandleCreateAoFromMesh(const TSharedPtr<FJsonObject>& Params);
}

#define GetStringFieldTextAuth McpTextureHandlers::GetStringField
#define GetNumberFieldTextAuth McpTextureHandlers::GetNumberField
#define GetBoolFieldTextAuth McpTextureHandlers::GetBoolField
