#include "McpAutomationBridge_TextureHandlersShared.h"

namespace McpTextureHandlers
{
TSharedPtr<FJsonObject> HandleTextureFilterAction(const FString& SubAction, const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    FString AssetPath = GetStringFieldTextAuth(Params, TEXT("assetPath"), TEXT(""));
    AssetPath = SanitizeProjectRelativePath(AssetPath);
    if (AssetPath.IsEmpty())
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Invalid assetPath: contains traversal or invalid characters"));
    }

    int32 Radius = 2;
    float Amount = static_cast<float>(GetNumberFieldTextAuth(Params, TEXT("amount"), 1.0));
    FString ValidationError;
    if (SubAction == TEXT("blur") &&
        !ValidateTextureIterationCount(GetNumberFieldTextAuth(Params, TEXT("radius"), 2),
                                       TEXT("radius"), 1, 10, Radius, ValidationError))
    {
        TEXTURE_ERROR_RESPONSE(ValidationError);
    }
    const bool bSave = GetBoolFieldTextAuth(Params, TEXT("save"), true);

    UTexture2D* Texture = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *AssetPath));
    if (!Texture)
    {
        TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Failed to load texture: %s"), *AssetPath));
    }
    if (!Texture->Source.IsValid())
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Texture has no source data - may be compressed or not fully loaded"));
    }
    if (Texture->IsStreamable())
    {
        Texture->SetForceMipLevelsToBeResident(30.0f);
    }

    const int32 Width = Texture->GetSizeX();
    const int32 Height = Texture->GetSizeY();
    uint8* MipData = Texture->Source.LockMip(0);
    if (!MipData)
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Failed to lock texture mip data - texture may be compressed or streaming"));
    }

    TArray<uint8> OriginalData;
    const int32 DataSize = Width * Height * 4;
    OriginalData.SetNumUninitialized(DataSize);
    FMemory::Memcpy(OriginalData.GetData(), MipData, DataSize);

    if (SubAction == TEXT("blur"))
    {
        Radius = FMath::Clamp(Radius, 1, 10);
        const int32 KernelSize = Radius * 2 + 1;
        const float KernelWeight = 1.0f / (KernelSize * KernelSize);
        for (int32 Y = 0; Y < Height; ++Y)
        {
            for (int32 X = 0; X < Width; ++X)
            {
                float SumR = 0.0f, SumG = 0.0f, SumB = 0.0f;
                for (int32 KY = -Radius; KY <= Radius; ++KY)
                {
                    for (int32 KX = -Radius; KX <= Radius; ++KX)
                    {
                        const int32 SampleX = FMath::Clamp(X + KX, 0, Width - 1);
                        const int32 SampleY = FMath::Clamp(Y + KY, 0, Height - 1);
                        const int32 SampleIdx = (SampleY * Width + SampleX) * 4;
                        SumB += OriginalData[SampleIdx + 0];
                        SumG += OriginalData[SampleIdx + 1];
                        SumR += OriginalData[SampleIdx + 2];
                    }
                }
                const int32 DstIdx = (Y * Width + X) * 4;
                MipData[DstIdx + 0] = static_cast<uint8>(SumB * KernelWeight);
                MipData[DstIdx + 1] = static_cast<uint8>(SumG * KernelWeight);
                MipData[DstIdx + 2] = static_cast<uint8>(SumR * KernelWeight);
            }
        }
    }
    else
    {
        Amount = FMath::Clamp(Amount, 0.0f, 5.0f);
        for (int32 Y = 1; Y < Height - 1; ++Y)
        {
            for (int32 X = 1; X < Width - 1; ++X)
            {
                const int32 CenterIdx = (Y * Width + X) * 4;
                const int32 LeftIdx = (Y * Width + X - 1) * 4;
                const int32 RightIdx = (Y * Width + X + 1) * 4;
                const int32 TopIdx = ((Y - 1) * Width + X) * 4;
                const int32 BottomIdx = ((Y + 1) * Width + X) * 4;
                for (int32 c = 0; c < 3; ++c)
                {
                    const float Sharpened = OriginalData[CenterIdx + c] * (1.0f + 4.0f * Amount) -
                        Amount * (OriginalData[LeftIdx + c] + OriginalData[RightIdx + c] +
                                  OriginalData[TopIdx + c] + OriginalData[BottomIdx + c]);
                    MipData[CenterIdx + c] = static_cast<uint8>(FMath::Clamp(Sharpened, 0.0f, 255.0f));
                }
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
    Response->SetStringField(TEXT("message"), SubAction == TEXT("blur")
        ? FString::Printf(TEXT("Blur applied (radius: %d)"), Radius)
        : FString::Printf(TEXT("Sharpen applied (amount: %.2f)"), Amount));
    Response->SetStringField(TEXT("assetPath"), AssetPath);
    return Response;
}
}
