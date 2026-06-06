#include "McpAutomationBridge_TextureHandlersShared.h"

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
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("CreateEmptyTexture: %s"), *DimensionError);
        return nullptr;
    }
    Width = SafeWidth;
    Height = SafeHeight;

    FString FullPath = NormalizeTexturePath(PackagePath / TextureName);
    const FString SanitizedFullPath = SanitizeProjectRelativePath(FullPath);
    if (SanitizedFullPath.IsEmpty())
    {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("CreateEmptyTexture: Invalid path rejected: %s"), *FullPath);
        return nullptr;
    }
    FullPath = SanitizedFullPath;

    UPackage* Package = CreatePackage(*FullPath);
    if (!Package)
    {
        return nullptr;
    }

    const EPixelFormat Format = bHDR ? PF_FloatRGBA : PF_B8G8R8A8;
    UTexture2D* NewTexture = FindObject<UTexture2D>(Package, *TextureName);
    if (!NewTexture)
    {
        NewTexture = NewObject<UTexture2D>(Package, UTexture2D::StaticClass(), FName(*TextureName), RF_Public | RF_Standalone);
    }
    if (!NewTexture)
    {
        return nullptr;
    }
    NewTexture->SetFlags(RF_Public | RF_Standalone);

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
    FAssetRegistryModule::AssetCreated(Texture);
    Texture->MarkPackageDirty();
    if (McpSafeAssetSave(Texture))
    {
        return true;
    }

    UPackage* Package = Texture->GetOutermost();
    if (!Package)
    {
        return false;
    }

    TArray<UPackage*> PackagesToSave;
    PackagesToSave.Add(Package);
    const FEditorFileUtils::EPromptReturnCode PromptSaveResult =
        FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, false, false);
    const bool bPromptSaveSucceeded = PromptSaveResult == FEditorFileUtils::PR_Success;
    const bool bEditorSaveSucceeded =
        !bPromptSaveSucceeded && UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, false);

    FString PackageFilename;
    const bool bHasFilename = FPackageName::TryConvertLongPackageNameToFilename(
        Package->GetName(), PackageFilename, FPackageName::GetAssetPackageExtension());
    const bool bExistsOnDisk = bHasFilename &&
        IFileManager::Get().FileExists(*FPaths::ConvertRelativePathToFull(PackageFilename));
    if (!bPromptSaveSucceeded && !bEditorSaveSucceeded && !bExistsOnDisk)
    {
        return false;
    }

    if (bHasFilename)
    {
        TArray<FString> FilesToScan;
        FilesToScan.Add(PackageFilename);
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get()
            .ScanFilesSynchronous(FilesToScan, true);
    }
    return true;
}
}
