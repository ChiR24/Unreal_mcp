#include "McpAutomationBridge_GeometryHandlers.h"

#if WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleGenerateCollision(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    FString CollisionType = GetStringFieldGeom(Payload, TEXT("collisionType"), TEXT("convex"));

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

// Geometry Script collision API differs between UE 5.4 and 5.5+
// UE 5.4: Use SetDynamicMeshCollisionFromMesh directly
// UE 5.5+: Use GenerateCollisionFromMesh + SetSimpleCollisionOfDynamicMeshComponent
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
    FGeometryScriptCollisionFromMeshOptions CollisionOptions;
    CollisionOptions.bEmitTransaction = false;

    // Set method based on collision type
    if (CollisionType == TEXT("box") || CollisionType == TEXT("boxes"))
    {
        CollisionOptions.Method = EGeometryScriptCollisionGenerationMethod::AlignedBoxes;
    }
    else if (CollisionType == TEXT("sphere") || CollisionType == TEXT("spheres"))
    {
        CollisionOptions.Method = EGeometryScriptCollisionGenerationMethod::MinimalSpheres;
    }
    else if (CollisionType == TEXT("capsule") || CollisionType == TEXT("capsules"))
    {
        CollisionOptions.Method = EGeometryScriptCollisionGenerationMethod::Capsules;
    }
    else if (CollisionType == TEXT("convex"))
    {
        CollisionOptions.Method = EGeometryScriptCollisionGenerationMethod::ConvexHulls;
        CollisionOptions.MaxConvexHullsPerMesh = 1;
    }
    else if (CollisionType == TEXT("convex_decomposition"))
    {
        CollisionOptions.Method = EGeometryScriptCollisionGenerationMethod::ConvexHulls;
        CollisionOptions.MaxConvexHullsPerMesh = 8;
    }
    else
    {
        CollisionOptions.Method = EGeometryScriptCollisionGenerationMethod::MinVolumeShapes;
    }

    FGeometryScriptSimpleCollision Collision = UGeometryScriptLibrary_CollisionFunctions::GenerateCollisionFromMesh(
        Mesh, CollisionOptions, nullptr);

    // Set the collision on the DynamicMeshComponent
    FGeometryScriptSetSimpleCollisionOptions SetOptions;
    UGeometryScriptLibrary_CollisionFunctions::SetSimpleCollisionOfDynamicMeshComponent(
        Collision, DMC, SetOptions, nullptr);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("collisionType"), CollisionType);
    Result->SetNumberField(TEXT("shapeCount"), UGeometryScriptLibrary_CollisionFunctions::GetSimpleCollisionShapeCount(Collision));
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Collision generated"), Result);
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 4
    // UE 5.4: Use SetDynamicMeshCollisionFromMesh directly (GenerateCollisionFromMesh not available)
    FGeometryScriptCollisionFromMeshOptions CollisionOptions;
    CollisionOptions.bEmitTransaction = false;

    // Set method based on collision type
    if (CollisionType == TEXT("box") || CollisionType == TEXT("boxes"))
    {
        CollisionOptions.Method = EGeometryScriptCollisionGenerationMethod::AlignedBoxes;
    }
    else if (CollisionType == TEXT("sphere") || CollisionType == TEXT("spheres"))
    {
        CollisionOptions.Method = EGeometryScriptCollisionGenerationMethod::MinimalSpheres;
    }
    else if (CollisionType == TEXT("capsule") || CollisionType == TEXT("capsules"))
    {
        CollisionOptions.Method = EGeometryScriptCollisionGenerationMethod::Capsules;
    }
    else if (CollisionType == TEXT("convex"))
    {
        CollisionOptions.Method = EGeometryScriptCollisionGenerationMethod::ConvexHulls;
        CollisionOptions.MaxConvexHullsPerMesh = 1;
    }
    else if (CollisionType == TEXT("convex_decomposition"))
    {
        CollisionOptions.Method = EGeometryScriptCollisionGenerationMethod::ConvexHulls;
        CollisionOptions.MaxConvexHullsPerMesh = 8;
    }
    else
    {
        CollisionOptions.Method = EGeometryScriptCollisionGenerationMethod::MinVolumeShapes;
    }

    // UE 5.4: SetDynamicMeshCollisionFromMesh sets collision directly on the component
    UGeometryScriptLibrary_CollisionFunctions::SetDynamicMeshCollisionFromMesh(
        Mesh, DMC, CollisionOptions, nullptr);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("collisionType"), CollisionType);
    Result->SetNumberField(TEXT("shapeCount"), 1); // Approximate count for UE 5.4
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Collision generated"), Result);
#else
    Self->SendAutomationError(Socket, RequestId, TEXT("Collision generation requires UE 5.4+"), TEXT("VERSION_NOT_SUPPORTED"));
#endif
    return true;
}

// -------------------------------------------------------------------------
// Transform Operations (Mirror, Array)
// -------------------------------------------------------------------------

bool HandleGenerateComplexCollision(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                           const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    int32 MaxHullCount = GetIntFieldGeom(Payload, TEXT("maxHullCount"), 8);
    int32 MaxHullVerts = GetIntFieldGeom(Payload, TEXT("maxHullVerts"), 32);
    double HullPrecision = GetNumberFieldGeom(Payload, TEXT("hullPrecision"), 100.0);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
    FGeometryScriptCollisionFromMeshOptions CollisionOptions;
    CollisionOptions.Method = EGeometryScriptCollisionGenerationMethod::ConvexHulls;
    CollisionOptions.MaxConvexHullsPerMesh = FMath::Clamp(MaxHullCount, 1, 64);
    CollisionOptions.bEmitTransaction = false;

    // Generate collision from mesh
    FGeometryScriptSimpleCollision Collision = UGeometryScriptLibrary_CollisionFunctions::GenerateCollisionFromMesh(
        Mesh, CollisionOptions, nullptr);

    // Set the collision on the DynamicMeshComponent
    FGeometryScriptSetSimpleCollisionOptions SetOptions;
    UGeometryScriptLibrary_CollisionFunctions::SetSimpleCollisionOfDynamicMeshComponent(
        Collision, DMC, SetOptions, nullptr);

    int32 ShapeCount = UGeometryScriptLibrary_CollisionFunctions::GetSimpleCollisionShapeCount(Collision);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("hullCount"), MaxHullCount);
    Result->SetNumberField(TEXT("shapeCount"), ShapeCount);
    Result->SetStringField(TEXT("collisionType"), TEXT("convex_decomposition"));

    // Add verification data
    McpHandlerUtils::AddVerification(Result, TargetActor);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Complex collision generated"), Result);
#else
    Self->SendAutomationError(Socket, RequestId, TEXT("Complex collision generation requires UE 5.4+"), TEXT("VERSION_NOT_SUPPORTED"));
#endif
    return true;
}

// -------------------------------------------------------------------------
// Simplify Collision
// -------------------------------------------------------------------------

bool HandleSimplifyCollision(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    double SimplificationFactor = GetNumberFieldGeom(Payload, TEXT("simplificationFactor"), 0.5);
    int32 TargetHullCount = GetIntFieldGeom(Payload, TEXT("targetHullCount"), 4);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
    // Simplify the mesh first, then generate simpler collision
    // Use mesh simplification to reduce geometry before collision generation
    FGeometryScriptSimplifyMeshOptions SimplifyOptions;
    SimplifyOptions.Method = EGeometryScriptRemoveMeshSimplificationType::StandardQEM;
    SimplifyOptions.bAllowSeamCollapse = true;

    // Calculate target triangle count based on simplification factor
    int32 CurrentTris = Mesh->GetTriangleCount();
    int32 TargetTris = FMath::Max(4, static_cast<int32>(CurrentTris * SimplificationFactor));

    UGeometryScriptLibrary_MeshSimplifyFunctions::ApplySimplifyToTriangleCount(
        Mesh, TargetTris, SimplifyOptions, nullptr);

    // Generate simplified collision using SetDynamicMeshCollisionFromMesh (UE 5.4+)
    FGeometryScriptCollisionFromMeshOptions CollisionOptions;
    CollisionOptions.Method = EGeometryScriptCollisionGenerationMethod::ConvexHulls;
    CollisionOptions.MaxConvexHullsPerMesh = FMath::Clamp(TargetHullCount, 1, 16);
    CollisionOptions.bEmitTransaction = false;

    // SetDynamicMeshCollisionFromMesh generates and applies collision in one step
    UGeometryScriptLibrary_CollisionFunctions::SetDynamicMeshCollisionFromMesh(
        Mesh, DMC, CollisionOptions, nullptr);

    // Get collision info from the component for reporting
    FGeometryScriptSetSimpleCollisionOptions SetOptions;
    FGeometryScriptSimpleCollision Collision = UGeometryScriptLibrary_CollisionFunctions::GetSimpleCollisionFromComponent(
        DMC, nullptr);

    int32 ShapeCount = UGeometryScriptLibrary_CollisionFunctions::GetSimpleCollisionShapeCount(Collision);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("trianglesBefore"), CurrentTris);
    Result->SetNumberField(TEXT("trianglesAfter"), Mesh->GetTriangleCount());
    Result->SetNumberField(TEXT("shapeCount"), ShapeCount);

    // Add verification data
    McpHandlerUtils::AddVerification(Result, TargetActor);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Collision simplified"), Result);
#else
    Self->SendAutomationError(Socket, RequestId, TEXT("Collision simplification requires UE 5.4+"), TEXT("VERSION_NOT_SUPPORTED"));
#endif
    return true;
}

// -------------------------------------------------------------------------
// LOD Generation (Geometry)
// -------------------------------------------------------------------------
} // namespace McpGeometryHandlers

#endif // WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT
