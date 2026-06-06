#include "McpAutomationBridge_GeometryHandlers.h"

#if WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleAutoUV(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                         const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));

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

    // UE 5.7: FGeometryScriptAutoUVOptions was removed, use XAtlas directly
    UGeometryScriptLibrary_MeshUVFunctions::AutoGenerateXAtlasMeshUVs(
        Mesh,
        0, // UV Channel
        FGeometryScriptXAtlasOptions(),
        nullptr
    );

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Auto UV generated"), Result);
    return true;
}

bool HandleProjectUV(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                            const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
FString ProjectionType = GetStringFieldGeom(Payload, TEXT("projectionType"), TEXT("box")).ToLower();
    double Scale = GetNumberFieldGeom(Payload, TEXT("scale"), 1.0);
    int32 UVChannel = GetIntFieldGeom(Payload, TEXT("uvChannel"), 0);

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

    // Create projection transform with scale applied
    FTransform ProjectionTransform(FQuat::Identity, FVector::ZeroVector, FVector(Scale));

    // UE 5.7: UV projection option structs removed. Use new function signatures directly.
    // Different projection types now have different function signatures.
    if (ProjectionType == TEXT("box") || ProjectionType == TEXT("cube"))
    {
        // UE 5.7: SetMeshUVsFromBoxProjection(Mesh, UVSetIndex, BoxTransform, Selection, MinIslandTriCount, Debug)
        UGeometryScriptLibrary_MeshUVFunctions::SetMeshUVsFromBoxProjection(
            Mesh, UVChannel, ProjectionTransform, FGeometryScriptMeshSelection(), 2, nullptr);
    }
    else if (ProjectionType == TEXT("planar"))
    {
        // UE 5.7: SetMeshUVsFromPlanarProjection(Mesh, UVSetIndex, PlaneTransform, Selection, Debug)
        UGeometryScriptLibrary_MeshUVFunctions::SetMeshUVsFromPlanarProjection(
            Mesh, UVChannel, ProjectionTransform, FGeometryScriptMeshSelection(), nullptr);
    }
    else if (ProjectionType == TEXT("cylindrical"))
    {
        // UE 5.7: SetMeshUVsFromCylinderProjection(Mesh, UVSetIndex, CylinderTransform, Selection, SplitAngle, Debug)
        UGeometryScriptLibrary_MeshUVFunctions::SetMeshUVsFromCylinderProjection(
            Mesh, UVChannel, ProjectionTransform, FGeometryScriptMeshSelection(), 45.0f, nullptr);
    }
    else
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Unknown projection type: %s. Use: box, planar, cylindrical"), *ProjectionType), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("projectionType"), ProjectionType);
    Result->SetNumberField(TEXT("scale"), Scale);
    Result->SetNumberField(TEXT("uvChannel"), UVChannel);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("UV projection applied"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Tangent Operations
// -------------------------------------------------------------------------

bool HandleTransformUVs(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                               const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    int32 UVChannel = GetIntFieldGeom(Payload, TEXT("uvChannel"), 0);

    // Transform parameters
double TranslateU = GetNumberFieldGeom(Payload, TEXT("translateU"), 0.0);
    double TranslateV = GetNumberFieldGeom(Payload, TEXT("translateV"), 0.0);
    double ScaleU = GetNumberFieldGeom(Payload, TEXT("scaleU"), 1.0);
    double ScaleV = GetNumberFieldGeom(Payload, TEXT("scaleV"), 1.0);
    double Rotation = GetNumberFieldGeom(Payload, TEXT("rotation"), 0.0);

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

    // UE 5.7: TransformMeshUVs was removed, use separate TranslateMeshUVs, ScaleMeshUVs, RotateMeshUVs
    FGeometryScriptMeshSelection Selection; // Empty = apply to entire mesh

    // Apply translation
    if (TranslateU != 0.0 || TranslateV != 0.0)
    {
        UGeometryScriptLibrary_MeshUVFunctions::TranslateMeshUVs(
            Mesh, UVChannel, FVector2D(TranslateU, TranslateV), Selection, nullptr);
    }

    // Apply scale
    if (ScaleU != 1.0 || ScaleV != 1.0)
    {
        UGeometryScriptLibrary_MeshUVFunctions::ScaleMeshUVs(
            Mesh, UVChannel, FVector2D(ScaleU, ScaleV), FVector2D(0.5, 0.5), Selection, nullptr);
    }

    // Apply rotation
    if (Rotation != 0.0)
    {
        UGeometryScriptLibrary_MeshUVFunctions::RotateMeshUVs(
            Mesh, UVChannel, Rotation, FVector2D(0.5, 0.5), Selection, nullptr);
    }

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("uvChannel"), UVChannel);
    Result->SetNumberField(TEXT("translateU"), TranslateU);
    Result->SetNumberField(TEXT("translateV"), TranslateV);
    Result->SetNumberField(TEXT("scaleU"), ScaleU);
    Result->SetNumberField(TEXT("scaleV"), ScaleV);
    Result->SetNumberField(TEXT("rotation"), Rotation);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("UVs transformed"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Boolean Trim Operation
// -------------------------------------------------------------------------
} // namespace McpGeometryHandlers

#endif // WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT
