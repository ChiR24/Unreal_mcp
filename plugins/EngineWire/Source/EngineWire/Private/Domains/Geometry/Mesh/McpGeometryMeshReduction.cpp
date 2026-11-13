#include "Domains/Geometry/McpGeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleSimplifyMesh(UEngineWireSubsystem* Self, const FString& RequestId,
                               const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));

    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;
    auto [TargetActor, DMC, Mesh] = *Target;

    FGeometryScriptSimplifyMeshOptions SimplifyOptions;
    SimplifyOptions.Method = EGeometryScriptRemoveMeshSimplificationType::StandardQEM;
    SimplifyOptions.bAllowSeamCollapse = true;

    int32 TriCountBefore = Mesh->GetTriangleCount();

    const double ReductionPercent = FMath::Clamp(GetJsonNumberField(Payload, TEXT("reductionPercent"), 50.0), 0.0, 100.0);
    const int32 TargetTriCount = FMath::Max(1, GetJsonIntField(Payload, TEXT("targetTriangleCount"),
        FMath::RoundToInt(TriCountBefore * (1.0 - ReductionPercent / 100.0))));

    UGeometryScriptLibrary_MeshSimplifyFunctions::ApplySimplifyToTriangleCount(
        Mesh,
        TargetTriCount,
        SimplifyOptions,
        nullptr
    );

    int32 TriCountAfter = Mesh->GetTriangleCount();

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("trianglesBefore"), TriCountBefore);
    Result->SetNumberField(TEXT("trianglesAfter"), TriCountAfter);
    Result->SetNumberField(TEXT("reductionPercent"), TriCountBefore > 0 ? (1.0 - ((double)TriCountAfter / (double)TriCountBefore)) * 100.0 : 0.0);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Mesh simplified"), Result);
    return true;
}

bool HandleSubdivide(UEngineWireSubsystem* Self, const FString& RequestId,
                            const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    const FString Scheme = GetJsonStringField(Payload, TEXT("scheme"), TEXT("pn")).ToLower();
    const int32 Iterations = FMath::Clamp(GetJsonIntField(Payload, TEXT("iterations"), 1), 1, MAX_SUBDIVIDE_ITERATIONS);
    if (Scheme != TEXT("pn") && Scheme != TEXT("catmull_clark") && Scheme != TEXT("loop") && Scheme != TEXT("bilinear"))
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Unknown subdivision scheme '%s'; use pn, catmull_clark, loop or bilinear."), *Scheme), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;
    if (Scheme != TEXT("pn"))
    {
        return SubdivideByScheme(Self, RequestId, Socket, *Target, ActorName, Scheme, Iterations);
    }
    auto [TargetActor, DMC, Mesh] = *Target;

    const int32 TriCountBefore = Mesh->GetTriangleCount();
    // Each PN tessellation pass quadruples the triangle count.
    if (!GuardMeshBudget(Self, RequestId, Socket, static_cast<int64>(TriCountBefore) << (2 * Iterations), TEXT("Subdivide"))) return true;

    for (int32 i = 0; i < Iterations; ++i)
    {
        UGeometryScriptLibrary_MeshSubdivideFunctions::ApplyPNTessellation(Mesh, FGeometryScriptPNTessellateOptions(), 1, nullptr);
    }
    const int32 TriCountAfter = Mesh->GetTriangleCount();

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("scheme"), TEXT("pn"));
    Result->SetNumberField(TEXT("level"), Iterations);
    Result->SetNumberField(TEXT("trianglesBefore"), TriCountBefore);
    Result->SetNumberField(TEXT("trianglesAfter"), TriCountAfter);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Mesh subdivided"), Result);
    return true;
}

bool HandleRemeshUniform(UEngineWireSubsystem* Self, const FString& RequestId,
                                const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;
    auto [TargetActor, DMC, Mesh] = *Target;
    const int32 TrisBefore = Mesh->GetTriangleCount();

    FGeometryScriptRemeshOptions RemeshOptions;
    RemeshOptions.bDiscardAttributes = false;
    RemeshOptions.bReprojectToInputMesh = true;
    FGeometryScriptUniformRemeshOptions UniformOptions;
    // The triangle budget every growing operation checks: the target count, or for an edge length the about
    // 2 x area / length^2 triangles a surface of that edge comes out at (the voxel remesh's estimate).
    int64 Estimated = 0;
    if (Payload->HasField(TEXT("targetEdgeLength")))
    {
        const double EdgeLength = GetJsonNumberField(Payload, TEXT("targetEdgeLength"), 10.0);
        if (!FMath::IsFinite(EdgeLength) || EdgeLength <= 0.0)
        {
            Self->SendAutomationError(Socket, RequestId, TEXT("targetEdgeLength must be a positive number of centimetres."), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        double Area = 0.0;
        Mesh->ProcessMesh([&Area](const UE::Geometry::FDynamicMesh3& Source)
        {
            for (const int32 TriangleId : Source.TriangleIndicesItr())
            {
                Area += Source.GetTriArea(TriangleId);
            }
        });
        Estimated = static_cast<int64>(FMath::Min(2.0 * Area / FMath::Square(EdgeLength), 1.0e15));
        UniformOptions.TargetType = EGeometryScriptUniformRemeshTargetType::TargetEdgeLength;
        UniformOptions.TargetEdgeLength = EdgeLength;
    }
    else
    {
        UniformOptions.TargetType = EGeometryScriptUniformRemeshTargetType::TriangleCount;
        UniformOptions.TargetTriangleCount = GetJsonIntField(Payload, TEXT("targetTriangleCount"), 5000);
        Estimated = UniformOptions.TargetTriangleCount;
    }
    if (!GuardMeshBudget(Self, RequestId, Socket, Estimated, TEXT("Uniform remesh"))) return true;
    UGeometryScriptLibrary_RemeshingFunctions::ApplyUniformRemesh(Mesh, RemeshOptions, UniformOptions, nullptr);
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("trianglesBefore"), TrisBefore);
    Result->SetNumberField(TEXT("trianglesAfter"), Mesh->GetTriangleCount());
    McpHandlerUtils::AddVerification(Result, TargetActor);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Uniform remesh applied"), Result);
    return true;
}

} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
