#include "McpAutomationBridge_GeometryHandlers.h"

#if WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleExtrudeAlongSpline(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    FString SplineActorName = GetStringFieldGeom(Payload, TEXT("splineActorName"));
    int32 Segments = GetIntFieldGeom(Payload, TEXT("segments"), 16);
    bool bCap = GetBoolFieldGeom(Payload, TEXT("cap"), true);
    double ScaleStart = GetNumberFieldGeom(Payload, TEXT("scaleStart"), 1.0);
    double ScaleEnd = GetNumberFieldGeom(Payload, TEXT("scaleEnd"), 1.0);
    double Twist = GetNumberFieldGeom(Payload, TEXT("twist"), 0.0);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (SplineActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("splineActorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;
    AActor* SplineActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == SplineActorName)
        {
            SplineActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    if (!SplineActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Spline actor not found: %s"), *SplineActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    USplineComponent* SplineComp = SplineActor->FindComponentByClass<USplineComponent>();
    if (!SplineComp)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("Spline actor has no USplineComponent"), TEXT("COMPONENT_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();
    int32 TrisBefore = Mesh->GetTriangleCount();

    // Get mesh bounding box to derive profile shape
    FBox MeshBBox = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(Mesh);
    FVector MeshCenter = MeshBBox.GetCenter();
    FVector MeshExtent = MeshBBox.GetExtent();

    // Create a cross-section profile from mesh XY bounds
    TArray<FVector2D> PolygonVertices;
    int32 NumPolySides = FMath::Clamp(Segments / 2, 4, 32);
    double ProfileRadius = FMath::Max(MeshExtent.X, MeshExtent.Y);

    if (ProfileRadius < KINDA_SMALL_NUMBER)
    {
        ProfileRadius = 50.0; // Default fallback
    }

    for (int32 i = 0; i < NumPolySides; ++i)
    {
        double Angle = 2.0 * PI * i / NumPolySides;
        PolygonVertices.Add(FVector2D(
            FMath::Cos(Angle) * ProfileRadius,
            FMath::Sin(Angle) * ProfileRadius
        ));
    }

    // Build path frames from spline
    TArray<FTransform> PathFrames;
    float SplineLength = SplineComp->GetSplineLength();
    int32 PathSteps = FMath::Clamp(Segments, 2, 256);

    for (int32 i = 0; i <= PathSteps; ++i)
    {
        float Alpha = (float)i / PathSteps;
        float Dist = SplineLength * Alpha;

        // Get spline location and rotation at this distance
        FVector Location = SplineComp->GetLocationAtDistanceAlongSpline(Dist, ESplineCoordinateSpace::World);
        FQuat Rotation = SplineComp->GetQuaternionAtDistanceAlongSpline(Dist, ESplineCoordinateSpace::World);

        // Apply twist interpolation
        float TwistAngle = FMath::DegreesToRadians(Twist * Alpha);
        FQuat TwistRotation = FQuat(FVector::ForwardVector, TwistAngle);
        Rotation = Rotation * TwistRotation;

        // Apply scale interpolation
        float Scale = FMath::Lerp((float)ScaleStart, (float)ScaleEnd, Alpha);

        PathFrames.Add(FTransform(Rotation, Location, FVector(Scale)));
    }

    // Use SweepPolygon to create the extruded mesh
    // UE 5.7: FGeometryScriptPolygonsToSweepOptions removed, use direct parameters
    FGeometryScriptPrimitiveOptions PrimOptions;

    // Clear existing mesh and sweep the profile along the path
    // UE 5.5+: AppendSweepPolygon has MiterLimit parameter
    // UE 5.4 and earlier: No MiterLimit parameter
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSweepPolygon(
        Mesh,
        PrimOptions,
        FTransform::Identity,
        PolygonVertices,
        PathFrames,
        true,  // bLoop
        bCap,  // bCapped
        1.0f,  // StartScale
        1.0f,  // EndScale
        0.0f,  // RotationAngleDeg
        1.0f,  // MiterLimit
        nullptr
    );
#else
    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSweepPolygon(
        Mesh,
        PrimOptions,
        FTransform::Identity,
        PolygonVertices,
        PathFrames,
        true,  // bLoop
        bCap,  // bCapped
        1.0f,  // StartScale
        1.0f,  // EndScale
        0.0f,  // RotationAngleDeg
        nullptr
    );
#endif

    DMC->NotifyMeshUpdated();

    int32 TrisAfter = Mesh->GetTriangleCount();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("splineActorName"), SplineActorName);
    Result->SetNumberField(TEXT("splineLength"), SplineLength);
    Result->SetNumberField(TEXT("segments"), Segments);
    Result->SetNumberField(TEXT("trianglesBefore"), TrisBefore);
    Result->SetNumberField(TEXT("trianglesAfter"), TrisAfter);

    // Add verification data for the target actor
    if (TargetActor)
    {
        McpHandlerUtils::AddVerification(Result, TargetActor);
    }

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Extruded profile along spline"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Edge Split Operations
// -------------------------------------------------------------------------
} // namespace McpGeometryHandlers

#endif // WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT
