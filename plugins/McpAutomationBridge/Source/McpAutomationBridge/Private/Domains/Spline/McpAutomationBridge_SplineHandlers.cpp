#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Spline/McpAutomationBridge_SplineHandlersPrivate.h"

#include "McpAutomationBridgeSubsystem.h"

DEFINE_LOG_CATEGORY(LogMcpSplineHandlers);

bool UMcpAutomationBridgeSubsystem::HandleManageSplinesAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString SubAction = GetJsonStringField(Payload, TEXT("subAction"), TEXT(""));

    UE_LOG(LogMcpSplineHandlers, Verbose, TEXT("HandleManageSplinesAction: SubAction=%s"), *SubAction);

    // Every handler reads actorName, which resolves a label, name or object path; a call that
    // named its spline only by the declared actorPath used to find nothing.
    TSharedPtr<FJsonObject> SplinePayload = Payload;
    const FString ActorPath = GetJsonStringField(Payload, TEXT("actorPath"));
    if (!ActorPath.IsEmpty() && GetJsonStringField(Payload, TEXT("actorName")).IsEmpty())
    {
        SplinePayload = MakeShared<FJsonObject>(*Payload);
        SplinePayload->SetStringField(TEXT("actorName"), ActorPath);
    }

    if (SubAction == TEXT("create_spline_actor"))
        return HandleCreateSplineActor(this, RequestId, SplinePayload, Socket);
    if (SubAction == TEXT("add_spline_point"))
        return HandleAddSplinePoint(this, RequestId, SplinePayload, Socket);
    if (SubAction == TEXT("remove_spline_point"))
        return HandleRemoveSplinePoint(this, RequestId, SplinePayload, Socket);
    if (SubAction == TEXT("set_spline_point_position"))
        return HandleSetSplinePointPosition(this, RequestId, SplinePayload, Socket);
    if (SubAction == TEXT("set_spline_point_tangents"))
        return HandleSetSplinePointTangents(this, RequestId, SplinePayload, Socket);
    if (SubAction == TEXT("set_spline_point_rotation"))
        return HandleSetSplinePointRotation(this, RequestId, SplinePayload, Socket);
    if (SubAction == TEXT("set_spline_point_scale"))
        return HandleSetSplinePointScale(this, RequestId, SplinePayload, Socket);
    if (SubAction == TEXT("set_spline_type"))
        return HandleSetSplineType(this, RequestId, SplinePayload, Socket);

    if (SubAction == TEXT("create_spline_mesh_component"))
        return HandleCreateSplineMeshComponent(this, RequestId, SplinePayload, Socket);
    if (SubAction == TEXT("set_spline_mesh_asset"))
        return HandleSetSplineMeshAsset(this, RequestId, SplinePayload, Socket);
    if (SubAction == TEXT("configure_spline_mesh_axis"))
        return HandleConfigureSplineMeshAxis(this, RequestId, SplinePayload, Socket);
    if (SubAction == TEXT("set_spline_mesh_material"))
        return HandleSetSplineMeshMaterial(this, RequestId, SplinePayload, Socket);

    if (SubAction == TEXT("scatter_meshes_along_spline"))
        return HandleScatterMeshesAlongSpline(this, RequestId, SplinePayload, Socket);
    if (SubAction == TEXT("configure_mesh_spacing"))
        return HandleConfigureMeshSpacing(this, RequestId, SplinePayload, Socket);
    if (SubAction == TEXT("configure_mesh_randomization"))
        return HandleConfigureMeshRandomization(this, RequestId, SplinePayload, Socket);

    if (SubAction == TEXT("create_road_spline"))
        return HandleCreateRoadSpline(this, RequestId, SplinePayload, Socket);
    if (SubAction == TEXT("create_river_spline"))
        return HandleCreateRiverSpline(this, RequestId, SplinePayload, Socket);
    if (SubAction == TEXT("create_fence_spline"))
        return HandleCreateFenceSpline(this, RequestId, SplinePayload, Socket);
    if (SubAction == TEXT("create_wall_spline"))
        return HandleCreateWallSpline(this, RequestId, SplinePayload, Socket);
    if (SubAction == TEXT("create_cable_spline"))
        return HandleCreateCableSpline(this, RequestId, SplinePayload, Socket);
    if (SubAction == TEXT("create_pipe_spline"))
        return HandleCreatePipeSpline(this, RequestId, SplinePayload, Socket);

    if (SubAction == TEXT("get_splines_info"))
        return HandleGetSplinesInfo(this, RequestId, SplinePayload, Socket);

    SendAutomationResponse(Socket, RequestId, false,
        FString::Printf(TEXT("Unknown spline subAction: %s"), *SubAction), nullptr, TEXT("UNKNOWN_ACTION"));
    return true;
}
