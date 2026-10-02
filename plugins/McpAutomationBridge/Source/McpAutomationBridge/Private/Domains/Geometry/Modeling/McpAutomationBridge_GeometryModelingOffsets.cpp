#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleOffsetFaces(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                              const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    double Distance = FaceOpDistance(Payload, 5.0);

    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;
    auto [TargetActor, DMC, Mesh] = *Target;

    // UE 5.7: FGeometryScriptMeshOffsetFacesOptions uses Distance not OffsetDistance
    FGeometryScriptMeshOffsetFacesOptions Options;
    Options.Distance = Distance;

    FGeometryScriptMeshSelection Selection;
    bool bHasSelection = false;
    if (!ReadTriangleSelection(Self, RequestId, Socket, Mesh, Payload, Selection, bHasSelection)) return true;
    const int32 TrianglesSelected = SelectedTriangleCount(Mesh, Selection, bHasSelection);

    UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshOffsetFaces(
        Mesh, Options, Selection, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("distance"), Distance);
    Result->SetNumberField(TEXT("trianglesSelected"), TrianglesSelected);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Offset faces applied"), Result);
    return true;
}

bool HandleShell(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                        const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    double Thickness = GetJsonNumberField(Payload, TEXT("thickness"), 5.0);

    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;
    auto [TargetActor, DMC, Mesh] = *Target;

    FGeometryScriptMeshOffsetOptions Options;
    Options.OffsetDistance = -Thickness;  // Negative to go inward for shell

    UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshShell(
        Mesh, Options, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("thickness"), Thickness);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Shell/solidify applied"), Result);
    return true;
}

} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
