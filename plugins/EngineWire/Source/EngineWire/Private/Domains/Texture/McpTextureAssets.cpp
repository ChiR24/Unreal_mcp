#include "Domains/Texture/McpTextureShared.h"

namespace McpTextureHandlers
{
UTexture2D* CreateEmptyTexture(const FString& PackagePath, const FString& TextureName, int32 Width, int32 Height, bool bHDR)
{
    int32 SafeWidth = 0;
    int32 SafeHeight = 0;
    FString DimensionError;
    if (!ValidateGeneratedTextureDimensions(static_cast<double>(Width), static_cast<double>(Height),
                                            TEXT("width"), TEXT("height"),
                                            SafeWidth, SafeHeight, DimensionError))
    {
        UE_LOG(LogEngineWireSubsystem, Warning, TEXT("CreateEmptyTexture: %s"), *DimensionError);
        return nullptr;
    }
    Width = SafeWidth;
    Height = SafeHeight;

    FString FullPath = NormalizeTexturePath(PackagePath / TextureName);
    const FString SanitizedFullPath = SanitizeProjectRelativePath(FullPath);
    if (SanitizedFullPath.IsEmpty())
    {
        UE_LOG(LogEngineWireSubsystem, Warning, TEXT("CreateEmptyTexture: Invalid path rejected: %s"), *FullPath);
        return nullptr;
    }
    FullPath = SanitizedFullPath;

    // A texture already on disk is loaded and refilled. A new object built over the unloaded one left
    // source data the editor refused to save ("bulkdata with an invalid payload") on the first
    // overwrite of every session; anything else at the path is never replaced.
    const FString ObjectPath = FullPath + TEXT(".") + TextureName;
    UTexture2D* NewTexture = LoadObject<UTexture2D>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
    if (!NewTexture && (FPackageName::DoesPackageExist(FullPath) || FindObject<UObject>(nullptr, *ObjectPath)))
    {
        UE_LOG(LogEngineWireSubsystem, Warning, TEXT("CreateEmptyTexture: %s holds an asset that is not a Texture2D; it is not replaced."), *FullPath);
        return nullptr;
    }
    // A new package through the shared helper: CreatePackage is a Fatal log for a name holding "//" (a name such as
    // "a//b" joined onto the folder), which the canonicalizer above lets through under /Game.
    FString CreateError, CreateCode;
    UPackage* Package = NewTexture ? NewTexture->GetOutermost() : McpCreatePackageForNewAsset(FullPath, CreateError, CreateCode);
    if (!Package)
    {
        UE_LOG(LogEngineWireSubsystem, Warning, TEXT("CreateEmptyTexture: %s"), *CreateError);
        return nullptr;
    }

    const EPixelFormat Format = bHDR ? PF_FloatRGBA : PF_B8G8R8A8;
    if (!NewTexture)
    {
        NewTexture = NewObject<UTexture2D>(Package, UTexture2D::StaticClass(), FName(*TextureName), RF_Public | RF_Standalone);
    }
    if (!NewTexture)
    {
        return nullptr;
    }
    NewTexture->SetFlags(RF_Public | RF_Standalone);
    // A texture loaded to be refilled may still be building from the call that made it: PreEditChange waits for that
    // build, as the engine asks before any change to a texture.
    NewTexture->PreEditChange(nullptr);

    if (!NewTexture->GetPlatformData())
    {
        NewTexture->SetPlatformData(new FTexturePlatformData());
    }
    NewTexture->GetPlatformData()->Mips.Empty();
    NewTexture->GetPlatformData()->SizeX = Width;
    NewTexture->GetPlatformData()->SizeY = Height;
    NewTexture->GetPlatformData()->PixelFormat = Format;

    FTexture2DMipMap* Mip = new FTexture2DMipMap();
    Mip->SizeX = Width;
    Mip->SizeY = Height;
    Mip->SizeZ = 1;
    NewTexture->GetPlatformData()->Mips.Add(Mip);

    const int32 BytesPerPixel = bHDR ? 16 : 4;
    const int32 DataSize = Width * Height * BytesPerPixel;
    Mip->BulkData.Lock(LOCK_READ_WRITE);
    void* TextureData = Mip->BulkData.Realloc(DataSize);
    FMemory::Memzero(TextureData, DataSize);
    Mip->BulkData.Unlock();

    NewTexture->Source.Init(Width, Height, 1, 1, bHDR ? TSF_RGBA16F : TSF_BGRA8);
    NewTexture->SRGB = !bHDR;
    NewTexture->CompressionSettings = bHDR ? TC_HDR : TC_Default;
    NewTexture->CompressionNone = true;
    NewTexture->NeverStream = true;
    NewTexture->MipGenSettings = TMGS_FromTextureGroup;
    NewTexture->LODGroup = TEXTUREGROUP_World;
    NewTexture->UpdateResource();
    NewTexture->PostEditChange();
    // PostEditChange starts this texture's build on a worker thread, and the caller goes on to set its colour space
    // and pixels. A colour space changed under that build fails a check in the engine's texture build task and takes the
    // editor down (an occlusion bake of 512 x 512 did), so the build is waited for here; the caller's pixel write or save
    // ends the edit with its own PostEditChange.
    NewTexture->PreEditChange(nullptr);
    Package->MarkPackageDirty();
    return NewTexture;
}

bool UpdateTextureBGRA8(UTexture2D* Texture, int32 Width, int32 Height, const TArray<uint8>& Pixels)
{
    if (!Texture || Pixels.Num() != Width * Height * 4 ||
        !Texture->GetPlatformData() || Texture->GetPlatformData()->Mips.Num() == 0)
    {
        return false;
    }

    Texture->Source.Init(Width, Height, 1, 1, TSF_BGRA8, Pixels.GetData());
    FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
    Mip.SizeX = Width;
    Mip.SizeY = Height;
    Mip.SizeZ = 1;
    Mip.BulkData.Lock(LOCK_READ_WRITE);
    void* TextureData = Mip.BulkData.Realloc(Pixels.Num());
    FMemory::Memcpy(TextureData, Pixels.GetData(), Pixels.Num());
    Mip.BulkData.Unlock();

    Texture->UpdateResource();
    Texture->PostEditChange();
    Texture->MarkPackageDirty();
    return true;
}

bool SaveTextureAsset(UTexture2D* Texture)
{
    if (!Texture)
    {
        return false;
    }

    Texture->PostEditChange();
    FlushRenderingCommands();
    Texture->MarkPackageDirty();
    // McpSafeAssetSave tries every save route and says yes only when one of them wrote the file. The
    // second round of the same routes that used to follow answered yes whenever an older version was
    // still on disk, so a texture whose five saves all failed was reported saved.
    return McpSafeAssetSave(Texture);
}

bool ReadGeneratedTextureTarget(const TSharedPtr<FJsonObject>& Params, FString& OutPath, FString& OutName, int32& OutWidth,
                                int32& OutHeight, FString& OutError)
{
    return ResolveOutputTarget(Params, TEXT("/Game/Textures"), FString(), OutPath, OutName, OutError) &&
           ValidateGeneratedTextureDimensions(GetJsonNumberField(Params, TEXT("width"), 1024),
                                              GetJsonNumberField(Params, TEXT("height"), 1024),
                                              TEXT("width"), TEXT("height"), OutWidth, OutHeight, OutError);
}

TSharedPtr<FJsonObject> FinishGeneratedTexture(TSharedPtr<FJsonObject> Response, const FString& Path, const FString& Name,
                                               int32 Width, int32 Height, const TArray<uint8>& Pixels, const TCHAR* Kind)
{
    UTexture2D* NewTexture = CreateEmptyTexture(Path, Name, Width, Height, false);
    if (!NewTexture)
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Failed to create texture"));
    }
    if (!UpdateTextureBGRA8(NewTexture, Width, Height, Pixels))
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Failed to update texture pixel data"));
    }
    if (!SaveTextureAsset(NewTexture))
    {
        TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Failed to save %s texture"), *FString(Kind).ToLower()));
    }

    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("message"), FString::Printf(TEXT("%s texture '%s' created"), Kind, *Name));
    McpHandlerUtils::AddVerification(Response, NewTexture);
    return Response;
}
}
