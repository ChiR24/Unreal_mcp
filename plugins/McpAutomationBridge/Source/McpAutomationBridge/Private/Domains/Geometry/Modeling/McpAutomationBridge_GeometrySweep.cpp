#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleSweep(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                        const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    const FString SplineActorName = GetJsonStringField(Payload, TEXT("splineActorName"));
    const int32 Steps = GetJsonIntField(Payload, TEXT("steps"), 16);
    const bool bCap = GetJsonBoolField(Payload, TEXT("cap"), true);

    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;
    auto [TargetActor, DMC, Mesh] = *Target;

    // A named spline must resolve: silently sweeping a straight line instead hid typos.
    USplineComponent* Spline = nullptr;
    if (!SplineActorName.IsEmpty())
    {
        Spline = ResolveGeometrySpline(Self, RequestId, Socket, SplineActorName);
        if (!Spline) return true;
    }
    const int32 TrisBefore = Mesh->GetTriangleCount();

    // Circular profile sized to the mesh footprint.
    const FBox MeshBBox = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(Mesh);
    const FVector MeshExtent = MeshBBox.GetExtent();
    const int32 NumPolySides = FMath::Clamp(Steps / 2, 4, 32);
    double ProfileRadius = FMath::Max(MeshExtent.X, MeshExtent.Y);
    if (ProfileRadius < KINDA_SMALL_NUMBER) ProfileRadius = 50.0;
    TArray<FVector2D> PolygonVertices;
    for (int32 i = 0; i < NumPolySides; ++i)
    {
        const double Angle = 2.0 * PI * i / NumPolySides;
        PolygonVertices.Add(FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * ProfileRadius);
    }

    // Path: along the spline, else a vertical line through the mesh.
    const int32 PathSteps = FMath::Clamp(Steps, 2, 256);
    const double SplineLength = Spline ? Spline->GetSplineLength() : 0.0;
    const double SweepHeight = MeshExtent.Z > KINDA_SMALL_NUMBER ? MeshExtent.Z * 2 : 100.0;
    TArray<FTransform> PathFrames;
    for (int32 i = 0; i <= PathSteps; ++i)
    {
        const double Alpha = static_cast<double>(i) / PathSteps;
        PathFrames.Add(Spline
            ? FTransform(Spline->GetQuaternionAtDistanceAlongSpline(SplineLength * Alpha, ESplineCoordinateSpace::World),
                         Spline->GetLocationAtDistanceAlongSpline(SplineLength * Alpha, ESplineCoordinateSpace::World))
            : FTransform(MeshBBox.GetCenter() + FVector(0, 0, SweepHeight * (Alpha - 0.5))));
    }

    FGeometryScriptPrimitiveOptions PrimOptions;
    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSweepPolygon(
        Mesh, PrimOptions, FTransform::Identity, PolygonVertices, PathFrames,
        true, bCap, 1.0f, 1.0f, 0.0f,
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
        1.0f, // MiterLimit, added in 5.5
#endif
        nullptr);
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    if (Spline)
    {
        Result->SetStringField(TEXT("splineActorName"), SplineActorName);
        Result->SetNumberField(TEXT("splineLength"), SplineLength);
    }
    Result->SetNumberField(TEXT("pathSteps"), PathSteps);
    Result->SetNumberField(TEXT("profileVertices"), PolygonVertices.Num());
    Result->SetBoolField(TEXT("cap"), bCap);
    Result->SetNumberField(TEXT("trianglesBefore"), TrisBefore);
    Result->SetNumberField(TEXT("trianglesAfter"), Mesh->GetTriangleCount());
    McpHandlerUtils::AddVerification(Result, TargetActor);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Sweep applied"), Result);
    return true;
}

bool HandleSegmentedSweep(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    // loft and extrude_along_spline are sweeps that name their path step count segments.
    const TSharedPtr<FJsonObject> SweepPayload = MakeShared<FJsonObject>(*Payload);
    if (Payload->HasField(TEXT("segments")) && !Payload->HasField(TEXT("steps")))
    {
        SweepPayload->SetNumberField(TEXT("steps"), GetJsonIntField(Payload, TEXT("segments"), 16));
    }
    return HandleSweep(Self, RequestId, SweepPayload, Socket);
}

} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
