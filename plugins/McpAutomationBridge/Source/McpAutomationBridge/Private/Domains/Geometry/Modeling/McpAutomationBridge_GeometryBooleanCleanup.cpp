#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleBooleanTrim(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                              const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    FString TrimActorName = GetJsonStringField(Payload, TEXT("trimActorName"));
    bool bKeepInside = GetJsonBoolField(Payload, TEXT("keepInside"), false);

    if (ActorName.IsEmpty() || TrimActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName and trimActorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;
    ADynamicMeshActor* TrimActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName) TargetActor = *It;
        if (It->GetActorLabel() == TrimActorName) TrimActor = *It;
    }

    if (!TargetActor || !TrimActor)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("One or both actors not found"), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    UDynamicMeshComponent* TrimDMC = TrimActor->GetDynamicMeshComponent();
    if (!DMC || !TrimDMC || !DMC->GetDynamicMesh() || !TrimDMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available on one or both actors"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();
    UDynamicMesh* TrimMesh = TrimDMC->GetDynamicMesh();

    // Perform boolean subtract as trim (keep inside or outside)
    FTransform TargetTransform = TargetActor->GetActorTransform();
    FTransform TrimTransform = TrimActor->GetActorTransform();

    FGeometryScriptMeshBooleanOptions BoolOptions;
    BoolOptions.bFillHoles = true;

    // If keepInside, intersect; otherwise subtract
    if (bKeepInside)
    {
        UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean(
            Mesh, TargetTransform, TrimMesh, TrimTransform, EGeometryScriptBooleanOperation::Intersection, BoolOptions, nullptr);
    }
    else
    {
        UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean(
            Mesh, TargetTransform, TrimMesh, TrimTransform, EGeometryScriptBooleanOperation::Subtract, BoolOptions, nullptr);
    }

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("trimActorName"), TrimActorName);
    Result->SetBoolField(TEXT("keepInside"), bKeepInside);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Boolean trim applied"), Result);
    return true;
}

bool HandleSelfUnion(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                            const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    bool bFillHoles = GetJsonBoolField(Payload, TEXT("fillHoles"), true);

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
    int32 TrisBefore = Mesh->GetTriangleCount();

    FGeometryScriptMeshSelfUnionOptions SelfUnionOptions;
    SelfUnionOptions.bFillHoles = bFillHoles;
    SelfUnionOptions.bTrimFlaps = true;
    UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshSelfUnion(Mesh, SelfUnionOptions, nullptr);

    int32 TrisAfter = Mesh->GetTriangleCount();
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("trianglesBefore"), TrisBefore);
    Result->SetNumberField(TEXT("trianglesAfter"), TrisAfter);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Self-union applied"), Result);
    return true;
}

} // namespace McpGeometryHandlers

#endif // WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT
