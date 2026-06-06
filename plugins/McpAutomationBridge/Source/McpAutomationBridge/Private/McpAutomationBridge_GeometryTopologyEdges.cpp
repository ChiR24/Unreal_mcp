#include "McpAutomationBridge_GeometryHandlers.h"

#if WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleLoopCut(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    int32 NumCuts = GetIntFieldGeom(Payload, TEXT("numCuts"), 1);
    double Offset = GetNumberFieldGeom(Payload, TEXT("offset"), 0.5);

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

    // Get optional axis parameter (default to Z for horizontal cuts)
    FString Axis = GetStringFieldGeom(Payload, TEXT("axis"), TEXT("Z")).ToUpper();

    // Real loop cut implementation using plane cutting
    // Unlike PN tessellation which subdivides ALL faces uniformly,
    // plane cutting inserts edges ONLY where the plane intersects the mesh

    // Get mesh bounds from the underlying FDynamicMesh3
    FDynamicMesh3& EditMesh = Mesh->GetMeshRef();
    UE::Geometry::FAxisAlignedBox3d Bounds = EditMesh.GetBounds();

    // Determine slice axis and bounds
    double MinExtent, MaxExtent;
    FVector PlaneNormal;
    FVector BoundsCenter(Bounds.Center().X, Bounds.Center().Y, Bounds.Center().Z);

    if (Axis == TEXT("X"))
    {
        MinExtent = Bounds.Min.X;
        MaxExtent = Bounds.Max.X;
        PlaneNormal = FVector(1.0, 0.0, 0.0);
    }
    else if (Axis == TEXT("Y"))
    {
        MinExtent = Bounds.Min.Y;
        MaxExtent = Bounds.Max.Y;
        PlaneNormal = FVector(0.0, 1.0, 0.0);
    }
    else // Default to Z
    {
        MinExtent = Bounds.Min.Z;
        MaxExtent = Bounds.Max.Z;
        PlaneNormal = FVector(0.0, 0.0, 1.0);
    }

    // Configure plane cut options - don't fill holes, just insert edge loops
    FGeometryScriptMeshPlaneCutOptions CutOptions;
    CutOptions.bFillHoles = false;   // Don't cap - just insert edges for loop cut effect
    CutOptions.bFillSpans = false;   // Don't fill boundary spans
    CutOptions.bFlipCutSide = false; // Keep both sides of the mesh

    int32 CutsApplied = 0;

    // Apply multiple cuts distributed across the mesh
    // Offset (0.0-1.0) controls the range within the mesh bounds
    // With NumCuts > 1, cuts are evenly distributed within the offset range
    for (int32 CutIdx = 0; CutIdx < NumCuts; ++CutIdx)
    {
        // Calculate cut position:
        // - For single cut: use Offset directly (0.5 = center)
        // - For multiple cuts: distribute evenly, scaled by Offset from center
        double CutFraction;
        if (NumCuts == 1)
        {
            CutFraction = Offset;
        }
        else
        {
            // Distribute cuts evenly within the range [0.5 - Offset/2, 0.5 + Offset/2]
            double RangeStart = 0.5 - (Offset * 0.5);
            double RangeEnd = 0.5 + (Offset * 0.5);
            CutFraction = FMath::Lerp(RangeStart, RangeEnd, (double)(CutIdx + 1) / (double)(NumCuts + 1));
        }

        double PlanePosition = FMath::Lerp(MinExtent, MaxExtent, CutFraction);

        // Build the cut plane transform
        FVector PlaneLocation = BoundsCenter;
        if (Axis == TEXT("X"))
            PlaneLocation.X = PlanePosition;
        else if (Axis == TEXT("Y"))
            PlaneLocation.Y = PlanePosition;
        else
            PlaneLocation.Z = PlanePosition;

        // Create transform with the plane facing along the cut axis
        FTransform PlaneTransform;
        PlaneTransform.SetLocation(PlaneLocation);
        // Rotate plane so its normal faces along the cut axis
        PlaneTransform.SetRotation(FQuat::FindBetweenNormals(FVector::UpVector, PlaneNormal));

        // Apply the plane cut
        UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshPlaneCut(
            Mesh,
            PlaneTransform,
            CutOptions,
            nullptr // UGeometryScriptDebug*
        );

        ++CutsApplied;
    }

    int32 TrisAfter = Mesh->GetTriangleCount();
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("numCuts"), NumCuts);
    Result->SetNumberField(TEXT("cutsApplied"), CutsApplied);
    Result->SetNumberField(TEXT("offset"), Offset);
    Result->SetStringField(TEXT("axis"), Axis);
    Result->SetNumberField(TEXT("trianglesBefore"), TrisBefore);
    Result->SetNumberField(TEXT("trianglesAfter"), TrisAfter);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Loop cut applied using plane cutting"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Split Normals Operation
// -------------------------------------------------------------------------

bool HandleEdgeSplit(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                            const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));

    // Parse edge indices to split (can be array or single number)
    TArray<int32> EdgeIndices;
    const TArray<TSharedPtr<FJsonValue>>* EdgeArray = nullptr;
    if (Payload->TryGetArrayField(TEXT("edges"), EdgeArray))
    {
        for (const auto& Val : *EdgeArray)
        {
            if (Val.IsValid() && Val->Type == EJson::Number)
            {
                EdgeIndices.Add(static_cast<int32>(Val->AsNumber()));
            }
        }
    }
    else
    {
        // Single edge index
        int32 EdgeIndex = GetIntFieldGeom(Payload, TEXT("edgeIndex"), -1);
        if (EdgeIndex >= 0)
        {
            EdgeIndices.Add(EdgeIndex);
        }
    }

    double SplitFactor = GetNumberFieldGeom(Payload, TEXT("splitFactor"), 0.5);
    bool bWeldVertices = GetBoolFieldGeom(Payload, TEXT("weldVertices"), true);
    double WeldTolerance = GetNumberFieldGeom(Payload, TEXT("weldTolerance"), 0.0001);

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

    // Use FDynamicMesh3 directly for edge splitting
    // GeometryScript doesn't have a direct edge split function, so we use the low-level API
    UE::Geometry::FDynamicMesh3& DynMesh = Mesh->GetMeshRef();

    int32 EdgesSplit = 0;
    for (int32 EdgeID : EdgeIndices)
    {
        if (DynMesh.IsEdge(EdgeID))
        {
            // Get edge vertices
            UE::Geometry::FIndex2i EdgeV = DynMesh.GetEdgeV(EdgeID);

            // Compute midpoint position
            FVector3d V0 = DynMesh.GetVertex(EdgeV.A);
            FVector3d V1 = DynMesh.GetVertex(EdgeV.B);
            FVector3d Midpoint = V0 + (V1 - V0) * SplitFactor;

            // Insert new vertex at midpoint
            int32 NewVertexID = DynMesh.AppendVertex(Midpoint);

            // Split triangles connected to this edge
            UE::Geometry::FIndex2i EdgeT = DynMesh.GetEdgeT(EdgeID);
            TArray<int32> TrisToModify;
            if (EdgeT.A >= 0) TrisToModify.Add(EdgeT.A);
            if (EdgeT.B >= 0) TrisToModify.Add(EdgeT.B);

            for (int32 TriID : TrisToModify)
            {
                if (DynMesh.IsTriangle(TriID))
                {
                    UE::Geometry::FIndex3i Tri = DynMesh.GetTriangle(TriID);

                    // Find which edges to split
                    int32 ReplaceV = -1;
                    int32 KeepV1 = -1, KeepV2 = -1;

                    if (Tri.A == EdgeV.A && Tri.B == EdgeV.B) { ReplaceV = Tri.B; KeepV1 = Tri.A; KeepV2 = Tri.C; }
                    else if (Tri.B == EdgeV.A && Tri.C == EdgeV.B) { ReplaceV = Tri.C; KeepV1 = Tri.A; KeepV2 = Tri.B; }
                    else if (Tri.C == EdgeV.A && Tri.A == EdgeV.B) { ReplaceV = Tri.A; KeepV1 = Tri.B; KeepV2 = Tri.C; }
                    else if (Tri.A == EdgeV.B && Tri.B == EdgeV.A) { ReplaceV = Tri.B; KeepV1 = Tri.A; KeepV2 = Tri.C; }
                    else if (Tri.B == EdgeV.B && Tri.C == EdgeV.A) { ReplaceV = Tri.C; KeepV1 = Tri.A; KeepV2 = Tri.B; }
                    else if (Tri.C == EdgeV.B && Tri.A == EdgeV.A) { ReplaceV = Tri.A; KeepV1 = Tri.B; KeepV2 = Tri.C; }

                    if (ReplaceV >= 0)
                    {
                        // Remove old triangle and add two new ones
                        DynMesh.RemoveTriangle(TriID);
                        DynMesh.AppendTriangle(KeepV1, NewVertexID, KeepV2);
                        DynMesh.AppendTriangle(NewVertexID, ReplaceV, KeepV2);
                        EdgesSplit++;
                    }
                }
            }
        }
    }

    // Weld vertices if requested
    if (bWeldVertices && WeldTolerance > 0)
    {
        FGeometryScriptWeldEdgesOptions WeldOptions;
        WeldOptions.Tolerance = WeldTolerance;
        WeldOptions.bOnlyUniquePairs = true;
        UGeometryScriptLibrary_MeshRepairFunctions::WeldMeshEdges(Mesh, WeldOptions, nullptr);
    }

    DMC->NotifyMeshUpdated();

    int32 TrisAfter = Mesh->GetTriangleCount();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("edgesSplit"), EdgesSplit);
    Result->SetNumberField(TEXT("trianglesBefore"), TrisBefore);
    Result->SetNumberField(TEXT("trianglesAfter"), TrisAfter);

    // Add verification data
    McpHandlerUtils::AddVerification(Result, TargetActor);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Edge split applied"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Quadrangulate Operations
// -------------------------------------------------------------------------
} // namespace McpGeometryHandlers

#endif // WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT
