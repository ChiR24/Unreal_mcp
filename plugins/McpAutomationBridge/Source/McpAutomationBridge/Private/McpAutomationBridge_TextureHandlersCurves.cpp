#include "McpAutomationBridge_TextureHandlersShared.h"

namespace McpTextureHandlers
{
TSharedPtr<FJsonObject> HandleAdjustCurves(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
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

    TArray<float> InputPointsR, OutputPointsR;
    TArray<float> InputPointsG, OutputPointsG;
    TArray<float> InputPointsB, OutputPointsB;

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
        if (InputArr.Num() == 0 || OutputArr.Num() == 0)
        {
            InputArr = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
            OutputArr = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
        }
    };

    if (Params->HasField(TEXT("inputR")))
    {
        ParseCurvePoints(TEXT("inputR"), TEXT("outputR"), InputPointsR, OutputPointsR);
        ParseCurvePoints(TEXT("inputG"), TEXT("outputG"), InputPointsG, OutputPointsG);
        ParseCurvePoints(TEXT("inputB"), TEXT("outputB"), InputPointsB, OutputPointsB);
    }
    else
    {
        TArray<float> MasterInput, MasterOutput;
        ParseCurvePoints(TEXT("input"), TEXT("output"), MasterInput, MasterOutput);
        InputPointsR = MasterInput; OutputPointsR = MasterOutput;
        InputPointsG = MasterInput; OutputPointsG = MasterOutput;
        InputPointsB = MasterInput; OutputPointsB = MasterOutput;
    }

    auto BuildLUT = [](const TArray<float>& Input, const TArray<float>& Output) -> TArray<uint8> {
        TArray<uint8> LUT;
        LUT.SetNum(256);

        if (Input.Num() < 2 || Output.Num() < 2 || Input.Num() != Output.Num())
        {
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
        FTexture2DMipMap& SrcMip = SourceTexture->GetPlatformData()->Mips[0];
        const uint8* SrcData = static_cast<const uint8*>(SrcMip.BulkData.LockReadOnly());
        FMemory::Memcpy(MipData, SrcData, Width * Height * 4);
        SrcMip.BulkData.Unlock();
    }

    int32 NumPixels = Width * Height;
    for (int32 i = 0; i < NumPixels; ++i)
    {
        int32 Idx = i * 4;
        MipData[Idx + 0] = LUT_B[MipData[Idx + 0]];
        MipData[Idx + 1] = LUT_G[MipData[Idx + 1]];
        MipData[Idx + 2] = LUT_R[MipData[Idx + 2]];
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
}
