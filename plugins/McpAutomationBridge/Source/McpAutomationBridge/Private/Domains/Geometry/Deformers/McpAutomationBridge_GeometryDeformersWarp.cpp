#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
namespace
{
// deform_mesh declares strength and axis. These handlers used to read only
// angle/extent/flareX/magnitude/frequency -- names the gateway rejects as
// undeclared -- so every bend, twist, taper and noise ran at its hardcoded
// default whatever the caller asked for. The declared name wins; the old
// names stay as fallbacks.
double DeclaredOr(const TSharedPtr<FJsonObject>& Payload, const TCHAR* Declared,
                  const TCHAR* Legacy, double Default)
{
    return Payload->HasField(Declared) ? GetJsonNumberField(Payload, Declared, Default)
                                       : GetJsonNumberField(Payload, Legacy, Default);
}

// The warps act along the frame's Z; axis turns that frame onto X, Y or Z.
FTransform WarpFrame(const TSharedPtr<FJsonObject>& Payload)
{
    const FString Axis = GetJsonStringField(Payload, TEXT("axis")).ToUpper();
    const FVector Dir = Axis == TEXT("X") ? FVector::ForwardVector
                      : Axis == TEXT("Y") ? FVector::RightVector : FVector::UpVector;
    return FTransform(FQuat::FindBetweenNormals(FVector::UpVector, Dir));
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
    const FVector Size = MeshSize(Mesh);
    const FString Axis = GetJsonStringField(Payload, TEXT("axis")).ToUpper();
    return 0.5 * (Axis == TEXT("X") ? Size.X : Axis == TEXT("Y") ? Size.Y : Size.Z);
}
}

bool HandleBend(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                       const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    double BendAngle = DeclaredOr(Payload, TEXT("strength"), TEXT("angle"), 45.0);

    ADynamicMeshActor* TargetActor = nullptr;
    UDynamicMeshComponent* DMC = nullptr;
    UDynamicMesh* Mesh = nullptr;
    if (!ResolveDynamicMeshForGeometry(Self, RequestId, ActorName, Socket, TargetActor, DMC, Mesh))
    {
        return true;
    }
    double BendExtent = GetJsonNumberField(Payload, TEXT("extent"), HalfExtentAlongAxis(Mesh, Payload));

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
    double TwistAngle = DeclaredOr(Payload, TEXT("strength"), TEXT("angle"), 45.0);

    ADynamicMeshActor* TargetActor = nullptr;
    UDynamicMeshComponent* DMC = nullptr;
    UDynamicMesh* Mesh = nullptr;
    if (!ResolveDynamicMeshForGeometry(Self, RequestId, ActorName, Socket, TargetActor, DMC, Mesh))
    {
        return true;
    }
    double TwistExtent = GetJsonNumberField(Payload, TEXT("extent"), HalfExtentAlongAxis(Mesh, Payload));

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
    double FlarePercentX = DeclaredOr(Payload, TEXT("strength"), TEXT("flareX"), 50.0);
    double FlarePercentY = DeclaredOr(Payload, TEXT("strength"), TEXT("flareY"), 50.0);

    ADynamicMeshActor* TargetActor = nullptr;
    UDynamicMeshComponent* DMC = nullptr;
    UDynamicMesh* Mesh = nullptr;
    if (!ResolveDynamicMeshForGeometry(Self, RequestId, ActorName, Socket, TargetActor, DMC, Mesh))
    {
        return true;
    }
    double FlareExtent = GetJsonNumberField(Payload, TEXT("extent"), HalfExtentAlongAxis(Mesh, Payload));

    FGeometryScriptFlareWarpOptions FlareOptions;
    FlareOptions.bSymmetricExtents = true;

    UGeometryScriptLibrary_MeshDeformFunctions::ApplyFlareWarpToMesh(
        Mesh, FlareOptions, WarpFrame(Payload), FlarePercentX, FlarePercentY, FlareExtent, nullptr);

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
    double Magnitude = DeclaredOr(Payload, TEXT("strength"), TEXT("magnitude"), 5.0);

    ADynamicMeshActor* TargetActor = nullptr;
    UDynamicMeshComponent* DMC = nullptr;
    UDynamicMesh* Mesh = nullptr;
    if (!ResolveDynamicMeshForGeometry(Self, RequestId, ActorName, Socket, TargetActor, DMC, Mesh))
    {
        return true;
    }
    // Three noise bumps across the mesh's largest dimension by default.
    double Frequency = GetJsonNumberField(Payload, TEXT("frequency"),
        3.0 / FMath::Max(1.0, MeshSize(Mesh).GetMax()));

    FGeometryScriptPerlinNoiseOptions NoiseOptions;
    NoiseOptions.BaseLayer.Magnitude = Magnitude;
    NoiseOptions.BaseLayer.Frequency = Frequency;
    NoiseOptions.bApplyAlongNormal = true;

    FGeometryScriptMeshSelection Selection;

#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 7
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

#endif // WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT
