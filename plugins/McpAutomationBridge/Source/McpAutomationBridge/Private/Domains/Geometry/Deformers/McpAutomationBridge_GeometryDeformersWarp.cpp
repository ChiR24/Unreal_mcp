#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
namespace
{
// deform_mesh declares strength (the angle, flare percent or noise magnitude) and axis.

// The warps act along the frame's Z; axis turns that frame onto X, Y or Z.
FTransform WarpFrame(const TSharedPtr<FJsonObject>& Payload)
{
    return FTransform(FQuat::FindBetweenNormals(FVector::UpVector, AxisVectorFromPayload(Payload)));
}

// Defaults sized to the mesh: a fixed extent of 50 or a noise wavelength of
// 4 units only suited a 100-unit mesh.
FVector MeshSize(UDynamicMesh* Mesh)
{
    const FBox Bounds = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(Mesh);
    return Bounds.IsValid ? Bounds.GetSize() : FVector(100.0);
}

double HalfExtentAlongAxis(UDynamicMesh* Mesh, const TSharedPtr<FJsonObject>& Payload)
{
    return 0.5 * MeshSize(Mesh)[AxisIndexFromPayload(Payload)];
}
}

bool HandleBend(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                       const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    double BendAngle = GetJsonNumberField(Payload, TEXT("strength"), 45.0);

    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;
    auto [TargetActor, DMC, Mesh] = *Target;
    const double BendExtent = HalfExtentAlongAxis(Mesh, Payload);

    FGeometryScriptBendWarpOptions BendOptions;
    BendOptions.bSymmetricExtents = true;
    BendOptions.bBidirectional = true;

    UGeometryScriptLibrary_MeshDeformFunctions::ApplyBendWarpToMesh(
        Mesh, BendOptions, WarpFrame(Payload), BendAngle, BendExtent, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("angle"), BendAngle);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Bend deformer applied"), Result);
    return true;
}

bool HandleTwist(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                        const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    double TwistAngle = GetJsonNumberField(Payload, TEXT("strength"), 45.0);

    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;
    auto [TargetActor, DMC, Mesh] = *Target;
    const double TwistExtent = HalfExtentAlongAxis(Mesh, Payload);

    FGeometryScriptTwistWarpOptions TwistOptions;
    TwistOptions.bSymmetricExtents = true;
    TwistOptions.bBidirectional = true;

    UGeometryScriptLibrary_MeshDeformFunctions::ApplyTwistWarpToMesh(
        Mesh, TwistOptions, WarpFrame(Payload), TwistAngle, TwistExtent, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("angle"), TwistAngle);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Twist deformer applied"), Result);
    return true;
}

bool HandleTaper(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                        const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    // strength is the flare percent on both cross axes.
    const double FlarePercent = GetJsonNumberField(Payload, TEXT("strength"), 50.0);

    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;
    auto [TargetActor, DMC, Mesh] = *Target;
    const double FlareExtent = HalfExtentAlongAxis(Mesh, Payload);

    FGeometryScriptFlareWarpOptions FlareOptions;
    FlareOptions.bSymmetricExtents = true;

    UGeometryScriptLibrary_MeshDeformFunctions::ApplyFlareWarpToMesh(
        Mesh, FlareOptions, WarpFrame(Payload), FlarePercent, FlarePercent, FlareExtent, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Taper/flare deformer applied"), Result);
    return true;
}

bool HandleNoiseDeform(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                              const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    double Magnitude = GetJsonNumberField(Payload, TEXT("strength"), 5.0);

    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;
    auto [TargetActor, DMC, Mesh] = *Target;
    // Three noise bumps across the mesh's largest dimension by default.
    double Frequency = GetJsonNumberField(Payload, TEXT("frequency"),
        3.0 / FMath::Max(1.0, MeshSize(Mesh).GetMax()));

    FGeometryScriptPerlinNoiseOptions NoiseOptions;
    NoiseOptions.BaseLayer.Magnitude = Magnitude;
    NoiseOptions.BaseLayer.Frequency = Frequency;
    NoiseOptions.bApplyAlongNormal = true;

    FGeometryScriptMeshSelection Selection;

#if ENGINE_MINOR_VERSION >= 7
    // UE 5.7+: Use ApplyPerlinNoiseToMesh2 (updated API)
    UGeometryScriptLibrary_MeshDeformFunctions::ApplyPerlinNoiseToMesh2(
        Mesh, Selection, NoiseOptions, nullptr);
#else
    // UE 5.0-5.6: Use original ApplyPerlinNoiseToMesh
    UGeometryScriptLibrary_MeshDeformFunctions::ApplyPerlinNoiseToMesh(
        Mesh, Selection, NoiseOptions, nullptr);
#endif

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("magnitude"), Magnitude);
    Result->SetNumberField(TEXT("frequency"), Frequency);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Noise deformer applied"), Result);
    return true;
}
} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
