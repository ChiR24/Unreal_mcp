// McpAutomationBridge_GeometrySubdivideSchemes.cpp — subdivide with scheme catmull_clark, loop or bilinear.
//
// The engine's FSubdividePoly (OpenSubdiv, in the optional ModelingComponentsEditorOnly module) refines a polygon
// cage. For catmull_clark and bilinear the cage is the mesh's polygroups, read through FGroupTopology: every
// polygroup is one face whose corners are the vertices where three or more groups meet, plus the vertices on
// the mesh boundary, so a cube with one polygroup per face is a six-quad cage. Loop refines the triangles
// themselves and needs no groups. FSubdividePoly keeps each polygroup id and gives every output triangle the
// material id most common in its cage face, so set_material_id before subdividing carries through.
#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

#if MCP_HAS_SUBDIVIDE_POLY
#include "GroupTopology.h"
#include "Modules/ModuleManager.h"
#include "Operations/SubdividePoly.h"
#endif

namespace McpGeometryHandlers
{
#if MCP_HAS_SUBDIVIDE_POLY
static bool SubdivPolyModuleReady()
{
    FModuleManager& Modules = FModuleManager::Get();
    return Modules.IsModuleLoaded(TEXT("ModelingComponentsEditorOnly"))
        || (Modules.ModuleExists(TEXT("ModelingComponentsEditorOnly")) && Modules.LoadModule(TEXT("ModelingComponentsEditorOnly")) != nullptr);
}

static const TCHAR* SubdivCageProblem(FSubdividePoly::ETopologyCheckResult Check)
{
    switch (Check)
    {
    case FSubdividePoly::ETopologyCheckResult::NoGroups:
    case FSubdividePoly::ETopologyCheckResult::InsufficientGroups:
        return TEXT("the mesh has no polygroups to use as cage faces");
    case FSubdividePoly::ETopologyCheckResult::UnboundedPolygroup:
        return TEXT("a polygroup has no boundary, so one group covers a whole closed surface");
    case FSubdividePoly::ETopologyCheckResult::MultiBoundaryPolygroup:
        return TEXT("a polygroup has more than one boundary loop, like a ring or a face with a hole");
    default:
        return TEXT("a polygroup has fewer than three corners");
    }
}

static const TCHAR* SubdivCageAdvice()
{
    return TEXT("catmull_clark and bilinear treat each polygroup as one face of a cage. Build the cage with edit_dynamic_mesh append_polygons "
                "(a vertex list and faces, one polygroup per face), or start from a primitive made with one polygroup per face, such as create_box. "
                "scheme loop subdivides the triangles as they are and needs no cage.");
}

// Polygroups ids size arrays inside FGroupTopology, so an id has to stay modest.
static constexpr int32 SubdivMaxPolygroupId = 4 * 1024 * 1024;
#endif

bool SubdivideByScheme(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket,
                       const FMcpGeometryTarget& Target, const FString& ActorName, const FString& Scheme, int32 Level)
{
#if MCP_HAS_SUBDIVIDE_POLY
    if (!SubdivPolyModuleReady())
    {
        Self->SendAutomationError(Socket, RequestId,
            TEXT("catmull_clark, loop and bilinear subdivision need the engine's ModelingComponentsEditorOnly module (the MeshModelingToolset plugin, "
                 "which GeometryScripting enables); it is not loaded in this editor. Use scheme pn instead."), TEXT("SUBDIVISION_UNAVAILABLE"));
        return true;
    }
    const bool bLoop = Scheme == TEXT("loop");
    // A copy: nothing touches the actor's mesh until a subdivision has succeeded.
    UE::Geometry::FDynamicMesh3 Source;
    Target.Mesh->ProcessMesh([&Source](const UE::Geometry::FDynamicMesh3& Original) { Source = Original; });
    const int32 TrianglesBefore = Source.TriangleCount();
    if (bLoop && !Source.HasTriangleGroups())
    {
        // Loop copies each triangle's group to its children, and a mesh without groups reports -1 for every triangle.
        Source.EnableTriangleGroups();
    }

    // FGroupTopology indexes by polygroup id, which is -1 on a mesh that has none.
    bool bGroupsUsable = Source.HasTriangleGroups() && Source.MaxGroupID() <= SubdivMaxPolygroupId;
    if (bGroupsUsable)
    {
        for (const int32 TriangleId : Source.TriangleIndicesItr())
        {
            bGroupsUsable = bGroupsUsable && Source.GetTriangleGroup(TriangleId) >= 0;
        }
    }
    UE::Geometry::FGroupTopology Topology(&Source, false);
    int32 CageFaces = TrianglesBefore;
    int64 EstimatedTriangles = static_cast<int64>(TrianglesBefore) << (2 * Level);
    if (!bLoop)
    {
        FString Problem = TEXT("the mesh has no polygroups to use as cage faces");
        bool bCageOk = false;
        if (bGroupsUsable)
        {
            // Boundary vertices are always corners: a cage's open edge keeps every one of its vertices.
            Topology.ShouldAddExtraCornerAtVert = [&Source](const UE::Geometry::FGroupTopology&, int32 VertexId, const UE::Geometry::FIndex2i&)
            {
                return Source.IsBoundaryVertex(VertexId);
            };
            if (Topology.RebuildTopology())
            {
                FSubdividePoly Check(Topology, Source, Level);
                const FSubdividePoly::ETopologyCheckResult Result = Check.ValidateTopology();
                bCageOk = Result == FSubdividePoly::ETopologyCheckResult::Ok;
                Problem = SubdivCageProblem(Result);
            }
            else
            {
                Problem = TEXT("the polygroup boundaries could not be traced, so the mesh is not a clean manifold");
            }
        }
        if (!bCageOk)
        {
            Self->SendAutomationError(Socket, RequestId,
                FString::Printf(TEXT("The mesh is not a polygon cage: %s. %s"), *Problem, SubdivCageAdvice()), TEXT("INVALID_CAGE"));
            return true;
        }
        // Each cage face of n corners becomes n quads, and every level quadruples the quads; a quad is two triangles.
        int64 Corners = 0;
        for (const UE::Geometry::FGroupTopology::FGroup& Group : Topology.Groups)
        {
            Corners += Group.Boundaries[0].GroupEdges.Num();
        }
        CageFaces = Topology.Groups.Num();
        EstimatedTriangles = (Corners * 2) << (2 * (Level - 1));
    }
    if (!GuardMeshBudget(Self, RequestId, Socket, EstimatedTriangles, TEXT("Subdivide"))) return true;

    // The first pass interpolates UVs when the mesh has them; if the engine cannot map them (a partly unset layer) the
    // second subdivides without. A mesh with no UV layer never asks for them: the engine reads one unchecked through 5.5 (5.6 and later check).
    const bool bHasUvs = Source.HasAttributes() && Source.Attributes()->NumUVLayers() > 0 && Source.Attributes()->GetUVLayer(0)->ElementCount() > 0;
    UE::Geometry::FDynamicMesh3 Subdivided;
    bool bDone = false;
    for (int32 Pass = 0; Pass < 2 && !bDone; ++Pass)
    {
        const bool bUseUvs = bHasUvs && Pass == 0;
        if (Pass == 1 && !bHasUvs)
        {
            break;
        }
        FSubdividePoly Subdivider(Topology, Source, Level);
        Subdivider.SubdivisionScheme = bLoop ? ESubdivisionScheme::Loop : (Scheme == TEXT("bilinear") ? ESubdivisionScheme::Bilinear : ESubdivisionScheme::CatmullClark);
        Subdivider.UVComputationMethod = bUseUvs ? ESubdivisionOutputUVs::Interpolated : ESubdivisionOutputUVs::None;
        bDone = Subdivider.ComputeTopologySubdivision() && Subdivider.ComputeSubdividedMesh(Subdivided);
    }
    // An OpenSubdiv-less engine build hands the mesh back as it was.
    if (!bDone || Subdivided.TriangleCount() <= TrianglesBefore)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(
            TEXT("The engine could not subdivide this mesh with scheme %s (OpenSubdiv refused the topology, or this engine build has no OpenSubdiv). The mesh is unchanged."), *Scheme),
            TEXT("SUBDIVISION_FAILED"));
        return true;
    }
    Target.Mesh->SetMesh(MoveTemp(Subdivided));
    Target.Component->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("scheme"), Scheme);
    Result->SetNumberField(TEXT("level"), Level);
    Result->SetNumberField(TEXT("trianglesBefore"), TrianglesBefore);
    Result->SetNumberField(TEXT("trianglesAfter"), Target.Mesh->GetTriangleCount());
    Result->SetNumberField(TEXT("cageFaces"), CageFaces);
    Result->SetNumberField(TEXT("iterations"), Level);
    Result->SetNumberField(TEXT("originalTriangles"), TrianglesBefore);
    Result->SetNumberField(TEXT("subdividedTriangles"), Target.Mesh->GetTriangleCount());
    McpHandlerUtils::AddVerification(Result, Target.Actor);
    Self->SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("Mesh subdivided (%s, level %d)"), *Scheme, Level), Result);
#else
    Self->SendAutomationError(Socket, RequestId,
        TEXT("catmull_clark, loop and bilinear subdivision need the engine's ModelingComponentsEditorOnly module, which this build of the plugin "
             "was compiled without. Use scheme pn instead."), TEXT("SUBDIVISION_UNAVAILABLE"));
#endif
    return true;
}
} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
