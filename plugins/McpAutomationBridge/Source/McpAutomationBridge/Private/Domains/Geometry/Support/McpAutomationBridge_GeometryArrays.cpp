#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
// The array target plus a snapshot of its current mesh to copy; unset after replying on a bad count or budget.
static TOptional<FMcpGeometryTarget> ResolveArraySource(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                                         const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket,
                                                         int32 Count, UDynamicMesh*& OutSource)
{
    if (Count < 1 || Count > 100)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("count must be between 1 and 100"), TEXT("INVALID_ARGUMENT"));
        return {};
    }
    TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, GetJsonStringField(Payload, TEXT("actorName")), Socket);
    if (!Target || !GuardMeshBudget(Self, RequestId, Socket, static_cast<int64>(Target->Mesh->GetTriangleCount()) * Count, TEXT("Array"))) return {};
    OutSource = NewObject<UDynamicMesh>(GetTransientPackage());
    OutSource->SetMesh(Target->Mesh->GetMeshRef());
    return Target;
}

static bool SendArrayResult(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket,
                            const TSharedPtr<FJsonObject>& Payload, const FMcpGeometryTarget& Target, int32 Count, const TCHAR* Message)
{
    Target.Component->NotifyMeshUpdated();
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), GetJsonStringField(Payload, TEXT("actorName")));
    Result->SetNumberField(TEXT("count"), Count);
    Self->SendAutomationResponse(Socket, RequestId, true, Message, Result);
    return true;
}

bool HandleArrayLinear(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                              const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const int32 Count = GetJsonIntField(Payload, TEXT("count"), 3);
    UDynamicMesh* SourceMesh = nullptr;
    const TOptional<FMcpGeometryTarget> Target = ResolveArraySource(Self, RequestId, Payload, Socket, Count, SourceMesh);
    if (!Target) return true;

    const FTransform RepeatTransform(ExtractVectorField(Payload, TEXT("offset"), FVector(100, 0, 0)));
    UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMeshRepeated(
        Target->Mesh, SourceMesh, RepeatTransform, Count - 1, false, false, FGeometryScriptAppendMeshOptions(), nullptr);
    return SendArrayResult(Self, RequestId, Socket, Payload, *Target, Count, TEXT("Linear array applied"));
}

bool HandleArrayRadial(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                              const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    // Copies turn about the Z axis through center.
    const int32 Count = GetJsonIntField(Payload, TEXT("count"), 6);
    UDynamicMesh* SourceMesh = nullptr;
    const TOptional<FMcpGeometryTarget> Target = ResolveArraySource(Self, RequestId, Payload, Socket, Count, SourceMesh);
    if (!Target) return true;

    const FVector Center = ExtractVectorField(Payload, TEXT("center"), FVector::ZeroVector);
    const double AngleStep = GetJsonNumberField(Payload, TEXT("angle"), 360.0) / Count;
    TArray<FTransform> Transforms;
    for (int32 i = 1; i < Count; ++i) // the original is copy 0
    {
        const FQuat Rotation(FVector::UpVector, FMath::DegreesToRadians(AngleStep * i));
        Transforms.Add(FTransform(Rotation, Center + Rotation.RotateVector(-Center)));
    }
    UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMeshTransformed(
        Target->Mesh, SourceMesh, Transforms, FTransform::Identity, true, false, FGeometryScriptAppendMeshOptions(), nullptr);
    return SendArrayResult(Self, RequestId, Socket, Payload, *Target, Count, TEXT("Radial array applied"));
}

} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
