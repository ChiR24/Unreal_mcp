#include "McpAutomationBridge_GeometryHandlers.h"

#if WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleSetUVs(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                         const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    int32 VertexIndex = GetIntFieldGeom(Payload, TEXT("vertexIndex"), -1);
    double U = GetNumberFieldGeom(Payload, TEXT("u"), 0.0);
    double V = GetNumberFieldGeom(Payload, TEXT("v"), 0.0);
    int32 UVChannel = GetIntFieldGeom(Payload, TEXT("uvChannel"), 0);

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

    // Ensure the mesh has UV overlay for the specified channel
    UE::Geometry::FDynamicMeshAttributeSet* Attributes = EditMesh.Attributes();
    if (!Attributes)
    {
        EditMesh.EnableAttributes();
        Attributes = EditMesh.Attributes();
    }

    if (UVChannel >= Attributes->NumUVLayers())
    {
        // Add UV layers up to the requested channel
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

    // Set UV for the vertex (all connected elements)
    FVector2f UVValue(static_cast<float>(U), static_cast<float>(V));
    int32 ElementsModified = 0;

    if (VertexIndex >= 0 && EditMesh.IsVertex(VertexIndex))
    {
        // Get all UV elements for this vertex and set their UVs
        TArray<int32> ElementIDs;
        for (int32 ElementID : UVOverlay->ElementIndicesItr())
        {
            if (UVOverlay->GetParentVertex(ElementID) == VertexIndex)
            {
                UVOverlay->SetElement(ElementID, UVValue);
                ElementsModified++;
            }
        }

        // If no elements exist for this vertex, we need to handle it differently
        if (ElementsModified == 0)
        {
            Self->SendAutomationError(Socket, RequestId,
                FString::Printf(TEXT("No UV elements found for vertex %d"), VertexIndex), TEXT("NO_UV_ELEMENTS"));
            return true;
        }
    }
    else
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Invalid vertex index: %d"), VertexIndex), TEXT("INVALID_VERTEX"));
        return true;
    }

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("vertexIndex"), VertexIndex);
    Result->SetNumberField(TEXT("u"), U);
    Result->SetNumberField(TEXT("v"), V);
    Result->SetNumberField(TEXT("uvChannel"), UVChannel);
    Result->SetNumberField(TEXT("elementsModified"), ElementsModified);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("UV coordinates set"), Result);
    return true;
}

// -------------------------------------------------------------------------
// append_vertex - Add a single vertex to mesh
// -------------------------------------------------------------------------

bool HandleUnwrapUV(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                           const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
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

    // Use XAtlas for proper UV unwrapping
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
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    int32 UVChannel = GetIntFieldGeom(Payload, TEXT("uvChannel"), 0);
    int32 TextureResolution = GetIntFieldGeom(Payload, TEXT("textureResolution"), 1024);

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

// -------------------------------------------------------------------------
// Nanite Conversion
// -------------------------------------------------------------------------
} // namespace McpGeometryHandlers

#endif // WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT
