#include "McpAutomationBridge_GeometryHandlers.h"

#if WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleMirror(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                         const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    FString Axis = GetStringFieldGeom(Payload, TEXT("axis"), TEXT("X")).ToUpper();
    bool bWeld = GetBoolFieldGeom(Payload, TEXT("weld"), true);

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

    // Create a copy of the mesh
    UDynamicMesh* MirroredMesh = NewObject<UDynamicMesh>(GetTransientPackage());
    MirroredMesh->SetMesh(Mesh->GetMeshRef());

    // Mirror by scaling with negative value on the axis
    FVector MirrorScale = FVector::OneVector;
    if (Axis == TEXT("X")) MirrorScale.X = -1.0;
    else if (Axis == TEXT("Y")) MirrorScale.Y = -1.0;
    else if (Axis == TEXT("Z")) MirrorScale.Z = -1.0;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
    UGeometryScriptLibrary_MeshTransformFunctions::ScaleMesh(MirroredMesh, MirrorScale, FVector::ZeroVector, true, nullptr);
#else
    // UE 5.3 fallback: Scale mesh using low-level API
    {
        UE::Geometry::FDynamicMesh3& EditMesh = MirroredMesh->GetMeshRef();
        for (int32 VID : EditMesh.VertexIndicesItr())
        {
            FVector3d Pos = EditMesh.GetVertex(VID);
            Pos.X *= MirrorScale.X;
            Pos.Y *= MirrorScale.Y;
            Pos.Z *= MirrorScale.Z;
            EditMesh.SetVertex(VID, Pos);
        }
            // EditMesh.UpdateVertexNormals(); // Not available in UE 5.3
            // MirroredMesh->NotifyMeshUpdated(); // Not available in UE 5.3
    }
#endif

    // Append mirrored mesh to original
    FGeometryScriptAppendMeshOptions AppendOptions;
    UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(Mesh, MirroredMesh, FTransform::Identity, false, AppendOptions, nullptr);

    // Optionally weld vertices at the mirror plane
    if (bWeld)
    {
        FGeometryScriptWeldEdgesOptions WeldOptions;
        WeldOptions.Tolerance = 0.001;
        UGeometryScriptLibrary_MeshRepairFunctions::WeldMeshEdges(Mesh, WeldOptions, nullptr);
    }

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("axis"), Axis);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Mirror applied"), Result);
    return true;
}

bool HandleTranslateMesh(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    FVector Translation = ReadVectorFromPayload(Payload, TEXT("translation"), FVector::ZeroVector);

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

    // Use Geometry Script to translate the mesh
    // UE 5.7+: TranslateMesh is in MeshTransformFunctions
    // UE 5.5+: TranslateMesh is in MeshTransformFunctions
    UGeometryScriptLibrary_MeshTransformFunctions::TranslateMesh(Mesh, Translation, nullptr);
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);

    TSharedPtr<FJsonObject> TransObj = McpHandlerUtils::CreateResultObject();
    TransObj->SetNumberField(TEXT("x"), Translation.X);
    TransObj->SetNumberField(TEXT("y"), Translation.Y);
    TransObj->SetNumberField(TEXT("z"), Translation.Z);
    Result->SetObjectField(TEXT("translation"), TransObj);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Mesh translated"), Result);
    return true;
}

// -------------------------------------------------------------------------
// UV Operations - Unwrap and Pack
// -------------------------------------------------------------------------
} // namespace McpGeometryHandlers

#endif // WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT
