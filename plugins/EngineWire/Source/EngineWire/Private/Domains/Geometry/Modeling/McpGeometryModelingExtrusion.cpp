#include "Domains/Geometry/McpGeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleExtrude(UEngineWireSubsystem* Self, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    const FVector Offset = ExtractVectorField(Payload, TEXT("offset"), FVector::ZeroVector);
    // extrude declares amount, so it wins over the distance spelling the other face operators use.
    const double Distance = Offset.IsNearlyZero()
        ? GetJsonNumberField(Payload, TEXT("amount"), GetJsonNumberField(Payload, TEXT("distance"), 10.0)) : Offset.Size();
    const FVector Direction = Offset.IsNearlyZero() ? FVector::UpVector : Offset.GetSafeNormal();

    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;
    auto [TargetActor, DMC, Mesh] = *Target;

    FGeometryScriptMeshLinearExtrudeOptions ExtrudeOptions;
    ExtrudeOptions.Distance = Distance;
    ExtrudeOptions.Direction = Direction;
    ExtrudeOptions.DirectionMode = EGeometryScriptLinearExtrudeDirection::FixedDirection;

    FGeometryScriptMeshSelection Selection;
    bool bHasSelection = false;
    if (!ReadTriangleSelection(Self, RequestId, Socket, Mesh, Payload, Selection, bHasSelection)) return true;
    const int32 TrianglesSelected = SelectedTriangleCount(Mesh, Selection, bHasSelection);

    UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshLinearExtrudeFaces(
        Mesh, ExtrudeOptions, Selection, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("distance"), Distance);
    Result->SetNumberField(TEXT("trianglesSelected"), TrianglesSelected);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Extrude applied"), Result);
    return true;
}

bool HandleInsetOutset(UEngineWireSubsystem* Self, const FString& RequestId,
                              const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket,
                              bool bIsInset)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    double Distance = FaceOpDistance(Payload, 5.0);

    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;
    auto [TargetActor, DMC, Mesh] = *Target;

    FGeometryScriptMeshInsetOutsetFacesOptions Options;
    Options.Distance = bIsInset ? -Distance : Distance;  // Negative for inset
    Options.bReproject = true;

    FGeometryScriptMeshSelection Selection;
    bool bHasSelection = false;
    if (!ReadTriangleSelection(Self, RequestId, Socket, Mesh, Payload, Selection, bHasSelection)) return true;
    const int32 TrianglesSelected = SelectedTriangleCount(Mesh, Selection, bHasSelection);

    UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshInsetOutsetFaces(
        Mesh, Options, Selection, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("operation"), bIsInset ? TEXT("inset") : TEXT("outset"));
    Result->SetNumberField(TEXT("distance"), Distance);
    Result->SetNumberField(TEXT("trianglesSelected"), TrianglesSelected);
    Self->SendAutomationResponse(Socket, RequestId, true, bIsInset ? TEXT("Inset applied") : TEXT("Outset applied"), Result);
    return true;
}

namespace
{
// The material id most of the triangles carry (every triangle when Triangles is null), lowest first on a tie: a bevel
// face takes it. The engine infers a face's id from the two faces it joins only on a flat cut (its rounded strips are
// rows its check does not count), and otherwise sets one id for all, 0 by default.
int32 CommonMaterialId(UDynamicMesh* Mesh, const TArray<int32>* Triangles)
{
    TMap<int32, int32> Counts;
    Mesh->ProcessMesh([&](const UE::Geometry::FDynamicMesh3& ReadMesh)
    {
        const UE::Geometry::FDynamicMeshMaterialAttribute* Ids = ReadMesh.HasAttributes() ? ReadMesh.Attributes()->GetMaterialID() : nullptr;
        const auto Count = [&](int32 TriangleId) { ++Counts.FindOrAdd(Ids ? Ids->GetValue(TriangleId) : 0); };
        if (!Triangles)
        {
            for (const int32 TriangleId : ReadMesh.TriangleIndicesItr()) Count(TriangleId);
            return;
        }
        for (const int32 TriangleId : *Triangles)
        {
            if (ReadMesh.IsTriangle(TriangleId)) Count(TriangleId);
        }
    });
    int32 Best = 0;
    int32 BestCount = 0;
    for (const TPair<int32, int32>& Entry : Counts)
    {
        if (Entry.Value > BestCount || (Entry.Value == BestCount && Entry.Key < Best))
        {
            Best = Entry.Key;
            BestCount = Entry.Value;
        }
    }
    return Best;
}
} // namespace

