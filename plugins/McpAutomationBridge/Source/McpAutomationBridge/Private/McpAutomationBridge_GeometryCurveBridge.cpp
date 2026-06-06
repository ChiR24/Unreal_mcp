#include "McpAutomationBridge_GeometryHandlers.h"

#if WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleBridge(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                         const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
int32 EdgeGroupA = GetIntFieldGeom(Payload, TEXT("edgeGroupA"), 0);
    int32 EdgeGroupB = GetIntFieldGeom(Payload, TEXT("edgeGroupB"), 1);
    int32 Subdivisions = GetIntFieldGeom(Payload, TEXT("subdivisions"), 1);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
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

    int32 TrianglesCreated = 0;
    FString BridgeStatus;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
    // Get direct access to FDynamicMesh3 for low-level operations
    UE::Geometry::FDynamicMesh3& EditMesh = Mesh->GetMeshRef();

    // Find boundary loops using GeometryCore's FMeshBoundaryLoops (UE 5.5+)
    UE::Geometry::FMeshBoundaryLoops BoundaryLoops(&EditMesh, true);

    if (BoundaryLoops.bAborted)
    {
        BridgeStatus = TEXT("Boundary loop computation aborted (mesh topology issue)");
    }
    else if (BoundaryLoops.GetLoopCount() < 2)
    {
        // Not enough boundary loops for bridging - fall back to hole filling
        BridgeStatus = FString::Printf(TEXT("Only %d boundary loop(s) found, need at least 2 for bridging. Filling holes instead."), BoundaryLoops.GetLoopCount());

        FGeometryScriptFillHolesOptions FillOptions;
        FillOptions.FillMethod = EGeometryScriptFillHolesMethod::MinimalFill;
        int32 NumFilledHoles = 0;
        int32 NumFailedHoleFills = 0;
        UGeometryScriptLibrary_MeshRepairFunctions::FillAllMeshHoles(Mesh, FillOptions, NumFilledHoles, NumFailedHoleFills, nullptr);
    }
    else
    {
        // Validate edge group indices
        int32 LoopCount = BoundaryLoops.GetLoopCount();
        int32 LoopIndexA = FMath::Clamp(EdgeGroupA, 0, LoopCount - 1);
        int32 LoopIndexB = FMath::Clamp(EdgeGroupB, 0, LoopCount - 1);

        if (LoopIndexA == LoopIndexB)
        {
            // Adjust to pick different loops if same index was provided
            LoopIndexB = (LoopIndexA + 1) % LoopCount;
        }

        const UE::Geometry::FEdgeLoop& LoopA = BoundaryLoops[LoopIndexA];
        const UE::Geometry::FEdgeLoop& LoopB = BoundaryLoops[LoopIndexB];

        const TArray<int32>& VertsA = LoopA.Vertices;
        const TArray<int32>& VertsB = LoopB.Vertices;

        int32 NumVertsA = VertsA.Num();
        int32 NumVertsB = VertsB.Num();

        if (NumVertsA > 0 && NumVertsB > 0)
        {
            // Find the closest starting vertex on LoopB to LoopA's first vertex
            FVector3d StartPosA = EditMesh.GetVertex(VertsA[0]);
            int32 BestStartB = 0;
            double BestDist = TNumericLimits<double>::Max();

            for (int32 i = 0; i < NumVertsB; ++i)
            {
                double Dist = FVector3d::DistSquared(StartPosA, EditMesh.GetVertex(VertsB[i]));
                if (Dist < BestDist)
                {
                    BestDist = Dist;
                    BestStartB = i;
                }
            }

            // Create triangle strips between the two loops
            // Handle loops of different sizes by using modular indexing
            int32 MaxVerts = FMath::Max(NumVertsA, NumVertsB);

            for (int32 i = 0; i < MaxVerts; ++i)
            {
                // Map indices to actual loop vertices with modular wrap
                int32 iA = i % NumVertsA;
                int32 iA_Next = (i + 1) % NumVertsA;
                int32 iB = (BestStartB + i) % NumVertsB;
                int32 iB_Next = (BestStartB + i + 1) % NumVertsB;

                int32 vA0 = VertsA[iA];
                int32 vA1 = VertsA[iA_Next];
                int32 vB0 = VertsB[iB];
                int32 vB1 = VertsB[iB_Next];

                // Create two triangles forming a quad between the loops
                // Triangle 1: vA0 -> vA1 -> vB0
                if (vA0 != vA1 && vA1 != vB0 && vB0 != vA0)
                {
                    int32 Result = EditMesh.AppendTriangle(vA0, vA1, vB0);
                    if (Result >= 0) TrianglesCreated++;
                }

                // Triangle 2: vB0 -> vA1 -> vB1
                if (vB0 != vA1 && vA1 != vB1 && vB1 != vB0)
                {
                    int32 Result = EditMesh.AppendTriangle(vB0, vA1, vB1);
                    if (Result >= 0) TrianglesCreated++;
                }
            }

            BridgeStatus = FString::Printf(TEXT("Bridged loop %d (%d verts) to loop %d (%d verts), created %d triangles"),
                LoopIndexA, NumVertsA, LoopIndexB, NumVertsB, TrianglesCreated);
        }
        else
        {
            BridgeStatus = TEXT("One or both boundary loops have no vertices");
        }
    }
#else
    // UE 5.3 fallback: Use hole filling instead of bridging
    BridgeStatus = TEXT("Bridging requires UE 5.4+ (FMeshBoundaryLoops). Using hole filling instead.");

    FGeometryScriptFillHolesOptions FillOptions;
    FillOptions.FillMethod = EGeometryScriptFillHolesMethod::MinimalFill;
    int32 NumFilledHoles = 0;
    int32 NumFailedHoleFills = 0;
    UGeometryScriptLibrary_MeshRepairFunctions::FillAllMeshHoles(Mesh, FillOptions, NumFilledHoles, NumFailedHoleFills, nullptr);
    TrianglesCreated = NumFilledHoles; // Approximate
#endif

    int32 TrisAfter = Mesh->GetTriangleCount();
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("edgeGroupA"), EdgeGroupA);
    Result->SetNumberField(TEXT("edgeGroupB"), EdgeGroupB);
    Result->SetNumberField(TEXT("subdivisions"), Subdivisions);
    Result->SetStringField(TEXT("bridgeStatus"), BridgeStatus);
    Result->SetNumberField(TEXT("trianglesCreated"), TrianglesCreated);
    Result->SetNumberField(TEXT("trianglesBefore"), TrisBefore);
    Result->SetNumberField(TEXT("trianglesAfter"), TrisAfter);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Bridge applied"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Loft Operation
// -------------------------------------------------------------------------

bool HandleDuplicateAlongSpline(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                       const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    FString SplineActorName = GetStringFieldGeom(Payload, TEXT("splineActorName"));
int32 Count = GetIntFieldGeom(Payload, TEXT("count"), 10);
    bool bAlignToSpline = GetBoolFieldGeom(Payload, TEXT("alignToSpline"), true);
    double ScaleVariation = GetNumberFieldGeom(Payload, TEXT("scaleVariation"), 0.0);

    if (ActorName.IsEmpty() || SplineActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName and splineActorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* SourceActor = nullptr;
    AActor* SplineActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            SourceActor = *It;
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

    if (!SourceActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Source actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    if (!SplineActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Spline actor not found: %s"), *SplineActorName), TEXT("SPLINE_NOT_FOUND"));
        return true;
    }

    USplineComponent* SplineComp = SplineActor->FindComponentByClass<USplineComponent>();
    if (!SplineComp)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("Actor does not have a spline component"), TEXT("SPLINE_COMPONENT_NOT_FOUND"));
        return true;
    }

    // Create duplicates along spline
    float SplineLength = SplineComp->GetSplineLength();
    TArray<FString> CreatedActors;

    UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (!ActorSS)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("EditorActorSubsystem unavailable"), TEXT("EDITOR_SUBSYSTEM_MISSING"));
        return true;
    }

    for (int32 i = 0; i < Count; ++i)
    {
        float Distance = SplineLength * ((float)i / FMath::Max(Count - 1, 1));
        FVector Location = SplineComp->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
        FRotator Rotation = bAlignToSpline ? SplineComp->GetRotationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World) : FRotator::ZeroRotator;

        // Duplicate the source actor at this location
        AActor* NewActor = ActorSS->DuplicateActor(SourceActor, World);
        if (NewActor)
        {
            NewActor->SetActorLocation(Location);
            NewActor->SetActorRotation(Rotation);

            // Apply scale variation if requested
            if (ScaleVariation > 0.0)
            {
                double ScaleFactor = 1.0 + FMath::RandRange(-ScaleVariation, ScaleVariation);
                NewActor->SetActorScale3D(FVector(ScaleFactor));
            }

            FString NewName = FString::Printf(TEXT("%s_Dup%d"), *ActorName, i);
            NewActor->SetActorLabel(NewName);
            CreatedActors.Add(NewName);
        }
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("sourceActor"), ActorName);
    Result->SetStringField(TEXT("splineActor"), SplineActorName);
    Result->SetNumberField(TEXT("count"), Count);
    Result->SetNumberField(TEXT("splineLength"), SplineLength);
    Result->SetBoolField(TEXT("alignToSpline"), bAlignToSpline);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Duplicates created along spline"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Loop Cut Operation
// -------------------------------------------------------------------------
} // namespace McpGeometryHandlers

#endif // WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT
