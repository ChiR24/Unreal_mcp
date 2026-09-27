#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleBridge(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                         const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));

    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;
    auto [TargetActor, DMC, Mesh] = *Target;
    int32 TrisBefore = Mesh->GetTriangleCount();

    int32 TrianglesCreated = 0;
    FString BridgeStatus;
    bool bFillHolesInstead = false;

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
        bFillHolesInstead = true;
    }
    else
    {
        // The first two boundary loops.
        const int32 LoopIndexA = 0;
        const int32 LoopIndexB = 1;

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
                if (vA0 != vA1 && vA1 != vB0 && vB0 != vA0)
                {
                    int32 Result = EditMesh.AppendTriangle(vA0, vA1, vB0);
                    if (Result >= 0) TrianglesCreated++;
                }

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
    BridgeStatus = TEXT("Bridging requires UE 5.5+ (FMeshBoundaryLoops). Filling holes instead.");
    bFillHolesInstead = true;
#endif
    if (bFillHolesInstead)
    {
        FGeometryScriptFillHolesOptions FillOptions;
        FillOptions.FillMethod = EGeometryScriptFillHolesMethod::MinimalFill;
        int32 NumFilledHoles = 0;
        int32 NumFailedHoleFills = 0;
        UGeometryScriptLibrary_MeshRepairFunctions::FillAllMeshHoles(Mesh, FillOptions, NumFilledHoles, NumFailedHoleFills, nullptr);
    }

    int32 TrisAfter = Mesh->GetTriangleCount();
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("bridgeStatus"), BridgeStatus);
    Result->SetNumberField(TEXT("trianglesCreated"), TrianglesCreated);
    Result->SetNumberField(TEXT("trianglesBefore"), TrisBefore);
    Result->SetNumberField(TEXT("trianglesAfter"), TrisAfter);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Bridge applied"), Result);
    return true;
}

bool HandleDuplicateAlongSpline(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                       const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    const FString SplineActorName = GetJsonStringField(Payload, TEXT("splineActorName"));
    const int32 Count = GetJsonIntField(Payload, TEXT("count"), 10);

    ADynamicMeshActor* SourceActor = FindGeometryActor<ADynamicMeshActor>(ActorName);
    if (!SourceActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Source actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }
    USplineComponent* Spline = ResolveGeometrySpline(Self, RequestId, Socket, SplineActorName);
    if (!Spline) return true;
    UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (!ActorSS)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("EditorActorSubsystem unavailable"), TEXT("EDITOR_SUBSYSTEM_MISSING"));
        return true;
    }

    const float SplineLength = Spline->GetSplineLength();
    TArray<TSharedPtr<FJsonValue>> CreatedActors;
    for (int32 i = 0; i < Count; ++i)
    {
        const float Distance = SplineLength * (static_cast<float>(i) / FMath::Max(Count - 1, 1));
        AActor* NewActor = ActorSS->DuplicateActor(SourceActor, SourceActor->GetWorld());
        if (!NewActor) continue;
        NewActor->SetActorLocationAndRotation(
            Spline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World),
            Spline->GetRotationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World));
        NewActor->SetActorLabel(FString::Printf(TEXT("%s_Dup%d"), *ActorName, i));
        CreatedActors.Add(MakeShared<FJsonValueString>(NewActor->GetActorLabel()));
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("sourceActor"), ActorName);
    Result->SetStringField(TEXT("splineActor"), SplineActorName);
    Result->SetNumberField(TEXT("count"), Count);
    Result->SetNumberField(TEXT("splineLength"), SplineLength);
    Result->SetArrayField(TEXT("createdActors"), CreatedActors);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Duplicates created along spline"), Result);
    return true;
}

} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
