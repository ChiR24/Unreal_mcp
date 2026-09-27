#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
namespace
{
// Iterative Laplacian smoothing; smooth and relax differ only in their defaults. `strength` is the step.
bool ApplySmoothing(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                    TSharedPtr<FMcpBridgeWebSocket> Socket, int32 DefaultIterations, double DefaultStep,
                    const TCHAR* Message)
{
    const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    const int32 Iterations = GetJsonIntField(Payload, TEXT("iterations"), DefaultIterations);
    const double Step = GetJsonNumberField(Payload, TEXT("strength"), DefaultStep);

    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;

    FGeometryScriptIterativeMeshSmoothingOptions SmoothOptions;
    SmoothOptions.NumIterations = Iterations;
    SmoothOptions.Alpha = Step;
    UGeometryScriptLibrary_MeshDeformFunctions::ApplyIterativeSmoothingToMesh(Target->Mesh, FGeometryScriptMeshSelection(), SmoothOptions, nullptr);
    Target->Component->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("iterations"), Iterations);
    Result->SetNumberField(TEXT("strength"), Step);
    Self->SendAutomationResponse(Socket, RequestId, true, Message, Result);
    return true;
}
}

bool HandleSmooth(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                         const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return ApplySmoothing(Self, RequestId, Payload, Socket, 10, 0.2, TEXT("Smooth applied"));
}

bool HandleRelax(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                        const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return ApplySmoothing(Self, RequestId, Payload, Socket, 3, 0.5, TEXT("Relax applied"));
}

bool HandleStretch(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    const double Factor = GetJsonNumberField(Payload, TEXT("strength"), 1.5);

    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;

    FVector ScaleVec = FVector::OneVector;
    ScaleVec[AxisIndexFromPayload(Payload)] = Factor;
    ScaleMesh(Target->Mesh, ScaleVec);
    Target->Component->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("axis"), GetJsonStringField(Payload, TEXT("axis"), TEXT("Z")).ToUpper());
    Result->SetNumberField(TEXT("factor"), Factor);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Stretch applied"), Result);
    return true;
}
} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
