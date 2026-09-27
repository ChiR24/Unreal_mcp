#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
void ScaleMesh(UDynamicMesh* Mesh, const FVector& Scale)
{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
    UGeometryScriptLibrary_MeshTransformFunctions::ScaleMesh(Mesh, Scale, FVector::ZeroVector, true, nullptr);
#else
    UE::Geometry::FDynamicMesh3& EditMesh = Mesh->GetMeshRef();
    for (int32 VID : EditMesh.VertexIndicesItr())
    {
        EditMesh.SetVertex(VID, EditMesh.GetVertex(VID) * FVector3d(Scale));
    }
#endif
}

bool HandleMirror(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                         const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    const FVector Axis = AxisVectorFromPayload(Payload, FVector::ForwardVector);
    const FVector Center = ExtractVectorField(Payload, TEXT("center"), FVector::ZeroVector);

    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;
    auto [TargetActor, DMC, Mesh] = *Target;

    UDynamicMesh* MirroredMesh = NewObject<UDynamicMesh>(GetTransientPackage());
    MirroredMesh->SetMesh(Mesh->GetMeshRef());
    // Mirror across the plane through center (mesh space) = scale by -1 along the axis about center.
    UGeometryScriptLibrary_MeshTransformFunctions::TranslateMesh(MirroredMesh, -Center, nullptr);
    ScaleMesh(MirroredMesh, FVector::OneVector - 2.0 * Axis);
    UGeometryScriptLibrary_MeshTransformFunctions::TranslateMesh(MirroredMesh, Center, nullptr);

    FGeometryScriptAppendMeshOptions AppendOptions;
    UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(Mesh, MirroredMesh, FTransform::Identity, false, AppendOptions, nullptr);

    FGeometryScriptWeldEdgesOptions WeldOptions;
    WeldOptions.Tolerance = 0.001f;
    UGeometryScriptLibrary_MeshRepairFunctions::WeldMeshEdges(Mesh, WeldOptions, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("axis"), GetJsonStringField(Payload, TEXT("axis"), TEXT("X")).ToUpper());
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Mirror applied"), Result);
    return true;
}

bool HandleTranslateMesh(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    FVector Translation = ExtractVectorField(Payload, TEXT("translation"), FVector::ZeroVector);

    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;
    auto [TargetActor, DMC, Mesh] = *Target;

    UGeometryScriptLibrary_MeshTransformFunctions::TranslateMesh(Mesh, Translation, nullptr);
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetObjectField(TEXT("translation"), McpHandlerUtils::VectorToJson(Translation));
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Mesh translated"), Result);
    return true;
}

} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
