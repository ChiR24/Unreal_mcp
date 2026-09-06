#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleRecomputeTangents(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));

    ADynamicMeshActor* TargetActor = nullptr;
    UDynamicMeshComponent* DMC = nullptr;
    UDynamicMesh* Mesh = nullptr;
    if (!ResolveDynamicMeshForGeometry(Self, RequestId, ActorName, Socket, TargetActor, DMC, Mesh))
    {
        return true;
    }

    // Recompute tangents using MikkT space
    FGeometryScriptTangentsOptions TangentOptions;
    UGeometryScriptLibrary_MeshNormalsFunctions::ComputeTangents(Mesh, TangentOptions, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Tangents recomputed"), Result);
    return true;
}

bool HandleSplitNormals(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                               const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    double SplitAngle = GetJsonNumberField(Payload, TEXT("splitAngle"), 60.0);

    ADynamicMeshActor* TargetActor = nullptr;
    UDynamicMeshComponent* DMC = nullptr;
    UDynamicMesh* Mesh = nullptr;
    if (!ResolveDynamicMeshForGeometry(Self, RequestId, ActorName, Socket, TargetActor, DMC, Mesh))
    {
        return true;
    }

    // UE 5.7: SplitAngle was removed from FGeometryScriptCalculateNormalsOptions
    // Use ComputeSplitNormals with FGeometryScriptSplitNormalsOptions instead
    FGeometryScriptSplitNormalsOptions SplitOptions;
    SplitOptions.bSplitByOpeningAngle = true;
    SplitOptions.OpeningAngleDeg = SplitAngle;
    SplitOptions.bSplitByFaceGroup = false;

    FGeometryScriptCalculateNormalsOptions CalcOptions;
    CalcOptions.bAngleWeighted = true;
    CalcOptions.bAreaWeighted = true;

    UGeometryScriptLibrary_MeshNormalsFunctions::ComputeSplitNormals(Mesh, SplitOptions, CalcOptions, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("splitAngle"), SplitAngle);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Split normals applied"), Result);
    return true;
}

} // namespace McpGeometryHandlers

#endif // WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT
