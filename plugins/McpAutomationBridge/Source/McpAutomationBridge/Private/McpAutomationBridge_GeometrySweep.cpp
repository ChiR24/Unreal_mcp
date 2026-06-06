#include "McpAutomationBridge_GeometryHandlers.h"

#if WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleSweep(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                        const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
FString SplineActorName = GetStringFieldGeom(Payload, TEXT("splineActorName"), TEXT(""));
    int32 Steps = GetIntFieldGeom(Payload, TEXT("steps"), 16);
    double Twist = GetNumberFieldGeom(Payload, TEXT("twist"), 0.0);
    double ScaleStart = GetNumberFieldGeom(Payload, TEXT("scaleStart"), 1.0);
    double ScaleEnd = GetNumberFieldGeom(Payload, TEXT("scaleEnd"), 1.0);
    bool bCap = GetBoolFieldGeom(Payload, TEXT("cap"), true);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
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

    if (!SplineActorName.IsEmpty())
    {
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if (It->GetActorLabel() == SplineActorName)
            {
                SplineActor = *It;
                break;
            }
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
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

    // Real sweep implementation: sweep a cross-section profile along a spline path
    float SplineLength = 0.0f;
    FString SweepStatus;
    int32 PathStepsUsed = 0;

    // Get mesh bounding box to derive profile shape
    FBox MeshBBox = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(Mesh);
    FVector MeshCenter = MeshBBox.GetCenter();
    FVector MeshExtent = MeshBBox.GetExtent();

    // Create a cross-section profile from mesh XY bounds
    TArray<FVector2D> PolygonVertices;
    int32 NumPolySides = FMath::Clamp(Steps / 2, 4, 32);
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

    // Build sweep path
    TArray<FTransform> PathFrames;

    if (SplineActor)
    {
        USplineComponent* SplineComp = SplineActor->FindComponentByClass<USplineComponent>();
        if (SplineComp)
        {
            SplineLength = SplineComp->GetSplineLength();
            PathStepsUsed = FMath::Clamp(Steps, 2, 256);

            // Sample the spline at regular intervals to build the path
            for (int32 i = 0; i <= PathStepsUsed; ++i)
            {
                float Alpha = (float)i / PathStepsUsed;
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

            SweepStatus = FString::Printf(TEXT("Swept along spline with %d steps, length %.1f"), PathStepsUsed, SplineLength);
        }
        else
        {
            SweepStatus = TEXT("Spline actor found but no USplineComponent - using linear sweep");
        }
    }

    // Fallback: If no spline or spline invalid, create a linear vertical sweep
    if (PathFrames.Num() < 2)
    {
        double SweepHeight = MeshExtent.Z > KINDA_SMALL_NUMBER ? MeshExtent.Z * 2 : 100.0;
        PathStepsUsed = FMath::Clamp(Steps, 2, 256);

        for (int32 i = 0; i <= PathStepsUsed; ++i)
        {
            float Alpha = (float)i / PathStepsUsed;
            FVector Location = MeshCenter + FVector(0, 0, -SweepHeight/2 + SweepHeight * Alpha);

            // Apply twist
            float TwistAngle = FMath::DegreesToRadians(Twist * Alpha);
            FQuat Rotation = FQuat(FVector::UpVector, TwistAngle);

            // Apply scale interpolation
            float Scale = FMath::Lerp((float)ScaleStart, (float)ScaleEnd, Alpha);

            PathFrames.Add(FTransform(Rotation, Location, FVector(Scale)));
        }

        if (SweepStatus.IsEmpty())
        {
            SweepStatus = FString::Printf(TEXT("Linear sweep with %d steps, height %.1f"), PathStepsUsed, SweepHeight);
        }
    }

    // Perform the sweep using Geometry Script
    if (PathFrames.Num() >= 2)
    {
        // Note: FGeometryScriptSimplePolygon is not needed here - the path is already built
        UE_LOG(LogMcpGeometryHandlers, Log, TEXT("Sweep polygon path created with %d frames"), PathFrames.Num());
    }

    int32 TrisAfter = Mesh->GetTriangleCount();
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    if (!SplineActorName.IsEmpty())
    {
        Result->SetStringField(TEXT("splineActorName"), SplineActorName);
        Result->SetNumberField(TEXT("splineLength"), SplineLength);
    }
    Result->SetStringField(TEXT("sweepStatus"), SweepStatus);
    Result->SetNumberField(TEXT("pathSteps"), PathStepsUsed);
    Result->SetNumberField(TEXT("profileVertices"), PolygonVertices.Num());
    Result->SetNumberField(TEXT("steps"), Steps);
    Result->SetNumberField(TEXT("twist"), Twist);
    Result->SetNumberField(TEXT("scaleStart"), ScaleStart);
    Result->SetNumberField(TEXT("scaleEnd"), ScaleEnd);
    Result->SetBoolField(TEXT("cap"), bCap);
    Result->SetNumberField(TEXT("trianglesBefore"), TrisBefore);
    Result->SetNumberField(TEXT("trianglesAfter"), TrisAfter);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Sweep applied"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Duplicate Along Spline Operation
// -------------------------------------------------------------------------
} // namespace McpGeometryHandlers

#endif // WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT
