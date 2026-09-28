#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
// Generates simple collision from the mesh onto its component; returns the resulting shape count.
static int32 ApplyMeshCollision(UDynamicMesh* Mesh, UDynamicMeshComponent* DMC, EGeometryScriptCollisionGenerationMethod Method, int32 MaxHulls)
{
    FGeometryScriptCollisionFromMeshOptions CollisionOptions;
    CollisionOptions.bEmitTransaction = false;
    CollisionOptions.Method = Method;
    CollisionOptions.MaxConvexHullsPerMesh = MaxHulls;
    UGeometryScriptLibrary_CollisionFunctions::SetDynamicMeshCollisionFromMesh(Mesh, DMC, CollisionOptions, nullptr);
    return UGeometryScriptLibrary_CollisionFunctions::GetSimpleCollisionShapeCount(
        UGeometryScriptLibrary_CollisionFunctions::GetSimpleCollisionFromComponent(DMC, nullptr));
}
#endif

bool HandleGenerateCollision(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, bool bComplex)
{
    // generate_complex_collision is convex decomposition into maxHullCount (or hullCount) hulls.
    const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    const FString CollisionType = bComplex ? TEXT("convex_decomposition") : GetJsonStringField(Payload, TEXT("collisionType"), TEXT("convex"));

    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
    EGeometryScriptCollisionGenerationMethod Method = EGeometryScriptCollisionGenerationMethod::MinVolumeShapes;
    int32 MaxHulls = 1;
    if (CollisionType == TEXT("box") || CollisionType == TEXT("boxes")) Method = EGeometryScriptCollisionGenerationMethod::AlignedBoxes;
    else if (CollisionType == TEXT("sphere") || CollisionType == TEXT("spheres")) Method = EGeometryScriptCollisionGenerationMethod::MinimalSpheres;
    else if (CollisionType == TEXT("capsule") || CollisionType == TEXT("capsules")) Method = EGeometryScriptCollisionGenerationMethod::Capsules;
    else if (CollisionType == TEXT("convex")) Method = EGeometryScriptCollisionGenerationMethod::ConvexHulls;
    else if (CollisionType == TEXT("convex_decomposition"))
    {
        Method = EGeometryScriptCollisionGenerationMethod::ConvexHulls;
        MaxHulls = FMath::Clamp(GetJsonIntField(Payload, TEXT("maxHullCount"), GetJsonIntField(Payload, TEXT("hullCount"), 8)), 1, 64);
    }
    else
    {
        // An unknown type used to fall back to MinVolumeShapes while being echoed back as applied.
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Unknown collisionType '%s'; use box, sphere, capsule, convex or convex_decomposition."), *CollisionType), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    const int32 ShapeCount = ApplyMeshCollision(Target->Mesh, Target->Component, Method, MaxHulls);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("collisionType"), CollisionType);
    Result->SetNumberField(TEXT("shapeCount"), ShapeCount);
    McpHandlerUtils::AddVerification(Result, Target->Actor);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Collision generated"), Result);
#else
    Self->SendAutomationError(Socket, RequestId, TEXT("Collision generation requires UE 5.4+"), TEXT("VERSION_NOT_SUPPORTED"));
#endif
    return true;
}

bool HandleSimplifyCollision(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    const double SimplificationFactor = GetJsonNumberField(Payload, TEXT("simplificationFactor"), 0.5);
    const int32 TargetHullCount = GetJsonIntField(Payload, TEXT("targetHullCount"), 4);

    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;
    auto [TargetActor, DMC, Mesh] = *Target;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
    FGeometryScriptSimplifyMeshOptions SimplifyOptions;
    SimplifyOptions.Method = EGeometryScriptRemoveMeshSimplificationType::StandardQEM;
    SimplifyOptions.bAllowSeamCollapse = true;
    // Simplify a COPY: decimating the component mesh itself used to wreck the render geometry too.
    const int32 CurrentTris = Mesh->GetTriangleCount();
    UDynamicMesh* CollisionSource = NewObject<UDynamicMesh>(GetTransientPackage());
    CollisionSource->SetMesh(Mesh->GetMeshRef());
    UGeometryScriptLibrary_MeshSimplifyFunctions::ApplySimplifyToTriangleCount(
        CollisionSource, FMath::Max(4, static_cast<int32>(CurrentTris * SimplificationFactor)), SimplifyOptions, nullptr);
    const int32 ShapeCount = ApplyMeshCollision(CollisionSource, DMC, EGeometryScriptCollisionGenerationMethod::ConvexHulls, FMath::Clamp(TargetHullCount, 1, 16));

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("trianglesBefore"), CurrentTris);
    Result->SetNumberField(TEXT("collisionSourceTriangles"), CollisionSource->GetTriangleCount());
    Result->SetNumberField(TEXT("renderTriangles"), Mesh->GetTriangleCount());
    Result->SetNumberField(TEXT("shapeCount"), ShapeCount);
    McpHandlerUtils::AddVerification(Result, TargetActor);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Collision simplified"), Result);
#else
    Self->SendAutomationError(Socket, RequestId, TEXT("Collision simplification requires UE 5.4+"), TEXT("VERSION_NOT_SUPPORTED"));
#endif
    return true;
}

} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
