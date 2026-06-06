#include "McpAutomationBridge_TextureHandlersShared.h"

namespace McpTextureHandlers
{
namespace
{
bool SanitizeOptionalPath(FString& Path, const TCHAR* ErrorPrefix, TSharedPtr<FJsonObject>& Response)
{
    if (Path.IsEmpty())
    {
        return true;
    }
    const FString Sanitized = SanitizeProjectRelativePath(Path);
    if (Sanitized.IsEmpty())
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Invalid %s path: contains traversal or invalid characters"), ErrorPrefix));
        return false;
    }
    Path = Sanitized;
    return true;
}

UTexture2D* LoadChannelTexture(const FString& Path, const TCHAR* ChannelName, TSharedPtr<FJsonObject>& Response)
{
    if (Path.IsEmpty())
    {
        return nullptr;
    }
    UTexture2D* Texture = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *Path));
    if (!Texture)
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to load %s texture: %s"), ChannelName, *Path));
    }
    return Texture;
}

TArray<uint8> GetChannelData(UTexture2D* Texture, int32 ChannelIndex)
{
    TArray<uint8> Data;
    if (!Texture || !Texture->Source.IsValid())
    {
        return Data;
    }
    if (Texture->IsStreamable())
    {
        Texture->SetForceMipLevelsToBeResident(30.0f);
    }
    const int32 Width = Texture->GetSizeX();
    const int32 Height = Texture->GetSizeY();
    Data.SetNumUninitialized(Width * Height);
    const uint8* MipData = Texture->Source.LockMipReadOnly(0);
    if (!MipData)
    {
        Data.Empty();
        return Data;
    }
    for (int32 Index = 0; Index < Width * Height; ++Index)
    {
        Data[Index] = MipData[Index * 4 + ChannelIndex];
    }
    Texture->Source.UnlockMip(0);
    return Data;
}
}

TSharedPtr<FJsonObject> HandleChannelPack(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    FString RedPath = NormalizeTexturePath(GetStringFieldTextAuth(Params, TEXT("redTexture"), TEXT("")));
    FString GreenPath = NormalizeTexturePath(GetStringFieldTextAuth(Params, TEXT("greenTexture"), TEXT("")));
    FString BluePath = NormalizeTexturePath(GetStringFieldTextAuth(Params, TEXT("blueTexture"), TEXT("")));
    FString AlphaPath = NormalizeTexturePath(GetStringFieldTextAuth(Params, TEXT("alphaTexture"), TEXT("")));
    FString Name = GetStringFieldTextAuth(Params, TEXT("name"), TEXT("ChannelPacked"));
    FString Path = NormalizeTexturePath(GetStringFieldTextAuth(Params, TEXT("path"), TEXT("/Game/Textures")));
    const bool bSave = GetBoolFieldTextAuth(Params, TEXT("save"), true);

    if (Name.IsEmpty())
    {
        TEXTURE_ERROR_RESPONSE(TEXT("name is required"));
    }
    if (!SanitizeOptionalPath(RedPath, TEXT("redTexture"), Response) ||
        !SanitizeOptionalPath(GreenPath, TEXT("greenTexture"), Response) ||
        !SanitizeOptionalPath(BluePath, TEXT("blueTexture"), Response) ||
        !SanitizeOptionalPath(AlphaPath, TEXT("alphaTexture"), Response))
    {
        return Response;
    }
    if (RedPath.IsEmpty() && GreenPath.IsEmpty() && BluePath.IsEmpty() && AlphaPath.IsEmpty())
    {
        TEXTURE_ERROR_RESPONSE(TEXT("At least one source texture (redTexture, greenTexture, blueTexture, or alphaTexture) is required"));
    }

    UTexture2D* RedTex = LoadChannelTexture(RedPath, TEXT("red"), Response);
    if (!RedPath.IsEmpty() && !RedTex) return Response;
    UTexture2D* GreenTex = LoadChannelTexture(GreenPath, TEXT("green"), Response);
    if (!GreenPath.IsEmpty() && !GreenTex) return Response;
    UTexture2D* BlueTex = LoadChannelTexture(BluePath, TEXT("blue"), Response);
    if (!BluePath.IsEmpty() && !BlueTex) return Response;
    UTexture2D* AlphaTex = LoadChannelTexture(AlphaPath, TEXT("alpha"), Response);
    if (!AlphaPath.IsEmpty() && !AlphaTex) return Response;

    int32 Width = 1024;
    int32 Height = 1024;
    if (RedTex) { Width = RedTex->GetSizeX(); Height = RedTex->GetSizeY(); }
    else if (GreenTex) { Width = GreenTex->GetSizeX(); Height = GreenTex->GetSizeY(); }
    else if (BlueTex) { Width = BlueTex->GetSizeX(); Height = BlueTex->GetSizeY(); }
    else if (AlphaTex) { Width = AlphaTex->GetSizeX(); Height = AlphaTex->GetSizeY(); }

    UTexture2D* OutputTexture = CreateEmptyTexture(Path, Name, Width, Height, false);
    if (!OutputTexture)
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Failed to create output texture"));
    }
    OutputTexture->PreEditChange(nullptr);
    OutputTexture->SRGB = false;
    OutputTexture->CompressionSettings = TC_Masks;
    OutputTexture->PostEditChange();

    uint8* OutData = OutputTexture->Source.LockMip(0);
    if (!OutData)
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Failed to lock output texture data"));
    }

    const TArray<uint8> RedData = GetChannelData(RedTex, 2);
    const TArray<uint8> GreenData = GetChannelData(GreenTex, 1);
    const TArray<uint8> BlueData = GetChannelData(BlueTex, 0);
    const TArray<uint8> AlphaData = GetChannelData(AlphaTex, 3);
    for (int32 Index = 0; Index < Width * Height; ++Index)
    {
        const int32 Idx = Index * 4;
        OutData[Idx + 0] = BlueData.Num() > Index ? BlueData[Index] : 0;
        OutData[Idx + 1] = GreenData.Num() > Index ? GreenData[Index] : 0;
        OutData[Idx + 2] = RedData.Num() > Index ? RedData[Index] : 0;
        OutData[Idx + 3] = AlphaData.Num() > Index ? AlphaData[Index] : 255;
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
}
