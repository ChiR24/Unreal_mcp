#include "McpAutomationBridge_GeometryHandlers.h"

#if WITH_EDITOR

DEFINE_LOG_CATEGORY(LogMcpGeometryHandlers);

bool UMcpAutomationBridgeSubsystem::HandleGeometryAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (Action != TEXT("manage_geometry"))
    {
        return false;
    }

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

    using namespace McpGeometryHandlers;

    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing payload"), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString SubAction = GetStringFieldGeom(Payload, TEXT("subAction"));
    if (SubAction.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'subAction' in payload"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Primitives
    if (SubAction == TEXT("create_box")) return HandleCreateBox(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("create_sphere")) return HandleCreateSphere(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("create_cylinder")) return HandleCreateCylinder(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("create_cone")) return HandleCreateCone(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("create_capsule")) return HandleCreateCapsule(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("create_torus")) return HandleCreateTorus(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("create_plane")) return HandleCreatePlane(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("create_disc")) return HandleCreateDisc(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("create_stairs")) return HandleCreateStairs(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("create_spiral_stairs")) return HandleCreateSpiralStairs(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("create_ring")) return HandleCreateRing(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("create_arch")) return HandleCreateArch(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("create_pipe")) return HandleCreatePipe(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("create_ramp")) return HandleCreateRamp(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("revolve")) return HandleRevolve(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("create_procedural_mesh")) return HandleCreateProceduralMesh(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("append_triangle")) return HandleAppendTriangle(this, RequestId, Payload, RequestingSocket);

    // Booleans
    if (SubAction == TEXT("boolean_union")) return HandleBooleanUnion(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("boolean_subtract")) return HandleBooleanSubtract(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("boolean_intersection")) return HandleBooleanIntersection(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("boolean_trim")) return HandleBooleanTrim(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("self_union")) return HandleSelfUnion(this, RequestId, Payload, RequestingSocket);

    // Mesh Utils
    if (SubAction == TEXT("get_mesh_info")) return HandleGetMeshInfo(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("recalculate_normals")) return HandleRecalculateNormals(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("flip_normals")) return HandleFlipNormals(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("simplify_mesh")) return HandleSimplifyMesh(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("subdivide")) return HandleSubdivide(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("auto_uv")) return HandleAutoUV(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("convert_to_static_mesh")) return HandleConvertToStaticMesh(this, RequestId, Payload, RequestingSocket);

    // Modeling Operations
    if (SubAction == TEXT("extrude")) return HandleExtrude(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("inset")) return HandleInsetOutset(this, RequestId, Payload, RequestingSocket, true);
    if (SubAction == TEXT("outset")) return HandleInsetOutset(this, RequestId, Payload, RequestingSocket, false);
    if (SubAction == TEXT("bevel")) return HandleBevel(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("offset_faces")) return HandleOffsetFaces(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("shell")) return HandleShell(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("chamfer")) return HandleChamfer(this, RequestId, Payload, RequestingSocket);

    // Deformers
    if (SubAction == TEXT("bend")) return HandleBend(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("twist")) return HandleTwist(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("taper")) return HandleTaper(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("noise_deform")) return HandleNoiseDeform(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("smooth")) return HandleSmooth(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("relax")) return HandleRelax(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("stretch")) return HandleStretch(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("spherify")) return HandleSpherify(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("cylindrify")) return HandleCylindrify(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("lattice_deform")) return HandleLatticeDeform(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("displace_by_texture")) return HandleDisplaceByTexture(this, RequestId, Payload, RequestingSocket);

    // Mesh Repair
    if (SubAction == TEXT("weld_vertices")) return HandleWeldVertices(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("fill_holes")) return HandleFillHoles(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("remove_degenerates")) return HandleRemoveDegenerates(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("remesh_uniform")) return HandleRemeshUniform(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("merge_vertices")) return HandleMergeVertices(this, RequestId, Payload, RequestingSocket);

    // Collision Generation
    if (SubAction == TEXT("generate_collision")) return HandleGenerateCollision(this, RequestId, Payload, RequestingSocket);

    // Transform Operations
    if (SubAction == TEXT("mirror")) return HandleMirror(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("array_linear")) return HandleArrayLinear(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("array_radial")) return HandleArrayRadial(this, RequestId, Payload, RequestingSocket);

    // Mesh Topology Operations
    if (SubAction == TEXT("triangulate")) return HandleTriangulate(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("poke")) return HandlePoke(this, RequestId, Payload, RequestingSocket);

    // UV Operations
    if (SubAction == TEXT("project_uv")) return HandleProjectUV(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("transform_uvs")) return HandleTransformUVs(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("set_uvs")) return HandleSetUVs(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("set_vertex_color")) return HandleSetVertexColor(this, RequestId, Payload, RequestingSocket);

    // Tangent Operations
    if (SubAction == TEXT("recompute_tangents")) return HandleRecomputeTangents(this, RequestId, Payload, RequestingSocket);

    // Normal Operations
    if (SubAction == TEXT("split_normals")) return HandleSplitNormals(this, RequestId, Payload, RequestingSocket);

    // Advanced Operations (Bridge, Loft, Sweep)
    if (SubAction == TEXT("bridge")) return HandleBridge(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("loft")) return HandleLoft(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("sweep")) return HandleSweep(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("loop_cut")) return HandleLoopCut(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("duplicate_along_spline")) return HandleDuplicateAlongSpline(this, RequestId, Payload, RequestingSocket);

    // Vertex and Triangle Operations
    if (SubAction == TEXT("append_vertex")) return HandleAppendVertex(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("delete_vertex")) return HandleDeleteVertex(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("delete_triangle")) return HandleDeleteTriangle(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("get_vertex_position")) return HandleGetVertexPosition(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("set_vertex_position")) return HandleSetVertexPosition(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("translate_mesh")) return HandleTranslateMesh(this, RequestId, Payload, RequestingSocket);

    // Additional UV Operations
    if (SubAction == TEXT("unwrap_uv")) return HandleUnwrapUV(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("pack_uv_islands")) return HandlePackUVIslands(this, RequestId, Payload, RequestingSocket);

    // Nanite Conversion
    if (SubAction == TEXT("convert_to_nanite")) return HandleConvertToNanite(this, RequestId, Payload, RequestingSocket);

    // Spline-based Operations
    if (SubAction == TEXT("extrude_along_spline")) return HandleExtrudeAlongSpline(this, RequestId, Payload, RequestingSocket);

    // Aliases
    if (SubAction == TEXT("difference")) return HandleBooleanSubtract(this, RequestId, Payload, RequestingSocket);

    // Edge Operations
    if (SubAction == TEXT("edge_split")) return HandleEdgeSplit(this, RequestId, Payload, RequestingSocket);

    // Topology Operations
    if (SubAction == TEXT("quadrangulate")) return HandleQuadrangulate(this, RequestId, Payload, RequestingSocket);

    // Remesh Operations
    if (SubAction == TEXT("remesh_voxel")) return HandleRemeshVoxel(this, RequestId, Payload, RequestingSocket);

    // Complex Collision
    if (SubAction == TEXT("generate_complex_collision")) return HandleGenerateComplexCollision(this, RequestId, Payload, RequestingSocket);

    // Collision Simplification
    if (SubAction == TEXT("simplify_collision")) return HandleSimplifyCollision(this, RequestId, Payload, RequestingSocket);

    // LOD Operations (Geometry-specific)
    if (SubAction == TEXT("generate_lods")) return HandleGenerateLODsGeometry(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("set_lod_settings")) return HandleSetLODSettings(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("set_lod_screen_sizes")) return HandleSetLODScreenSizes(this, RequestId, Payload, RequestingSocket);

    SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Unknown geometry subAction: '%s'"), *SubAction), TEXT("UNKNOWN_SUBACTION"));
    return true;
#else
    // UE 5.0 doesn't have full GeometryScript support
    SendAutomationError(RequestingSocket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
}

#endif // WITH_EDITOR
