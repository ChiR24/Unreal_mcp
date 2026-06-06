#include "McpAutomationBridge_TextureHandlersShared.h"

#include "Engine/StaticMesh.h"

namespace McpTextureHandlers
{
TSharedPtr<FJsonObject> HandleCreateAoFromMesh(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    FString MeshPath = GetStringFieldTextAuth(Params, TEXT("meshPath"), TEXT(""));
    FString Name = GetStringFieldTextAuth(Params, TEXT("name"), TEXT(""));
    FString Path = NormalizeTexturePath(GetStringFieldTextAuth(Params, TEXT("path"), TEXT("/Game/Textures")));
    int32 Width = 0;
    int32 Height = 0;
    int32 SampleCount = 0;
    FString ValidationError;
    if (!ValidateGeneratedTextureDimensions(GetNumberFieldTextAuth(Params, TEXT("width"), 1024),
                                            GetNumberFieldTextAuth(Params, TEXT("height"), 1024),
                                            TEXT("width"), TEXT("height"),
                                            Width, Height, ValidationError))
    {
        TEXTURE_ERROR_RESPONSE(ValidationError);
    }
    const double SampleCountValue = Params->HasField(TEXT("samples"))
                                        ? GetNumberFieldTextAuth(Params, TEXT("samples"), 64)
                                        : GetNumberFieldTextAuth(Params, TEXT("sampleCount"), 64);
    if (!ValidateTextureIterationCount(SampleCountValue, TEXT("samples"), 1, 128,
                                       SampleCount, ValidationError))
    {
        TEXTURE_ERROR_RESPONSE(ValidationError);
    }
    float RayDistance = static_cast<float>(GetNumberFieldTextAuth(Params, TEXT("rayDistance"), 100.0));
    float Bias = static_cast<float>(GetNumberFieldTextAuth(Params, TEXT("bias"), 0.01));
    int32 UVChannel = static_cast<int32>(GetNumberFieldTextAuth(Params, TEXT("uvChannel"), 0));
    bool bSave = GetBoolFieldTextAuth(Params, TEXT("save"), true);

    if (MeshPath.IsEmpty())
    {
        TEXTURE_ERROR_RESPONSE(TEXT("meshPath is required"));
    }
    if (Name.IsEmpty())
    {
        TEXTURE_ERROR_RESPONSE(TEXT("name is required"));
    }

    FString SanitizedMeshPath = SanitizeProjectRelativePath(MeshPath);
    if (SanitizedMeshPath.IsEmpty())
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Invalid meshPath: contains traversal sequences or invalid characters"));
    }
    MeshPath = SanitizedMeshPath;

    FString SanitizedPath = SanitizeProjectRelativePath(Path);
    if (SanitizedPath.IsEmpty())
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Invalid path: contains traversal sequences or invalid characters"));
    }
    Path = SanitizedPath;

    FString SanitizedName = SanitizeAssetName(Name);
    if (SanitizedName.IsEmpty())
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Invalid name: contains invalid characters"));
    }
    Name = SanitizedName;

    UStaticMesh* SourceMesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *MeshPath));
    if (!SourceMesh)
    {
        TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Mesh not found: %s"), *MeshPath));
    }

    if (SourceMesh->GetRenderData() == nullptr ||
        SourceMesh->GetRenderData()->LODResources.Num() == 0 ||
        SourceMesh->GetRenderData()->LODResources[0].VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords() <= static_cast<uint32>(UVChannel))
    {
        TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Mesh has no UV channel %d or no render data"), UVChannel));
    }

    UTexture2D* AOTexture = CreateEmptyTexture(Path, Name, Width, Height, false);
    if (!AOTexture)
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Failed to create AO output texture"));
    }

    AOTexture->PreEditChange(nullptr);
    AOTexture->SRGB = false;
    AOTexture->CompressionSettings = TC_Grayscale;
    AOTexture->CompressionNone = true;
    AOTexture->NeverStream = true;
    AOTexture->MipGenSettings = TMGS_FromTextureGroup;
    AOTexture->LODGroup = TEXTUREGROUP_World;
    AOTexture->PostEditChange();
    AOTexture->UpdateResource();

    uint8* AOData = AOTexture->Source.LockMip(0);
    if (!AOData)
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Failed to lock AO texture for writing"));
    }

    const FStaticMeshLODResources& LOD = SourceMesh->GetRenderData()->LODResources[0];
    const FStaticMeshVertexBuffer& VertexBuffer = LOD.VertexBuffers.StaticMeshVertexBuffer;

    for (int32 i = 0; i < Width * Height * 4; ++i)
    {
        AOData[i] = 255;
    }

    int32 NumVertices = VertexBuffer.GetNumVertices();
    if (NumVertices > 0)
    {
        for (int32 y = 0; y < Height; ++y)
        {
            for (int32 x = 0; x < Width; ++x)
            {
                float U = static_cast<float>(x) / Width;
                float V = static_cast<float>(y) / Height;
                float Occlusion = 0.0f;
                int32 Samples = 0;

                for (int32 vIdx = 0; vIdx < NumVertices && Samples < SampleCount; ++vIdx)
                {
                    FVector2D UV = FVector2D::ZeroVector;
                    uint32 UVChannelIdx = static_cast<uint32>(UVChannel);
                    if (UVChannelIdx < VertexBuffer.GetNumTexCoords())
                    {
                        UV = FVector2D(
                            VertexBuffer.GetVertexUV(vIdx, UVChannelIdx).X,
                            VertexBuffer.GetVertexUV(vIdx, UVChannelIdx).Y
                        );
                    }

                    float Dist = FMath::Square(UV.X - U) + FMath::Square(UV.Y - V);
                    if (Dist < 0.001f)
                    {
                        Occlusion += 0.3f;
                    }
                    Samples++;
                }

                uint8 AOValue = static_cast<uint8>(FMath::Clamp(255.0f - Occlusion * 255.0f, 0.0f, 255.0f));
                int32 Idx = (y * Width + x) * 4;
                AOData[Idx + 0] = AOValue;
                AOData[Idx + 1] = AOValue;
                AOData[Idx + 2] = AOValue;
                AOData[Idx + 3] = 255;
            }
        }
    }

    AOTexture->Source.UnlockMip(0);
    AOTexture->UpdateResource();

    if (bSave)
    {
        FAssetRegistryModule::AssetCreated(AOTexture);
        McpSafeAssetSave(AOTexture);
    }

    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("message"), FString::Printf(TEXT("AO texture '%s' created from mesh '%s'"), *Name, *MeshPath));
    Response->SetStringField(TEXT("assetPath"), Path / Name);
    Response->SetNumberField(TEXT("width"), Width);
    Response->SetNumberField(TEXT("height"), Height);
    Response->SetStringField(TEXT("sourceMesh"), MeshPath);
    return Response;
}
}
