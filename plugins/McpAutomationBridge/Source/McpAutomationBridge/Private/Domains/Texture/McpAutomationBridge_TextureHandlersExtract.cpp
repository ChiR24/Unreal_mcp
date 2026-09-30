#include "Domains/Texture/McpAutomationBridge_TextureHandlersShared.h"

namespace McpTextureHandlers
{
TSharedPtr<FJsonObject> HandleChannelExtract(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    const FString Channel = GetJsonStringField(Params, TEXT("channel"), TEXT("R"));
    const TArray<int32> Offsets = ChannelOffsets(Channel);
    const int32 Offset = Offsets.Num() == 1 ? Offsets[0] : 2;
    FString SourcePath;
    FString Error;
    UTexture2D* SourceTexture = LoadSourceTexture(GetJsonStringField(Params, TEXT("assetPath")), TEXT("assetPath"), SourcePath, Error);
    FString Path;
    FString Name;
    if (!SourceTexture ||
        !ResolveOutputTarget(Params, FPaths::GetPath(SourcePath), FPaths::GetBaseFilename(SourcePath) + TEXT("_") + Channel, Path, Name, Error))
    {
        TEXTURE_ERROR_RESPONSE(Error);
    }

    const int32 Width = SourceTexture->Source.GetSizeX();
    const int32 Height = SourceTexture->Source.GetSizeY();
    const FString FullAssetPath = Path / Name;
    UPackage* Package = CreatePackage(*FullAssetPath);
    UTexture2D* NewTexture = Package ? NewObject<UTexture2D>(Package, FName(*Name), RF_Public | RF_Standalone) : nullptr;
    if (!NewTexture)
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Failed to create output texture"));
    }
    NewTexture->Source.Init(Width, Height, 1, 1, TSF_G8);

    const TArray<uint8> SrcData = ReadSourceBGRA(SourceTexture);
    uint8* DestData = NewTexture->Source.LockMip(0);
    if (SrcData.IsEmpty() || !DestData)
    {
        if (DestData) NewTexture->Source.UnlockMip(0);
        TEXTURE_ERROR_RESPONSE(TEXT("Failed to lock texture data"));
    }
    for (int32 i = 0; i < Width * Height; ++i)
    {
        DestData[i] = SrcData[i * 4 + Offset];
    }
    NewTexture->Source.UnlockMip(0);

    NewTexture->SRGB = false;
    NewTexture->CompressionSettings = TC_Grayscale;
    NewTexture->MipGenSettings = TMGS_FromTextureGroup;
    NewTexture->LODGroup = TEXTUREGROUP_World;
    NewTexture->UpdateResource();
    Package->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(NewTexture);
    McpSafeAssetSave(NewTexture);

    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Channel '%s' extracted to grayscale texture"), *Channel));
    Response->SetStringField(TEXT("assetPath"), FullAssetPath);
    Response->SetStringField(TEXT("channel"), Channel);
    Response->SetNumberField(TEXT("width"), Width);
    Response->SetNumberField(TEXT("height"), Height);
    return Response;
}
}
