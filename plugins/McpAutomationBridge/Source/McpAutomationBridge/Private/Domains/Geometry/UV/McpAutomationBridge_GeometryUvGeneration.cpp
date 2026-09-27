#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleAutoUV(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                         const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));

    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;
    auto [TargetActor, DMC, Mesh] = *Target;
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
    int32 UVChannel = GetJsonIntField(Payload, TEXT("uvChannel"), 0);

    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;
    auto [TargetActor, DMC, Mesh] = *Target;

    UGeometryScriptLibrary_MeshUVFunctions::SetMeshUVsFromBoxProjection(
        Mesh, UVChannel, FTransform::Identity, FGeometryScriptMeshSelection(), 2, nullptr);
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("uvChannel"), UVChannel);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("UV projection applied"), Result);
    return true;
}

bool HandleTransformUVs(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                               const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    int32 UVChannel = GetJsonIntField(Payload, TEXT("uvChannel"), 0);

    // uvOffset / uvScale are {u, v} objects.
    auto ReadUV = [&Payload](const TCHAR* Field, double Default)
    {
        const TSharedPtr<FJsonObject>* Obj = nullptr;
        return Payload->TryGetObjectField(Field, Obj)
            ? FVector2D(GetJsonNumberField(*Obj, TEXT("u"), Default), GetJsonNumberField(*Obj, TEXT("v"), Default))
            : FVector2D(Default, Default);
    };
    const FVector2D Translate = ReadUV(TEXT("uvOffset"), 0.0);
    const FVector2D Scale = ReadUV(TEXT("uvScale"), 1.0);
    const double Rotation = GetJsonNumberField(Payload, TEXT("rotation"), 0.0);

    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;
    auto [TargetActor, DMC, Mesh] = *Target;

    // UE 5.7: TransformMeshUVs was removed, use separate TranslateMeshUVs, ScaleMeshUVs, RotateMeshUVs
    FGeometryScriptMeshSelection Selection; // Empty = apply to entire mesh

    if (!Translate.IsZero())
    {
        UGeometryScriptLibrary_MeshUVFunctions::TranslateMeshUVs(Mesh, UVChannel, Translate, Selection, nullptr);
    }
    if (Scale != FVector2D(1.0, 1.0))
    {
        UGeometryScriptLibrary_MeshUVFunctions::ScaleMeshUVs(Mesh, UVChannel, Scale, FVector2D(0.5, 0.5), Selection, nullptr);
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
    Result->SetNumberField(TEXT("translateU"), Translate.X);
    Result->SetNumberField(TEXT("translateV"), Translate.Y);
    Result->SetNumberField(TEXT("scaleU"), Scale.X);
    Result->SetNumberField(TEXT("scaleV"), Scale.Y);
    Result->SetNumberField(TEXT("rotation"), Rotation);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("UVs transformed"), Result);
    return true;
}

} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
