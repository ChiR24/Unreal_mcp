#include "McpAutomationBridge_TextureHandlersShared.h"

namespace McpTextureHandlers
{
TSharedPtr<FJsonObject> HandleChannelExtract(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
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

    int32 Width = SourceTexture->GetSizeX();
    int32 Height = SourceTexture->GetSizeY();
    if (!SourceTexture->Source.IsValid())
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Source texture has no source data - may be compressed or not fully loaded"));
    }

    if (SourceTexture->IsStreamable())
    {
        SourceTexture->SetForceMipLevelsToBeResident(30.0f);
    }

    const uint8* SrcData = SourceTexture->Source.LockMipReadOnly(0);
    if (!SrcData)
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Failed to lock source texture data - texture may be compressed or streaming"));
    }

    if (OutputPath.IsEmpty())
    {
        OutputPath = FPaths::GetPath(SourcePath);
    }
    if (Name.IsEmpty())
    {
        Name = FPaths::GetBaseFilename(SourcePath) + TEXT("_") + Channel;
    }

    FString FullAssetPath = OutputPath / Name;
    UPackage* Package = CreatePackage(*FullAssetPath);
    if (!Package)
    {
        SourceTexture->Source.UnlockMip(0);
        TEXTURE_ERROR_RESPONSE(TEXT("Failed to create package for output texture"));
    }

    UTexture2D* NewTexture = NewObject<UTexture2D>(Package, FName(*Name), RF_Public | RF_Standalone);
    if (!NewTexture)
    {
        SourceTexture->Source.UnlockMip(0);
        TEXTURE_ERROR_RESPONSE(TEXT("Failed to create output texture"));
    }

    NewTexture->Source.Init(Width, Height, 1, 1, TSF_G8);

    uint8* DestData = NewTexture->Source.LockMip(0);
    if (!DestData)
    {
        SourceTexture->Source.UnlockMip(0);
        TEXTURE_ERROR_RESPONSE(TEXT("Failed to lock destination texture data"));
    }

    for (int32 i = 0; i < Width * Height; ++i)
    {
        int32 Idx = i * 4;
        uint8 Value;
        if (Channel.Equals(TEXT("R"), ESearchCase::IgnoreCase))
        {
            Value = SrcData[Idx + 2];
        }
        else if (Channel.Equals(TEXT("G"), ESearchCase::IgnoreCase))
        {
            Value = SrcData[Idx + 1];
        }
        else if (Channel.Equals(TEXT("B"), ESearchCase::IgnoreCase))
        {
            Value = SrcData[Idx + 0];
        }
        else if (Channel.Equals(TEXT("A"), ESearchCase::IgnoreCase))
        {
            Value = SrcData[Idx + 3];
        }
        else
        {
            Value = SrcData[Idx + 2];
        }
        DestData[i] = Value;
    }

    NewTexture->Source.UnlockMip(0);
    SourceTexture->Source.UnlockMip(0);

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
}
