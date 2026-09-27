#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleLatticeDeform(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;
    auto [TargetActor, DMC, Mesh] = *Target;

    const int32 LatticeResolution = FMath::Clamp(GetJsonIntField(Payload, TEXT("latticeResolution"), 3), 2, 16);
    const double Weight = FMath::Clamp(GetJsonNumberField(Payload, TEXT("weight"), GetJsonNumberField(Payload, TEXT("strength"), 0.25)), -2.0, 2.0);
    const FBox BBox = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(Mesh);
    if (!BBox.IsValid)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh bounds are invalid"), TEXT("MESH_INVALID"));
        return true;
    }

    const FVector BoundsSize = BBox.GetSize();
    const double MaxExtent = FMath::Max3(BoundsSize.X, BoundsSize.Y, BoundsSize.Z);
    const FVector Center = Payload->HasField(TEXT("position"))
        ? ExtractVectorField(Payload, TEXT("position"), BBox.GetCenter())
        : BBox.GetCenter();
    const double Radius = FMath::Max(MaxExtent * 0.75, KINDA_SMALL_NUMBER);
    const FVector DisplacementAxis = AxisVectorFromPayload(Payload);
    const double Amplitude = MaxExtent * 0.25 * Weight;

    TargetActor->Modify();
    DMC->Modify();
    const int32 VerticesModified = DeformVertices(Mesh, [&](const FVector& OriginalPos)
    {
        const FVector Normalized(
            BoundsSize.X > KINDA_SMALL_NUMBER ? (OriginalPos.X - BBox.Min.X) / BoundsSize.X : 0.5,
            BoundsSize.Y > KINDA_SMALL_NUMBER ? (OriginalPos.Y - BBox.Min.Y) / BoundsSize.Y : 0.5,
            BoundsSize.Z > KINDA_SMALL_NUMBER ? (OriginalPos.Z - BBox.Min.Z) / BoundsSize.Z : 0.5);
        const double Falloff = FMath::Clamp(1.0 - FVector::Dist(OriginalPos, Center) / Radius, 0.0, 1.0);
        const double LatticeWave =
            FMath::Sin(Normalized.X * PI * static_cast<double>(LatticeResolution)) *
            FMath::Sin(Normalized.Y * PI * static_cast<double>(LatticeResolution)) *
            FMath::Sin((Normalized.Z + 0.5) * PI * static_cast<double>(LatticeResolution));
        return OriginalPos + DisplacementAxis * (LatticeWave * Falloff * Amplitude);
    });
    RecomputeMeshNormals(Mesh);
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
    const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    const FString TexturePath = GetJsonStringField(Payload, TEXT("texturePath"));
    if (TexturePath.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("texturePath required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;
    auto [TargetActor, DMC, Mesh] = *Target;

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

    const double HeightScale = GetJsonNumberField(Payload, TEXT("heightScale"), GetJsonNumberField(Payload, TEXT("strength"), 10.0));
    const double Midpoint = FMath::Clamp(GetJsonNumberField(Payload, TEXT("midpoint"), 0.5), 0.0, 1.0);
    const FVector DisplacementAxis = AxisVectorFromPayload(Payload);
    const FBox BBox = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(Mesh);
    if (!BBox.IsValid)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh bounds are invalid"), TEXT("MESH_INVALID"));
        return true;
    }

    const FVector BoundsSize = BBox.GetSize();
    TargetActor->Modify();
    DMC->Modify();
    const int32 VerticesModified = DeformVertices(Mesh, [&](const FVector& OriginalPos)
    {
        const double U = BoundsSize.X > KINDA_SMALL_NUMBER ? (OriginalPos.X - BBox.Min.X) / BoundsSize.X : 0.5;
        const double V = BoundsSize.Y > KINDA_SMALL_NUMBER ? (OriginalPos.Y - BBox.Min.Y) / BoundsSize.Y : 0.5;
        double Luminance = 0.0;
        return SampleTextureLuminance(Texture, U, V, Luminance)
            ? OriginalPos + DisplacementAxis * ((Luminance - Midpoint) * HeightScale)
            : OriginalPos;
    });
    RecomputeMeshNormals(Mesh);
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

} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
