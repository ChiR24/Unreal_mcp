#include "Domains/Environment/McpAutomationBridge_EnvironmentHandlersShared.h"

#if WITH_EDITOR
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInterface.h"

namespace McpEnvironmentHandlers {

namespace {
template <typename TEnum>
FString McpEnumName(TEnum Value)
{
    const UEnum *Enum = StaticEnum<TEnum>();
    return Enum ? Enum->GetNameStringByValue(static_cast<int64>(Value)) : FString();
}

TSharedPtr<FJsonObject> McpMakeParameterEntry(const FMaterialParameterInfo &Info, bool bResolved)
{
    TSharedPtr<FJsonObject> Entry = McpHandlerUtils::CreateResultObject();
    Entry->SetStringField(TEXT("name"), Info.Name.ToString());
    Entry->SetBoolField(TEXT("resolved"), bResolved);
    return Entry;
}
} // namespace

// get_material_details / inspect_object on a UMaterialInterface: blend mode,
// shading model, two-sided, domain/parent and every scalar/vector/texture
// parameter with its current (instance-overridden or inherited) value.
void McpDescribeMaterialAsset(UMaterialInterface *Material, TSharedPtr<FJsonObject> Resp)
{
    if (!Material || !Resp.IsValid())
    {
        return;
    }
    Resp->SetStringField(TEXT("assetType"), TEXT("Material"));
    Resp->SetStringField(TEXT("blendMode"), McpEnumName(Material->GetBlendMode()));
    const FMaterialShadingModelField ShadingModels = Material->GetShadingModels();
    const int32 ShadingModelCount = ShadingModels.CountShadingModels();
    Resp->SetStringField(TEXT("shadingModel"), ShadingModelCount > 0
        ? McpEnumName(ShadingModels.GetFirstShadingModel()) : FString(TEXT("None")));
    Resp->SetNumberField(TEXT("shadingModelCount"), ShadingModelCount);
    Resp->SetBoolField(TEXT("twoSided"), Material->IsTwoSided());
    if (UMaterial *BaseMaterial = Material->GetMaterial())
    {
        Resp->SetStringField(TEXT("baseMaterial"), BaseMaterial->GetPathName());
        Resp->SetStringField(TEXT("materialDomain"), McpEnumName(BaseMaterial->MaterialDomain.GetValue()));
    }
    UMaterialInstance *Instance = Cast<UMaterialInstance>(Material);
    Resp->SetBoolField(TEXT("isMaterialInstance"), Instance != nullptr);
    if (Instance)
    {
        Resp->SetStringField(TEXT("parent"), Instance->Parent ? Instance->Parent->GetPathName() : TEXT(""));
    }

    TArray<FMaterialParameterInfo> Infos;
    TArray<FGuid> Ids;
    TArray<TSharedPtr<FJsonValue>> Scalars;
    Material->GetAllScalarParameterInfo(Infos, Ids);
    for (const FMaterialParameterInfo &Info : Infos)
    {
        float Value = 0.0f;
        const bool bResolved = Material->GetScalarParameterValue(FHashedMaterialParameterInfo(Info), Value);
        TSharedPtr<FJsonObject> Entry = McpMakeParameterEntry(Info, bResolved);
        Entry->SetNumberField(TEXT("value"), Value);
        Scalars.Add(MakeShared<FJsonValueObject>(Entry));
    }
    Infos.Reset();
    Ids.Reset();
    TArray<TSharedPtr<FJsonValue>> Vectors;
    Material->GetAllVectorParameterInfo(Infos, Ids);
    for (const FMaterialParameterInfo &Info : Infos)
    {
        FLinearColor Color(0.0f, 0.0f, 0.0f, 0.0f);
        const bool bResolved = Material->GetVectorParameterValue(FHashedMaterialParameterInfo(Info), Color);
        TSharedPtr<FJsonObject> Entry = McpMakeParameterEntry(Info, bResolved);
        Entry->SetObjectField(TEXT("value"), McpHandlerUtils::LinearColorToJson(Color));
        Vectors.Add(MakeShared<FJsonValueObject>(Entry));
    }
    Infos.Reset();
    Ids.Reset();
    TArray<TSharedPtr<FJsonValue>> Textures;
    Material->GetAllTextureParameterInfo(Infos, Ids);
    for (const FMaterialParameterInfo &Info : Infos)
    {
        UTexture *Texture = nullptr;
        const bool bResolved = Material->GetTextureParameterValue(FHashedMaterialParameterInfo(Info), Texture);
        TSharedPtr<FJsonObject> Entry = McpMakeParameterEntry(Info, bResolved);
        Entry->SetStringField(TEXT("value"), Texture ? Texture->GetPathName() : TEXT(""));
        Textures.Add(MakeShared<FJsonValueObject>(Entry));
    }
    Resp->SetArrayField(TEXT("scalarParameters"), Scalars);
    Resp->SetArrayField(TEXT("vectorParameters"), Vectors);
    Resp->SetArrayField(TEXT("textureParameters"), Textures);
    Resp->SetNumberField(TEXT("parameterCount"), Scalars.Num() + Vectors.Num() + Textures.Num());
}

// get_texture_details / inspect_object on a UTexture: dimensions, pixel format,
// sRGB, mips, compression and LOD group.
void McpDescribeTextureAsset(UTexture *Texture, TSharedPtr<FJsonObject> Resp)
{
    if (!Texture || !Resp.IsValid())
    {
        return;
    }
    Resp->SetStringField(TEXT("assetType"), TEXT("Texture"));
    Resp->SetNumberField(TEXT("width"), Texture->GetSurfaceWidth());
    Resp->SetNumberField(TEXT("height"), Texture->GetSurfaceHeight());
    Resp->SetBoolField(TEXT("sRGB"), Texture->SRGB != 0);
    Resp->SetStringField(TEXT("compressionSettings"), McpEnumName(Texture->CompressionSettings.GetValue()));
    Resp->SetStringField(TEXT("lodGroup"), McpEnumName(Texture->LODGroup.GetValue()));
    Resp->SetStringField(TEXT("mipGenSettings"), McpEnumName(Texture->MipGenSettings.GetValue()));
    Resp->SetBoolField(TEXT("neverStream"), Texture->NeverStream != 0);
    Resp->SetNumberField(TEXT("lodBias"), Texture->LODBias);
    FString Format;
    if (UTexture2D *Texture2D = Cast<UTexture2D>(Texture))
    {
        Resp->SetNumberField(TEXT("sizeX"), Texture2D->GetSizeX());
        Resp->SetNumberField(TEXT("sizeY"), Texture2D->GetSizeY());
        Resp->SetNumberField(TEXT("mipCount"), Texture2D->GetNumMips());
        if (const UEnum *FormatEnum = UTexture::GetPixelFormatEnum())
        {
            Format = FormatEnum->GetNameStringByValue(static_cast<int64>(Texture2D->GetPixelFormat()));
        }
    }
    Resp->SetStringField(TEXT("format"), Format);
}

} // namespace McpEnvironmentHandlers
#endif
