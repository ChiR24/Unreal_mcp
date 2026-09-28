#include "Domains/Texture/McpAutomationBridge_TextureHandlersShared.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Engine/StaticMesh.h"
#include "Image/ImageDimensions.h"
#include "MeshDescription.h"
#include "MeshDescriptionToDynamicMesh.h"
#include "Sampling/MeshImageBakingCache.h"
#include "Sampling/MeshOcclusionMapBaker.h"

// create_ao_from_mesh: a real ray-traced ambient occlusion bake of a static mesh into its own
// UV space, with the engine's occlusion baker (the one the Bake Mesh Maps tool uses).
namespace McpTextureHandlers
{
namespace
{
TSharedPtr<FJsonObject> AoBakeError(const TCHAR* Code, const FString& Message)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    Response->SetBoolField(TEXT("success"), false);
    Response->SetStringField(TEXT("error"), Message);
    Response->SetStringField(TEXT("errorCode"), Code);
    return Response;
}

// True when every vertex lies on or behind the planes of the sampled triangles: a convex mesh
// cannot occlude itself, so its bake is flat white by nature and not a failed bake.
// ponytail: samples at most 512 triangle planes, so a mostly convex mesh can pass; a full hull test
// is the upgrade if a flat bake is ever accepted wrongly.
bool IsMeshConvexForAo(const UE::Geometry::FDynamicMesh3& Mesh)
{
    const double Tolerance = FMath::Max(Mesh.GetBounds().DiagonalLength() * 1e-3, 1e-4);
    const int32 Stride = FMath::Max(1, Mesh.TriangleCount() / 512);
    int32 Seen = 0;
    for (const int32 TID : Mesh.TriangleIndicesItr())
    {
        if (Seen++ % Stride != 0) continue;
        const FVector3d Normal = Mesh.GetTriNormal(TID);
        const FVector3d Origin = Mesh.GetTriCentroid(TID);
        for (const int32 VID : Mesh.VertexIndicesItr())
        {
            if ((Mesh.GetVertex(VID) - Origin).Dot(Normal) > Tolerance) return false;
        }
    }
    return true;
}
}

TSharedPtr<FJsonObject> HandleCreateAoFromMesh(const TSharedPtr<FJsonObject>& Params)
{
    FString Path;
    FString Name;
    FString Error;
    int32 Width = 0;
    int32 Height = 0;
    int32 Samples = 0;
    if (!ResolveOutputTarget(Params, TEXT("/Game/Textures"), FString(), Path, Name, Error) ||
        !ValidateGeneratedTextureDimensions(GetJsonNumberField(Params, TEXT("width"), 1024), GetJsonNumberField(Params, TEXT("height"), 1024),
                                            TEXT("width"), TEXT("height"), Width, Height, Error) ||
        !ValidateTextureIterationCount(GetJsonNumberField(Params, TEXT("samples"), 64), TEXT("samples"), 1, 1024, Samples, Error))
    {
        return AoBakeError(TEXT("INVALID_ARGUMENT"), Error);
    }
    const double RayDistance = GetJsonNumberField(Params, TEXT("rayDistance"), 0.0);
    const int32 UVChannel = static_cast<int32>(GetJsonNumberField(Params, TEXT("uvChannel"), 0.0));
    if (!FMath::IsFinite(RayDistance) || RayDistance < 0.0)
    {
        return AoBakeError(TEXT("INVALID_ARGUMENT"), TEXT("rayDistance must be 0 (unlimited) or a positive distance in cm."));
    }
    const FString MeshPath = NormalizeTexturePath(GetJsonStringField(Params, TEXT("meshPath")));
    UStaticMesh* StaticMesh = MeshPath.IsEmpty() ? nullptr : LoadObject<UStaticMesh>(nullptr, *MeshPath);
    const FMeshDescription* Description = StaticMesh ? StaticMesh->GetMeshDescription(0) : nullptr;
    if (!Description)
    {
        return AoBakeError(StaticMesh ? TEXT("NO_SOURCE_MESH") : TEXT("MESH_NOT_FOUND"), StaticMesh
            ? FString::Printf(TEXT("'%s' has no LOD 0 source mesh to bake from."), *MeshPath)
            : FString::Printf(TEXT("No static mesh at '%s'."), *GetJsonStringField(Params, TEXT("meshPath"))));
    }

    UE::Geometry::FDynamicMesh3 Mesh;
    FMeshDescriptionToDynamicMesh Converter;
    Converter.Convert(Description, Mesh);
    const int32 NumUVLayers = Mesh.HasAttributes() ? Mesh.Attributes()->NumUVLayers() : 0;
    if (UVChannel < 0 || UVChannel >= NumUVLayers || !Mesh.Attributes()->PrimaryNormals())
    {
        return AoBakeError(TEXT("NO_UV_CHANNEL"), FString::Printf(
            TEXT("'%s' has %d UV channels (0 to %d); uvChannel %d does not exist."), *MeshPath, NumUVLayers, NumUVLayers - 1, UVChannel));
    }
    UE::Geometry::FDynamicMeshAABBTree3 Spatial(&Mesh, true);
    UE::Geometry::FMeshImageBakingCache Cache;
    Cache.SetDetailMesh(&Mesh, &Spatial);
    Cache.SetBakeTargetMesh(&Mesh);
    Cache.SetDimensions(UE::Geometry::FImageDimensions(Width, Height));
    Cache.SetUVLayer(UVChannel);
    Cache.SetThickness(0.1);
    if (!Cache.ValidateCache())
    {
        return AoBakeError(TEXT("BAKE_FAILED"), TEXT("The mesh could not be sampled in its UV space."));
    }
    UE::Geometry::FMeshOcclusionMapBaker Baker;
    Baker.SetCache(&Cache);
    Baker.OcclusionType = UE::Geometry::EOcclusionMapType::AmbientOcclusion;
    Baker.NumOcclusionRays = Samples;
    // 0 keeps the engine default (unlimited): passing 0 through would give zero-length rays and a white bake.
    if (RayDistance > 0.0)
    {
        Baker.MaxDistance = RayDistance;
    }
    Baker.Bake();
    const TUniquePtr<UE::Geometry::TImageBuilder<FVector3f>>& Image = Baker.GetResult(UE::Geometry::FMeshOcclusionMapBaker::EResult::AmbientOcclusion);
    if (!Image.IsValid())
    {
        return AoBakeError(TEXT("BAKE_FAILED"), TEXT("The occlusion baker produced no image."));
    }

    TArray<uint8> Pixels;
    Pixels.SetNumUninitialized(Width * Height * 4);
    double MinValue = 1.0;
    double MaxValue = 0.0;
    double Sum = 0.0;
    for (int32 Index = 0; Index < Width * Height; ++Index)
    {
        const double Value = FMath::Clamp(static_cast<double>(Image->GetPixel(static_cast<int64>(Index)).X), 0.0, 1.0);
        MinValue = FMath::Min(MinValue, Value);
        MaxValue = FMath::Max(MaxValue, Value);
        Sum += Value;
        const uint8 Gray = static_cast<uint8>(FMath::RoundToInt(Value * 255.0));
        Pixels[Index * 4 + 0] = Gray;
        Pixels[Index * 4 + 1] = Gray;
        Pixels[Index * 4 + 2] = Gray;
        Pixels[Index * 4 + 3] = 255;
    }
    const bool bConvex = MinValue >= MaxValue && IsMeshConvexForAo(Mesh);
    if (MinValue >= MaxValue && !bConvex)
    {
        return AoBakeError(TEXT("AO_BAKE_FLAT"), FString::Printf(
            TEXT("The bake of '%s' came out flat (every texel %.3f) although the mesh can occlude itself. Raise rayDistance or samples, or check uvChannel %d is unwrapped."),
            *MeshPath, MinValue, UVChannel));
    }

    UTexture2D* Texture = CreateEmptyTexture(Path, Name, Width, Height, false);
    if (!Texture)
    {
        return AoBakeError(TEXT("CREATE_FAILED"), FString::Printf(TEXT("Could not create the texture %s/%s."), *Path, *Name));
    }
    Texture->SRGB = false; // occlusion is linear data
    if (!UpdateTextureBGRA8(Texture, Width, Height, Pixels))
    {
        return AoBakeError(TEXT("CREATE_FAILED"), TEXT("Failed to write the baked pixels into the texture."));
    }
    const bool bSaved = SaveTextureAsset(Texture);
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Baked ambient occlusion of %s into %s (%dx%d, %d rays)"),
        *MeshPath, *Texture->GetPathName(), Width, Height, Samples));
    Response->SetStringField(TEXT("assetPath"), Texture->GetPathName());
    Response->SetNumberField(TEXT("width"), Width);
    Response->SetNumberField(TEXT("height"), Height);
    Response->SetNumberField(TEXT("minValue"), MinValue);
    Response->SetNumberField(TEXT("meanValue"), Sum / (static_cast<double>(Width) * Height));
    Response->SetNumberField(TEXT("maxValue"), MaxValue);
    Response->SetBoolField(TEXT("convexMesh"), bConvex);
    Response->SetBoolField(TEXT("saved"), bSaved);
    McpHandlerUtils::AddVerification(Response, Texture);
    return Response;
}
}