bool HandleBevel(UEngineWireSubsystem* Self, const FString& RequestId,
                        const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    double BevelDistance = FaceOpDistance(Payload, 5.0);
    // No segments = a single flat chamfer (the chamfer action routes here).
    int32 Subdivisions = GetJsonIntField(Payload, TEXT("segments"), 0);
#if !(ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4)
    if (Subdivisions > 0)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("segments (rounded bevel subdivisions) needs UE 5.4 or later; omit it for a flat bevel."), TEXT("UNSUPPORTED_VERSION"));
        return true;
    }
#endif

    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;
    auto [TargetActor, DMC, Mesh] = *Target;

    FGeometryScriptMeshSelection BevelSelection;
    bool bHasBevelSelection = false;
    TArray<int32> Picked;
    if (!ReadTriangleSelection(Self, RequestId, Socket, Mesh, Payload, BevelSelection, bHasBevelSelection, &Picked)) return true;
    const int32 TrianglesSelected = SelectedTriangleCount(Mesh, BevelSelection, bHasBevelSelection);
    const int32 TrianglesBefore = Mesh->GetTriangleCount();
    // A flat cut between two faces of one material id takes that id; every other bevel face the commonest id of the
    // faces worked on (the engine set id 0 on them all).
    const int32 MaterialId = CommonMaterialId(Mesh, bHasBevelSelection ? &Picked : nullptr);
    if (bHasBevelSelection)
    {
        // Every PolyGroup edge of the picked faces, as with no selection for the whole mesh. The engine's TriangleArea
        // mode bevels only where picked meets unpicked, so whole parts picked (a box's six faces, a material id) got none.
        FGeometryScriptMeshBevelSelectionOptions SelectionOptions;
        SelectionOptions.BevelDistance = BevelDistance;
        SelectionOptions.bInferMaterialID = true;
        SelectionOptions.SetMaterialID = MaterialId;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
        SelectionOptions.Subdivisions = Subdivisions;
#endif
        UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshBevelSelection(
            Mesh, BevelSelection, EGeometryScriptMeshBevelSelectionMode::AllPolygroupEdges, SelectionOptions, nullptr);
    }
    else
    {
        FGeometryScriptMeshBevelOptions BevelOptions;
        BevelOptions.BevelDistance = BevelDistance;
        BevelOptions.bInferMaterialID = true;
        BevelOptions.SetMaterialID = MaterialId;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
        BevelOptions.Subdivisions = Subdivisions;
#endif
        UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshPolygroupBevel(Mesh, BevelOptions, nullptr);
    }
    // Each beveled edge adds a strip of faces, so a count that did not move means nothing was beveled.
    const int32 TrianglesAdded = Mesh->GetTriangleCount() - TrianglesBefore;
    if (TrianglesAdded == 0)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT(
            "Nothing was beveled: the %d triangle(s) worked on have no PolyGroup edge, the line where one PolyGroup meets "
            "another, which is what bevel and chamfer cut (a mesh whose triangles all share one PolyGroup has none). The mesh "
            "is unchanged. Meshes built by create_box, append_polygons, append_cuboids or append_loft give each face its own "
            "PolyGroup."), TrianglesSelected), TEXT("NOTHING_BEVELED"));
        return true;
    }

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("distance"), BevelDistance);
    Result->SetNumberField(TEXT("segments"), Subdivisions);
    Result->SetNumberField(TEXT("trianglesSelected"), TrianglesSelected);
    Result->SetNumberField(TEXT("trianglesAdded"), TrianglesAdded);
    Result->SetNumberField(TEXT("triangleCount"), Mesh->GetTriangleCount());
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Bevel applied"), Result);
    return true;
}
} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
