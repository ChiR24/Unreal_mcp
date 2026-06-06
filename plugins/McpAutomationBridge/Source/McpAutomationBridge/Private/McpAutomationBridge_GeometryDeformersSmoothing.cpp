#include "McpAutomationBridge_GeometryHandlers.h"

#if WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleSmooth(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                         const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
int32 Iterations = GetIntFieldGeom(Payload, TEXT("iterations"), 10);
    double Alpha = GetNumberFieldGeom(Payload, TEXT("alpha"), 0.2);

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

    FGeometryScriptIterativeMeshSmoothingOptions SmoothOptions;
    SmoothOptions.NumIterations = Iterations;
    SmoothOptions.Alpha = Alpha;

    FGeometryScriptMeshSelection Selection;

    UGeometryScriptLibrary_MeshDeformFunctions::ApplyIterativeSmoothingToMesh(
        Mesh, Selection, SmoothOptions, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("iterations"), Iterations);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Smooth applied"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Mesh Repair
// -------------------------------------------------------------------------

bool HandleRelax(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                        const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    int32 Iterations = GetIntFieldGeom(Payload, TEXT("iterations"), 3);
    double Strength = GetNumberFieldGeom(Payload, TEXT("strength"), 0.5);

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

    // Relax is essentially Laplacian smoothing with lower strength
    FGeometryScriptIterativeMeshSmoothingOptions SmoothOptions;
    SmoothOptions.NumIterations = Iterations;
    SmoothOptions.Alpha = Strength;
    UGeometryScriptLibrary_MeshDeformFunctions::ApplyIterativeSmoothingToMesh(Mesh, FGeometryScriptMeshSelection(), SmoothOptions, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("iterations"), Iterations);
    Result->SetNumberField(TEXT("strength"), Strength);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Relax applied"), Result);
    return true;
}

// -------------------------------------------------------------------------
// UV Operations (Project UV)
// -------------------------------------------------------------------------

bool HandleStretch(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    FString Axis = GetStringFieldGeom(Payload, TEXT("axis"), TEXT("Z")).ToUpper();
    double Factor = GetNumberFieldGeom(Payload, TEXT("factor"), 1.5);

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

    // Stretch by non-uniform scaling
    FVector ScaleVec = FVector::OneVector;
    if (Axis == TEXT("X")) ScaleVec.X = Factor;
    else if (Axis == TEXT("Y")) ScaleVec.Y = Factor;
    else ScaleVec.Z = Factor;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
    UGeometryScriptLibrary_MeshTransformFunctions::ScaleMesh(Mesh, ScaleVec, FVector::ZeroVector, true, nullptr);
#else
    // UE 5.3 fallback: Scale mesh using low-level API
    {
        UE::Geometry::FDynamicMesh3& EditMesh = Mesh->GetMeshRef();
        for (int32 VID : EditMesh.VertexIndicesItr())
        {
            FVector3d Pos = EditMesh.GetVertex(VID);
            Pos.X *= ScaleVec.X;
            Pos.Y *= ScaleVec.Y;
            Pos.Z *= ScaleVec.Z;
            EditMesh.SetVertex(VID, Pos);
        }
            // EditMesh.UpdateVertexNormals(); // Not available in UE 5.3
            // Mesh->NotifyMeshUpdated(); // Not available in UE 5.3
    }
#endif

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("axis"), Axis);
    Result->SetNumberField(TEXT("factor"), Factor);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Stretch applied"), Result);
    return true;
}
} // namespace McpGeometryHandlers

#endif // WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT
