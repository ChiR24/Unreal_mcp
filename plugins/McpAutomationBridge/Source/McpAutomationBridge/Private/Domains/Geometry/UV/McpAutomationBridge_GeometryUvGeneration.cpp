#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleAutoUV(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                         const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));

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
    const int32 UVChannel = FMath::Max(0, GetJsonIntField(Payload, TEXT("uvChannel"), 0));

    // XAtlas silently refuses a non-compact mesh or a missing UV layer (its Debug sink
    // was null), which is how auto_uv reported success while writing nothing (dogfood
    // #133). Compact first, make sure the layer exists, and fall back to a bounds-sized
    // box projection so the channel is never left empty.
    bool bCompacted = false;
    if (!Mesh->GetMeshRef().IsCompact())
    {
        UGeometryScriptLibrary_MeshRepairFunctions::CompactMesh(Mesh, nullptr);
        bCompacted = true;
    }
    {
        UE::Geometry::FDynamicMesh3& EditMesh = Mesh->GetMeshRef();
        if (!EditMesh.HasAttributes())
        {
            EditMesh.EnableAttributes();
        }
        if (EditMesh.Attributes()->NumUVLayers() <= UVChannel)
        {
            EditMesh.Attributes()->SetNumUVLayers(UVChannel + 1);
        }
    }
    auto CountUVElements = [Mesh, UVChannel]() -> int32
    {
        UE::Geometry::FDynamicMesh3& EditMesh = Mesh->GetMeshRef();
        UE::Geometry::FDynamicMeshUVOverlay* Overlay =
            EditMesh.HasAttributes() && UVChannel < EditMesh.Attributes()->NumUVLayers()
                ? EditMesh.Attributes()->GetUVLayer(UVChannel)
                : nullptr;
        return Overlay ? Overlay->ElementCount() : 0;
    };

    UGeometryScriptDebug* Debug = NewObject<UGeometryScriptDebug>();
    // UE 5.7: FGeometryScriptAutoUVOptions was removed, use XAtlas directly
    UGeometryScriptLibrary_MeshUVFunctions::AutoGenerateXAtlasMeshUVs(
        Mesh, UVChannel, FGeometryScriptXAtlasOptions(), Debug);
    FString XAtlasError;
    for (const FGeometryScriptDebugMessage& Message : Debug->Messages)
    {
        if (Message.MessageType == EGeometryScriptDebugMessageType::ErrorMessage)
        {
            XAtlasError = Message.Message.ToString();
            break;
        }
    }
    FString Method = TEXT("xatlas");
    int32 ElementCount = CountUVElements();
    if (!XAtlasError.IsEmpty() || ElementCount == 0)
    {
        const UE::Geometry::FAxisAlignedBox3d Bounds = Mesh->GetMeshRef().GetBounds();
        FVector BoxSize = FVector(Bounds.Max - Bounds.Min);
        BoxSize.X = FMath::Max(BoxSize.X, 1.0);
        BoxSize.Y = FMath::Max(BoxSize.Y, 1.0);
        BoxSize.Z = FMath::Max(BoxSize.Z, 1.0);
        UGeometryScriptLibrary_MeshUVFunctions::SetMeshUVsFromBoxProjection(
            Mesh, UVChannel, FTransform(FQuat::Identity, FVector(Bounds.Center()), BoxSize),
            FGeometryScriptMeshSelection(), 2, nullptr);
        Method = TEXT("box_projection_fallback");
        ElementCount = CountUVElements();
    }

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("uvChannel"), UVChannel);
    Result->SetStringField(TEXT("method"), Method);
    Result->SetNumberField(TEXT("uvElementCount"), ElementCount);
    Result->SetBoolField(TEXT("compacted"), bCompacted);
    if (!XAtlasError.IsEmpty())
    {
        Result->SetStringField(TEXT("xatlasError"), XAtlasError);
    }
    if (ElementCount == 0)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Auto UV produced no UV elements"), Result, TEXT("UV_GENERATION_FAILED"));
        return true;
    }
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Auto UV generated"), Result);
    return true;
}

bool HandleProjectUV(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                            const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
FString ProjectionType = GetJsonStringField(Payload, TEXT("projectionType"), TEXT("box")).ToLower();
    double Scale = GetJsonNumberField(Payload, TEXT("scale"), 1.0);
    int32 UVChannel = GetJsonIntField(Payload, TEXT("uvChannel"), 0);

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
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    int32 UVChannel = GetJsonIntField(Payload, TEXT("uvChannel"), 0);

double TranslateU = GetJsonNumberField(Payload, TEXT("translateU"), 0.0);
    double TranslateV = GetJsonNumberField(Payload, TEXT("translateV"), 0.0);
    double ScaleU = GetJsonNumberField(Payload, TEXT("scaleU"), 1.0);
    double ScaleV = GetJsonNumberField(Payload, TEXT("scaleV"), 1.0);
    double Rotation = GetJsonNumberField(Payload, TEXT("rotation"), 0.0);

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

    if (TranslateU != 0.0 || TranslateV != 0.0)
    {
        UGeometryScriptLibrary_MeshUVFunctions::TranslateMeshUVs(
            Mesh, UVChannel, FVector2D(TranslateU, TranslateV), Selection, nullptr);
    }

    if (ScaleU != 1.0 || ScaleV != 1.0)
    {
        UGeometryScriptLibrary_MeshUVFunctions::ScaleMeshUVs(
            Mesh, UVChannel, FVector2D(ScaleU, ScaleV), FVector2D(0.5, 0.5), Selection, nullptr);
    }

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
