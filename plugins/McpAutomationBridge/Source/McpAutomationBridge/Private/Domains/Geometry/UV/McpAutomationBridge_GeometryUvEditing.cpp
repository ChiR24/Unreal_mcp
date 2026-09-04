#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleSetUVs(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                         const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    int32 VertexIndex = GetJsonIntField(Payload, TEXT("vertexIndex"), -1);
    double U = GetJsonNumberField(Payload, TEXT("u"), 0.0);
    double V = GetJsonNumberField(Payload, TEXT("v"), 0.0);
    int32 UVChannel = GetJsonIntField(Payload, TEXT("uvChannel"), 0);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("No world available"), TEXT("NO_WORLD"));
        return true;
    }

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
    UE::Geometry::FDynamicMesh3& EditMesh = Mesh->GetMeshRef();

    UE::Geometry::FDynamicMeshAttributeSet* Attributes = EditMesh.Attributes();
    if (!Attributes)
    {
        EditMesh.EnableAttributes();
        Attributes = EditMesh.Attributes();
    }

    if (UVChannel >= Attributes->NumUVLayers())
    {
        for (int32 i = Attributes->NumUVLayers(); i <= UVChannel; ++i)
        {
            Attributes->SetNumUVLayers(i + 1);
        }
    }

    UE::Geometry::FDynamicMeshUVOverlay* UVOverlay = Attributes->GetUVLayer(UVChannel);
    if (!UVOverlay)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("Failed to access UV layer"), TEXT("UV_LAYER_ERROR"));
        return true;
    }

    FVector2f UVValue(static_cast<float>(U), static_cast<float>(V));
    int32 ElementsModified = 0;
    int32 ElementsCreated = 0;

    if (VertexIndex < 0 || !EditMesh.IsVertex(VertexIndex))
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Invalid vertex index: %d (the dynamic mesh has %d vertices)"), VertexIndex, EditMesh.VertexCount()), TEXT("INVALID_VERTEX"));
        return true;
    }
    for (int32 ElementID : UVOverlay->ElementIndicesItr())
    {
        if (UVOverlay->GetParentVertex(ElementID) == VertexIndex)
        {
            UVOverlay->SetElement(ElementID, UVValue);
            ElementsModified++;
        }
    }
    if (ElementsModified == 0)
    {
        // A vertex whose triangles have no UV elements yet (fresh or non-compact mesh, a
        // layer XAtlas skipped) gets them authored here instead of NO_UV_ELEMENTS
        // (dogfood #133): one element per unset corner, reusing another triangle's
        // element for the same vertex so no seam is introduced.
        auto FindElementForVertex = [UVOverlay](int32 VertexID) -> int32
        {
            for (int32 ElementID : UVOverlay->ElementIndicesItr())
            {
                if (UVOverlay->GetParentVertex(ElementID) == VertexID) return ElementID;
            }
            return UE::Geometry::FDynamicMesh3::InvalidID;
        };
        for (int32 TriangleID : EditMesh.VtxTrianglesItr(VertexIndex))
        {
            if (UVOverlay->IsSetTriangle(TriangleID))
            {
                continue;
            }
            const UE::Geometry::FIndex3i Triangle = EditMesh.GetTriangle(TriangleID);
            UE::Geometry::FIndex3i Elements(UE::Geometry::FDynamicMesh3::InvalidID, UE::Geometry::FDynamicMesh3::InvalidID, UE::Geometry::FDynamicMesh3::InvalidID);
            for (int32 Corner = 0; Corner < 3; ++Corner)
            {
                const int32 CornerVertex = Triangle[Corner];
                Elements[Corner] = FindElementForVertex(CornerVertex);
                if (Elements[Corner] == UE::Geometry::FDynamicMesh3::InvalidID)
                {
                    Elements[Corner] = UVOverlay->AppendElement(CornerVertex == VertexIndex ? UVValue : FVector2f::ZeroVector);
                    UVOverlay->SetParentVertex(Elements[Corner], CornerVertex);
                    ElementsCreated++;
                }
                else if (CornerVertex == VertexIndex)
                {
                    UVOverlay->SetElement(Elements[Corner], UVValue);
                    ElementsModified++;
                }
            }
            UVOverlay->SetTriangle(TriangleID, Elements);
        }
        if (ElementsCreated == 0 && ElementsModified == 0)
        {
            Self->SendAutomationError(Socket, RequestId,
                FString::Printf(TEXT("Vertex %d is not referenced by any triangle, so it has no UV corner to set"), VertexIndex), TEXT("NO_UV_ELEMENTS"));
            return true;
        }
    }

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("vertexIndex"), VertexIndex);
    Result->SetNumberField(TEXT("u"), U);
    Result->SetNumberField(TEXT("v"), V);
    Result->SetNumberField(TEXT("uvChannel"), UVChannel);
    Result->SetNumberField(TEXT("elementsModified"), ElementsModified);
    Result->SetNumberField(TEXT("elementsCreated"), ElementsCreated);
    Result->SetNumberField(TEXT("uvElementCount"), UVOverlay->ElementCount());
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("UV coordinates set"), Result);
    return true;
}

bool HandleUnwrapUV(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                           const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
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

    FGeometryScriptXAtlasOptions XAtlasOptions;
    // XAtlas defaults are reasonable for most cases

    UGeometryScriptLibrary_MeshUVFunctions::AutoGenerateXAtlasMeshUVs(
        Mesh,
        UVChannel,
        XAtlasOptions,
        nullptr
    );

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("uvChannel"), UVChannel);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("UV unwrapping completed"), Result);
    return true;
}

bool HandlePackUVIslands(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    int32 UVChannel = GetJsonIntField(Payload, TEXT("uvChannel"), 0);
    int32 TextureResolution = GetJsonIntField(Payload, TEXT("textureResolution"), 1024);

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

    // Use XAtlas with packing - it handles both unwrapping and packing
    FGeometryScriptXAtlasOptions XAtlasOptions;
    // XAtlas will pack islands efficiently by default

    UGeometryScriptLibrary_MeshUVFunctions::AutoGenerateXAtlasMeshUVs(
        Mesh,
        UVChannel,
        XAtlasOptions,
        nullptr
    );

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("uvChannel"), UVChannel);
    Result->SetNumberField(TEXT("textureResolution"), TextureResolution);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("UV islands packed"), Result);
    return true;
}

} // namespace McpGeometryHandlers

#endif // WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT
