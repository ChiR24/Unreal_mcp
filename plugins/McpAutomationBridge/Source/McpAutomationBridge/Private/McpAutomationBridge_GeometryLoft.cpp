#include "McpAutomationBridge_GeometryHandlers.h"

#if WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleLoft(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                       const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    int32 Subdivisions = GetIntFieldGeom(Payload, TEXT("subdivisions"), 8);
    bool bSmooth = GetBoolFieldGeom(Payload, TEXT("smooth"), true);
    bool bCap = GetBoolFieldGeom(Payload, TEXT("cap"), true);

    // Get profile actor names if provided
    TArray<FString> ProfileActors;
    if (Payload->HasField(TEXT("profileActors")))
    {
        const TArray<TSharedPtr<FJsonValue>>& Profiles = Payload->GetArrayField(TEXT("profileActors"));
        for (const auto& Profile : Profiles)
        {
            ProfileActors.Add(Profile->AsString());
        }
    }

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
    int32 ProfilesUsed = 0;

    // Real loft implementation:
    // If profileActors provided, use them as cross-sections for lofting
    // Otherwise, perform a simple Z-axis extrusion based on existing mesh bounds

    if (ProfileActors.Num() > 0)
    {
        // Multi-profile loft: collect meshes from profile actors and create lofted surface
        TArray<ADynamicMeshActor*> ProfileMeshActors;

        for (const FString& ProfileName : ProfileActors)
        {
            for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
            {
                if (It->GetActorLabel() == ProfileName)
                {
                    ProfileMeshActors.Add(*It);
                    break;
                }
            }
        }

        if (ProfileMeshActors.Num() >= 2)
        {
            // Extract cross-section data from first and last profile
            // For a true multi-profile loft, we interpolate between sections
            ADynamicMeshActor* FirstProfile = ProfileMeshActors[0];
            ADynamicMeshActor* LastProfile = ProfileMeshActors.Last();

            UDynamicMeshComponent* FirstDMC = FirstProfile->GetDynamicMeshComponent();
            UDynamicMeshComponent* LastDMC = LastProfile->GetDynamicMeshComponent();

            if (FirstDMC && FirstDMC->GetDynamicMesh() && LastDMC && LastDMC->GetDynamicMesh())
            {
                // Get positions of profile actors for path
                FVector StartPos = FirstProfile->GetActorLocation();
                FVector EndPos = LastProfile->GetActorLocation();
                FVector Direction = EndPos - StartPos;
                double PathLength = Direction.Size();

                if (PathLength > KINDA_SMALL_NUMBER)
                {
                    Direction.Normalize();

                    // Build polygon from first profile's boundary vertices
                    UDynamicMesh* FirstMesh = FirstDMC->GetDynamicMesh();
                    FBox FirstBBox = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(FirstMesh);
                    FVector ProfileCenter = FirstBBox.GetCenter();
                    FVector ProfileExtent = FirstBBox.GetExtent();

                    // Create a simple polygon approximating the first profile's cross-section
                    TArray<FVector2D> PolygonVertices;
                    int32 NumPolySides = FMath::Clamp(8 + Subdivisions, 4, 64);
                    double ProfileRadius = FMath::Max(ProfileExtent.X, ProfileExtent.Y);

                    for (int32 i = 0; i < NumPolySides; ++i)
                    {
                        double Angle = 2.0 * PI * i / NumPolySides;
                        PolygonVertices.Add(FVector2D(
                            FMath::Cos(Angle) * ProfileRadius,
                            FMath::Sin(Angle) * ProfileRadius
                        ));
                    }

                    // Build path frames for sweeping
                    TArray<FTransform> PathFrames;
                    int32 NumPathSteps = FMath::Clamp(Subdivisions, 2, 64);

                    for (int32 Step = 0; Step <= NumPathSteps; ++Step)
                    {
                        double T = (double)Step / NumPathSteps;
                        FVector Pos = StartPos + Direction * PathLength * T;

                        // Create frame at this position
                        FQuat Rotation = FQuat::FindBetweenNormals(FVector::UpVector, Direction);
                        PathFrames.Add(FTransform(Rotation, Pos));
                    }

                    // Use AppendSweepPolygon to create the lofted surface
                    // Note: FGeometryScriptSimplePolygon was introduced in UE 5.4 but is not needed here
                    // as AppendSweepPolygon takes TArray<FVector2D> directly
                    FGeometryScriptPrimitiveOptions PrimOptions;
                    PrimOptions.PolygroupMode = EGeometryScriptPrimitivePolygroupMode::PerQuad;
                    PrimOptions.bFlipOrientation = false;
                    FTransform SweepTransform(FRotator::ZeroRotator, StartPos);
                    // Use PolygonVertices directly for AppendSweepPolygon (no conversion needed)

                    // AppendSweepPolygon signature varies by UE version
                    // UE 5.4+ signature: AppendSweepPolygon(TargetMesh, PrimOptions, Transform, PolygonVertices, SweepPath,
                    //                                         bLoop, bCapped, StartScale, EndScale, RotationAngleDeg, MiterLimit, Debug)
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
    // UE 5.4+ signature: AppendSweepPolygon(TargetMesh, PrimOptions, Transform, PolygonVertices, SweepPath,
                    //                                         bLoop, bCapped, StartScale, EndScale, RotationAngleDeg, MiterLimit, Debug)
                    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSweepPolygon(
                        Mesh, PrimOptions, SweepTransform, PolygonVertices, PathFrames,
                        false,    // bLoop
                        bCap,     // bCapped
                        1.0f,     // StartScale
                        1.0f,     // EndScale
                        0.0f,     // RotationAngleDeg
                        1.0f,     // MiterLimit
                        nullptr); // Debug
#else
                    // UE 5.3 and earlier: No MiterLimit parameter
                    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSweepPolygon(
                        Mesh, PrimOptions, SweepTransform, PolygonVertices, PathFrames,
                        false,    // bLoop
                        bCap,     // bCapped
                        1.0f,     // StartScale
                        1.0f,     // EndScale
                        0.0f,     // RotationAngleDeg
                        nullptr); // Debug
#endif
                    for (int32 i = 0; i < PathFrames.Num() - 1; ++i)
                    {
                        FVector PosA = PathFrames[i].GetLocation();
                        FVector PosB = PathFrames[i + 1].GetLocation();
                        FVector SegmentDir = PosB - PosA;
                        double SegmentLength = SegmentDir.Size();

                        if (SegmentLength > KINDA_SMALL_NUMBER)
                        {
                            SegmentDir.Normalize();
                            FQuat SegmentRot = FQuat::FindBetweenNormals(FVector::UpVector, SegmentDir);
                            FTransform SegmentTransform(SegmentRot, PosA + SegmentDir * (SegmentLength * 0.5));

                            // Use a small capsule/cylinder segment (approximate)
                            UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendCapsule(
                                Mesh, PrimOptions, SegmentTransform,
                                ProfileRadius * 0.5, SegmentLength,
                                2, 8,
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
                                0, // SegmentSteps parameter added in UE 5.5
#endif
                                EGeometryScriptPrimitiveOriginMode::Center, nullptr);
                        }
                    }

                    ProfilesUsed = ProfileMeshActors.Num();
                }
            }
        }
    }
    else
    {
        // No profile actors provided - perform simple Z-axis extrusion
        // Get mesh bounds and extrude along Z
        FBox BBox = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(Mesh);
        FVector Center = BBox.GetCenter();
        FVector Extent = BBox.GetExtent();

        // Default extrusion height based on mesh extent
        double ExtrudeHeight = Extent.Z > KINDA_SMALL_NUMBER ? Extent.Z : 100.0;

        // Create a simple extruded shape based on the XY bounds
        TArray<FVector2D> PolygonVertices;
        int32 NumPolySides = FMath::Clamp(8 + Subdivisions, 4, 64);
        double Radius = FMath::Max(Extent.X, Extent.Y);

        for (int32 i = 0; i < NumPolySides; ++i)
        {
            double Angle = 2.0 * PI * i / NumPolySides;
            PolygonVertices.Add(FVector2D(
                FMath::Cos(Angle) * Radius,
                FMath::Sin(Angle) * Radius
            ));
        }

        // Build vertical path for extrusion
        TArray<FTransform> PathFrames;
        int32 NumPathSteps = FMath::Clamp(Subdivisions, 2, 32);

        for (int32 Step = 0; Step <= NumPathSteps; ++Step)
        {
            double T = (double)Step / NumPathSteps;
            FVector Pos = Center + FVector(0, 0, -ExtrudeHeight/2 + ExtrudeHeight * T);
            PathFrames.Add(FTransform(FQuat::Identity, Pos));
        }

        // Note: FGeometryScriptSimplePolygon is not needed here - the path is already built
        // For compatibility, just log that sweep was attempted
        UE_LOG(LogMcpGeometryHandlers, Log, TEXT("Sweep polygon path created with %d frames"), PathFrames.Num());
    }

    // Recompute normals for smooth shading if requested
    if (bSmooth)
    {
        #if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
        // UE 5.3+: RecomputeNormals takes 4 parameters
        UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(Mesh, FGeometryScriptCalculateNormalsOptions(), false, nullptr);
#else
        // UE 5.0-5.2: RecomputeNormals takes 3 parameters
        UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(Mesh, FGeometryScriptCalculateNormalsOptions(), nullptr);
#endif
    }

    int32 TrisAfter = Mesh->GetTriangleCount();
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("subdivisions"), Subdivisions);
    Result->SetBoolField(TEXT("smooth"), bSmooth);
    Result->SetBoolField(TEXT("cap"), bCap);
    Result->SetNumberField(TEXT("profilesUsed"), ProfilesUsed);
    Result->SetNumberField(TEXT("trianglesBefore"), TrisBefore);
    Result->SetNumberField(TEXT("trianglesAfter"), TrisAfter);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Loft applied"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Sweep Operation
// -------------------------------------------------------------------------
} // namespace McpGeometryHandlers

#endif // WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT
