#include "McpAutomationBridge_GeometryHandlers.h"

#if WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleLatticeDeform(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    ADynamicMeshActor* TargetActor = nullptr;
    UDynamicMeshComponent* DMC = nullptr;
    UDynamicMesh* Mesh = nullptr;
    if (!ResolveDynamicMeshForGeometry(Self, RequestId, ActorName, Socket, TargetActor, DMC, Mesh))
    {
        return true;
    }

    const int32 LatticeResolution = FMath::Clamp(GetIntFieldGeom(Payload, TEXT("latticeResolution"), 3), 2, 16);
    const double Weight = FMath::Clamp(GetNumberFieldGeom(Payload, TEXT("weight"), GetNumberFieldGeom(Payload, TEXT("strength"), 0.25)), -2.0, 2.0);
    const FBox BBox = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(Mesh);
    if (!BBox.IsValid)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh bounds are invalid"), TEXT("MESH_INVALID"));
        return true;
    }

    const FVector BoundsSize = BBox.GetSize();
    const double MaxExtent = FMath::Max3(BoundsSize.X, BoundsSize.Y, BoundsSize.Z);
    const FVector Center = Payload->HasField(TEXT("position"))
        ? ReadVectorFromPayload(Payload, TEXT("position"), BBox.GetCenter())
        : BBox.GetCenter();
    const double Radius = FMath::Max(GetNumberFieldGeom(Payload, TEXT("radius"), MaxExtent * 0.75), KINDA_SMALL_NUMBER);
    const FVector DisplacementAxis = AxisVectorFromPayload(Payload);
    const double Amplitude = MaxExtent * 0.25 * Weight;

    FGeometryScriptIndexList VertexIDList;
    bool bHasGaps = false;
    UGeometryScriptLibrary_MeshQueryFunctions::GetAllVertexIDs(Mesh, VertexIDList, bHasGaps);
    const int32 NumVertices = VertexIDList.List.IsValid() ? VertexIDList.List->Num() : 0;
    int32 VerticesModified = 0;

    TargetActor->Modify();
    DMC->Modify();

    for (int32 Index = 0; Index < NumVertices; ++Index)
    {
        const int32 VertexID = (*VertexIDList.List)[Index];
        bool bIsValid = false;
        const FVector OriginalPos = UGeometryScriptLibrary_MeshQueryFunctions::GetVertexPosition(Mesh, VertexID, bIsValid);
        if (!bIsValid)
        {
            continue;
        }

        const FVector Normalized(
            BoundsSize.X > KINDA_SMALL_NUMBER ? (OriginalPos.X - BBox.Min.X) / BoundsSize.X : 0.5,
            BoundsSize.Y > KINDA_SMALL_NUMBER ? (OriginalPos.Y - BBox.Min.Y) / BoundsSize.Y : 0.5,
            BoundsSize.Z > KINDA_SMALL_NUMBER ? (OriginalPos.Z - BBox.Min.Z) / BoundsSize.Z : 0.5);
        const double Falloff = FMath::Clamp(1.0 - FVector::Dist(OriginalPos, Center) / Radius, 0.0, 1.0);
        const double LatticeWave =
            FMath::Sin(Normalized.X * PI * static_cast<double>(LatticeResolution)) *
            FMath::Sin(Normalized.Y * PI * static_cast<double>(LatticeResolution)) *
            FMath::Sin((Normalized.Z + 0.5) * PI * static_cast<double>(LatticeResolution));
        const FVector NewPos = OriginalPos + DisplacementAxis * (LatticeWave * Falloff * Amplitude);

        bool bVertexValid = false;
        UGeometryScriptLibrary_MeshBasicEditFunctions::SetVertexPosition(Mesh, VertexID, NewPos, bVertexValid, true);
        if (bVertexValid)
        {
            VerticesModified++;
        }
    }

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
    UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(Mesh, FGeometryScriptCalculateNormalsOptions(), false, nullptr);
#else
    UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(Mesh, FGeometryScriptCalculateNormalsOptions(), nullptr);
#endif
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("latticeResolution"), LatticeResolution);
    Result->SetNumberField(TEXT("weight"), Weight);
    Result->SetNumberField(TEXT("verticesModified"), VerticesModified);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Lattice deform applied"), Result);
    return true;
}

