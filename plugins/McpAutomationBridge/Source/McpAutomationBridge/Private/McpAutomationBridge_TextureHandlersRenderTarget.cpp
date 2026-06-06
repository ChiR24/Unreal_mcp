#include "McpAutomationBridge_TextureHandlersShared.h"

namespace McpTextureHandlers
{
TSharedPtr<FJsonObject> HandleCreateRenderTarget(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    FString Name = GetStringFieldTextAuth(Params, TEXT("name"), TEXT(""));
    FString Path = NormalizeTexturePath(GetStringFieldTextAuth(Params, TEXT("path"), TEXT("/Game/Textures")));

    FString RenderTargetPath = GetStringFieldTextAuth(Params, TEXT("renderTargetPath"), TEXT(""));
    if (!RenderTargetPath.IsEmpty())
    {
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

    double WidthValue = GetNumberFieldTextAuth(Params, TEXT("width"), 1024);
    double HeightValue = GetNumberFieldTextAuth(Params, TEXT("height"), 1024);
    FString FormatStr = GetStringFieldTextAuth(Params, TEXT("format"), TEXT("RGBA8"));
    bool bSave = GetBoolFieldTextAuth(Params, TEXT("save"), true);

    if (Name.IsEmpty())
    {
        TEXTURE_ERROR_RESPONSE(TEXT("name is required"));
    }

    if (!FMath::IsFinite(WidthValue) || !FMath::IsFinite(HeightValue) ||
        WidthValue != FMath::FloorToDouble(WidthValue) || HeightValue != FMath::FloorToDouble(HeightValue))
    {
        TEXTURE_ERROR_RESPONSE(TEXT("width and height must be finite whole numbers"));
    }

    const int32 Width = static_cast<int32>(WidthValue);
    const int32 Height = static_cast<int32>(HeightValue);
    const int32 MaxRenderTargetDimension = 8192;
    const int32 MaxWidth = FMath::Min(MaxRenderTargetDimension, FMath::Max(1, GTextureRenderTarget2DMaxSizeX));
    const int32 MaxHeight = FMath::Min(MaxRenderTargetDimension, FMath::Max(1, GTextureRenderTarget2DMaxSizeY));
    if (Width < 1 || Height < 1 || Width > MaxWidth || Height > MaxHeight)
    {
        TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("width and height must be between 1 and %d x %d"), MaxWidth, MaxHeight));
    }

    EPixelFormat Format = PF_B8G8R8A8;
    if (FormatStr.Equals(TEXT("RGBA8"), ESearchCase::IgnoreCase))
    {
        Format = PF_B8G8R8A8;
    }
    else if (FormatStr.Equals(TEXT("RGBA16F"), ESearchCase::IgnoreCase) || FormatStr.Equals(TEXT("FloatRGBA"), ESearchCase::IgnoreCase))
    {
        Format = PF_FloatRGBA;
    }
    else if (FormatStr.Equals(TEXT("RGBA32F"), ESearchCase::IgnoreCase))
    {
        Format = PF_A32B32G32R32F;
    }
    else if (FormatStr.Equals(TEXT("R8"), ESearchCase::IgnoreCase))
    {
        Format = PF_G8;
    }
    else if (FormatStr.Equals(TEXT("RG8"), ESearchCase::IgnoreCase))
    {
        Format = PF_R8G8;
    }
    else if (FormatStr.Equals(TEXT("R16F"), ESearchCase::IgnoreCase))
    {
        Format = PF_R16F;
    }
    else if (FormatStr.Equals(TEXT("RG16F"), ESearchCase::IgnoreCase))
    {
        Format = PF_G16R16F;
    }
    else if (FormatStr.Equals(TEXT("R32F"), ESearchCase::IgnoreCase))
    {
        Format = PF_R32_FLOAT;
    }
    else if (FormatStr.Equals(TEXT("RG32F"), ESearchCase::IgnoreCase))
    {
        Format = PF_G32R32F;
    }
    else if (FormatStr.Equals(TEXT("A2B10G10R10"), ESearchCase::IgnoreCase) || FormatStr.Equals(TEXT("RGB10A2"), ESearchCase::IgnoreCase))
    {
        Format = PF_A2B10G10R10;
    }
    else
    {
        TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Unsupported render target format: %s"), *FormatStr));
    }

    if (!FTextureRenderTargetResource::IsSupportedFormat(Format))
    {
        TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Unsupported render target pixel format: %s"), *FormatStr));
    }

    const int32 BytesPerPixel = GPixelFormats[Format].BlockBytes;
    const int64 PixelCount = static_cast<int64>(Width) * static_cast<int64>(Height);
    const int64 MaxAllocationBytes = 512ll * 1024ll * 1024ll;
    if (BytesPerPixel <= 0 || PixelCount > MaxAllocationBytes / static_cast<int64>(BytesPerPixel))
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Render target dimensions exceed the safe allocation limit"));
    }

    FString SanitizedPath = Path.Equals(TEXT("/Game")) ? Path : SanitizeProjectRelativePath(Path);
    if (SanitizedPath.IsEmpty())
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Invalid path: contains traversal or invalid characters"));
    }
    Path = SanitizedPath;
    if (!Path.Equals(TEXT("/Game")) && !Path.StartsWith(TEXT("/Game/")))
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Invalid path: render targets can only be created under /Game"));
    }

    const FString TrimmedName = Name.TrimStartAndEnd();
    FString SanitizedName = SanitizeAssetName(Name);
    if (SanitizedName.IsEmpty() || SanitizedName != TrimmedName)
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Invalid name: must be a valid Unreal asset name without sanitization"));
    }
    Name = SanitizedName;

    FString FullPath = Path / Name;
    FText PackageValidationReason;
    if (!FPackageName::IsValidLongPackageName(FullPath, true, &PackageValidationReason))
    {
        TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Invalid package path: %s"), *PackageValidationReason.ToString()));
    }
    FString FullObjectPath = FString::Printf(TEXT("%s.%s"), *FullPath, *Name);

    UTextureRenderTarget2D* ExistingRenderTarget = FindObject<UTextureRenderTarget2D>(nullptr, *FullObjectPath);
    const FAssetData ExistingAssetData = GetTextureAssetDataByObjectPath(FullObjectPath);
    if (!ExistingRenderTarget && ExistingAssetData.IsValid())
    {
        ExistingRenderTarget = Cast<UTextureRenderTarget2D>(ExistingAssetData.GetAsset());
    }
    if (ExistingRenderTarget)
    {
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Render target already exists: %s"), *FullPath));
        McpHandlerUtils::AddVerification(Response, ExistingRenderTarget);
        Response->SetNumberField(TEXT("width"), ExistingRenderTarget->SizeX);
        Response->SetNumberField(TEXT("height"), ExistingRenderTarget->SizeY);
        return Response;
    }

    if (ExistingAssetData.IsValid())
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Asset with this name already exists: %s"), *FullPath));
        Response->SetStringField(TEXT("errorCode"), TEXT("ASSET_ALREADY_EXISTS"));
        return Response;
    }

    UPackage* Package = CreatePackage(*FullPath);
    if (!Package)
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Failed to create package"));
    }

    UObject* InMemoryCollision = FindObject<UObject>(Package, *Name);
    if (InMemoryCollision)
    {
        if (UTextureRenderTarget2D* InMemoryRenderTarget = Cast<UTextureRenderTarget2D>(InMemoryCollision))
        {
            Response->SetBoolField(TEXT("success"), true);
            Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Render target already exists: %s"), *FullPath));
            McpHandlerUtils::AddVerification(Response, InMemoryRenderTarget);
            Response->SetNumberField(TEXT("width"), InMemoryRenderTarget->SizeX);
            Response->SetNumberField(TEXT("height"), InMemoryRenderTarget->SizeY);
            return Response;
        }

        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Asset with this name already exists: %s"), *FullPath));
        Response->SetStringField(TEXT("errorCode"), TEXT("ASSET_ALREADY_EXISTS"));
        return Response;
    }

    UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>(Package, UTextureRenderTarget2D::StaticClass(), FName(*Name), RF_Public | RF_Standalone);
    if (!RenderTarget)
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Failed to create render target"));
    }

    RenderTarget->InitCustomFormat(Width, Height, Format, false);
    RenderTarget->UpdateResourceImmediate(true);
    RenderTarget->MarkPackageDirty();

    FAssetRegistryModule::AssetCreated(RenderTarget);
    if (bSave)
    {
        McpSafeAssetSave(RenderTarget);
    }

    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Render target '%s' created"), *Name));
    Response->SetBoolField(TEXT("saved"), bSave);
    McpHandlerUtils::AddVerification(Response, RenderTarget);
    Response->SetNumberField(TEXT("width"), RenderTarget->SizeX);
    Response->SetNumberField(TEXT("height"), RenderTarget->SizeY);
    return Response;
}
}
