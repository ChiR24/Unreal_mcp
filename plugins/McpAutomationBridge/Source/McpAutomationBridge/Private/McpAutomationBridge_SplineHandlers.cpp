#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_SplineHandlersPrivate.h"

#include "McpAutomationBridgeSubsystem.h"

DEFINE_LOG_CATEGORY(LogMcpSplineHandlers);

bool UMcpAutomationBridgeSubsystem::HandleManageSplinesAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    FString SubAction = GetJsonStringFieldSpline(Payload, TEXT("subAction"), TEXT(""));

    UE_LOG(LogMcpSplineHandlers, Verbose, TEXT("HandleManageSplinesAction: SubAction=%s"), *SubAction);

    if (SubAction == TEXT("create_spline_actor"))
        return HandleCreateSplineActor(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("add_spline_point"))
        return HandleAddSplinePoint(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("remove_spline_point"))
        return HandleRemoveSplinePoint(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("set_spline_point_position"))
        return HandleSetSplinePointPosition(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("set_spline_point_tangents"))
        return HandleSetSplinePointTangents(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("set_spline_point_rotation"))
        return HandleSetSplinePointRotation(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("set_spline_point_scale"))
        return HandleSetSplinePointScale(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("set_spline_type"))
        return HandleSetSplineType(this, RequestId, Payload, Socket);

    if (SubAction == TEXT("create_spline_mesh_component"))
        return HandleCreateSplineMeshComponent(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("create_spline_mesh_actor"))
        return HandleCreateSplineMeshActor(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("set_spline_mesh_asset"))
        return HandleSetSplineMeshAsset(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("configure_spline_mesh_axis"))
        return HandleConfigureSplineMeshAxis(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("set_spline_mesh_material"))
        return HandleSetSplineMeshMaterial(this, RequestId, Payload, Socket);

    if (SubAction == TEXT("scatter_meshes_along_spline"))
        return HandleScatterMeshesAlongSpline(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("configure_mesh_spacing"))
        return HandleConfigureMeshSpacing(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("configure_mesh_randomization"))
        return HandleConfigureMeshRandomization(this, RequestId, Payload, Socket);

    if (SubAction == TEXT("create_road_spline"))
        return HandleCreateRoadSpline(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("create_river_spline"))
        return HandleCreateRiverSpline(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("create_fence_spline"))
        return HandleCreateFenceSpline(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("create_wall_spline"))
        return HandleCreateWallSpline(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("create_cable_spline"))
        return HandleCreateCableSpline(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("create_pipe_spline"))
        return HandleCreatePipeSpline(this, RequestId, Payload, Socket);

    if (SubAction == TEXT("get_splines_info"))
        return HandleGetSplinesInfo(this, RequestId, Payload, Socket);

    SendAutomationResponse(Socket, RequestId, false,
        FString::Printf(TEXT("Unknown spline subAction: %s"), *SubAction), nullptr, TEXT("UNKNOWN_ACTION"));
    return true;
#else
    SendAutomationResponse(Socket, RequestId, false,
        TEXT("Spline operations require editor build"), nullptr, TEXT("EDITOR_ONLY"));
    return true;
#endif
}