bool HandleDisplaceByTexture(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    const FString TexturePath = GetStringFieldGeom(Payload, TEXT("texturePath"));
    if (TexturePath.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("texturePath required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    ADynamicMeshActor* TargetActor = nullptr;
    UDynamicMeshComponent* DMC = nullptr;
    UDynamicMesh* Mesh = nullptr;
    if (!ResolveDynamicMeshForGeometry(Self, RequestId, ActorName, Socket, TargetActor, DMC, Mesh))
    {
        return true;
    }

    FString ResolvedTexturePath;
    UTexture2D* Texture = ResolveGeometryTexture(TexturePath, ResolvedTexturePath);
    if (!Texture)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Texture not found or invalid: %s"), *TexturePath), TEXT("TEXTURE_NOT_FOUND"));
        return true;
    }

    double Probe = 0.0;
    if (!SampleTextureLuminance(Texture, 0.5, 0.5, Probe))
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("Texture source format is not supported for displacement"), TEXT("TEXTURE_FORMAT_UNSUPPORTED"));
        return true;
    }

    const double HeightScale = GetNumberFieldGeom(Payload, TEXT("heightScale"), GetNumberFieldGeom(Payload, TEXT("strength"), 10.0));
    const double Midpoint = FMath::Clamp(GetNumberFieldGeom(Payload, TEXT("midpoint"), 0.5), 0.0, 1.0);
    const FVector DisplacementAxis = AxisVectorFromPayload(Payload);
    const FBox BBox = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(Mesh);
    if (!BBox.IsValid)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh bounds are invalid"), TEXT("MESH_INVALID"));
        return true;
    }

    const FVector BoundsSize = BBox.GetSize();
    FGeometryScriptIndexList VertexIDList;
    bool bHasGaps = false;
    UGeometryScriptLibrary_MeshQueryFunctions::GetAllVertexIDs(Mesh, VertexIDList, bHasGaps);
    const int32 NumVertices = VertexIDList.List.IsValid() ? VertexIDList.List->Num() : 0;
    int32 VerticesModified = 0;

    TargetActor->Modify();
    DMC->Modify();

    for (int32 Index = 0; Index < NumVertices; ++Index)
    {
        const int32 VertexID = (*VertexIDList.List)[Index];
        bool bIsValid = false;
        const FVector OriginalPos = UGeometryScriptLibrary_MeshQueryFunctions::GetVertexPosition(Mesh, VertexID, bIsValid);
        if (!bIsValid)
        {
            continue;
        }

        const double U = BoundsSize.X > KINDA_SMALL_NUMBER ? (OriginalPos.X - BBox.Min.X) / BoundsSize.X : 0.5;
        const double V = BoundsSize.Y > KINDA_SMALL_NUMBER ? (OriginalPos.Y - BBox.Min.Y) / BoundsSize.Y : 0.5;
        double Luminance = 0.0;
        if (!SampleTextureLuminance(Texture, U, V, Luminance))
        {
            continue;
        }

        const FVector NewPos = OriginalPos + DisplacementAxis * ((Luminance - Midpoint) * HeightScale);
        bool bVertexValid = false;
        UGeometryScriptLibrary_MeshBasicEditFunctions::SetVertexPosition(Mesh, VertexID, NewPos, bVertexValid, true);
        if (bVertexValid)
        {
            VerticesModified++;
        }
    }

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
    UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(Mesh, FGeometryScriptCalculateNormalsOptions(), false, nullptr);
#else
    UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(Mesh, FGeometryScriptCalculateNormalsOptions(), nullptr);
#endif
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("texturePath"), ResolvedTexturePath.IsEmpty() ? TexturePath : ResolvedTexturePath);
    Result->SetNumberField(TEXT("heightScale"), HeightScale);
    Result->SetNumberField(TEXT("midpoint"), Midpoint);
    Result->SetNumberField(TEXT("verticesModified"), VerticesModified);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Texture displacement applied"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Chamfer Operation
// -------------------------------------------------------------------------
} // namespace McpGeometryHandlers

#endif // WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT
