#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleBooleanOperation(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                   const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket,
                                   EGeometryScriptBooleanOperation BoolOp, const FString& OpName)
{
    FString TargetActorName = GetJsonStringField(Payload, TEXT("targetActor"));
    FString ToolActorName = GetJsonStringField(Payload, TEXT("toolActor"));
    bool bKeepTool = GetJsonBoolField(Payload, TEXT("keepTool"), true);

    if (TargetActorName.IsEmpty() || ToolActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("targetActor and toolActor required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    ADynamicMeshActor* TargetActor = FindGeometryActor<ADynamicMeshActor>(TargetActorName);
    ADynamicMeshActor* ToolActor = FindGeometryActor<ADynamicMeshActor>(ToolActorName);
    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Target actor not found: %s"), *TargetActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }
    if (!ToolActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Tool actor not found: %s"), *ToolActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* TargetDMC = TargetActor->GetDynamicMeshComponent();
    UDynamicMeshComponent* ToolDMC = ToolActor->GetDynamicMeshComponent();
    UDynamicMesh* TargetMesh = TargetDMC ? TargetDMC->GetDynamicMesh() : nullptr;
    UDynamicMesh* ToolMesh = ToolDMC ? ToolDMC->GetDynamicMesh() : nullptr;
    if (!TargetMesh || !ToolMesh)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    const int32 TargetTriCount = TargetMesh->GetTriangleCount();
    const int32 ToolTriCount = ToolMesh->GetTriangleCount();
    // A boolean can at most combine both meshes, plus the new cut geometry.
    if (!GuardMeshBudget(Self, RequestId, Socket, (static_cast<int64>(TargetTriCount) + ToolTriCount) * 3,
                         *FString::Printf(TEXT("Boolean %s"), *OpName))) return true;

    FGeometryScriptMeshBooleanOptions BoolOptions;
    BoolOptions.bFillHoles = true;
    BoolOptions.bSimplifyOutput = false;

    UGeometryScriptDebug* BoolDebug = NewObject<UGeometryScriptDebug>(GetTransientPackage());
    UDynamicMesh* ResultMesh = UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean(
        TargetMesh,
        TargetActor->GetActorTransform(),
        ToolMesh,
        ToolActor->GetActorTransform(),
        BoolOp,
        BoolOptions,
        BoolDebug
    );

    bool bEmptyResult = false;
    if (BoolDebug)
    {
        for (const FGeometryScriptDebugMessage& DebugMessage : BoolDebug->Messages)
        {
            if (DebugMessage.MessageType == EGeometryScriptDebugMessageType::ErrorMessage &&
                DebugMessage.Message.ToString().Contains(TEXT("empty result"), ESearchCase::IgnoreCase))
            {
                bEmptyResult = true;
                break;
            }
        }
    }
    const int32 ResultTriCount = ResultMesh ? ResultMesh->GetTriangleCount() : 0;
    // An empty result leaves the target unchanged (same count) or empty: both are failures.
    bool bBooleanSucceeded = ResultMesh && !(bEmptyResult && (ResultTriCount == 0 || ResultTriCount == TargetTriCount));
    bool bSubtractNoOp = false;
    bool bSubtractBoundsOverlap = false;
    if (bBooleanSucceeded && !bEmptyResult &&
        BoolOp == EGeometryScriptBooleanOperation::Subtract &&
        ResultTriCount == TargetTriCount &&
        TargetTriCount > 0 && ToolTriCount > 0)
    {
        auto MeshWorldBox = [](UDynamicMesh* Mesh, const FTransform& T, FBox& OutBox)
        {
            UE::Geometry::FAxisAlignedBox3d LocalBox;
            Mesh->ProcessMesh([&LocalBox](const UE::Geometry::FDynamicMesh3& M) { LocalBox = M.GetBounds(); });
            OutBox.Init();
            for (int32 Corner = 0; Corner < 8; ++Corner)
            {
                const FVector3d P(
                    (Corner & 1) ? LocalBox.Max.X : LocalBox.Min.X,
                    (Corner & 2) ? LocalBox.Max.Y : LocalBox.Min.Y,
                    (Corner & 4) ? LocalBox.Max.Z : LocalBox.Min.Z);
                OutBox += T.TransformPosition(P);
            }
        };
        FBox TargetWorldBox, ToolWorldBox;
        MeshWorldBox(TargetMesh, TargetActor->GetActorTransform(), TargetWorldBox);
        MeshWorldBox(ToolMesh, ToolActor->GetActorTransform(), ToolWorldBox);
        bSubtractBoundsOverlap = TargetWorldBox.Intersect(ToolWorldBox);
        const int32 TargetVerts = UGeometryScriptLibrary_MeshQueryFunctions::GetVertexCount(TargetMesh);
        const int32 ResultVerts = ResultMesh ? UGeometryScriptLibrary_MeshQueryFunctions::GetVertexCount(ResultMesh) : -1;
        if (!bSubtractBoundsOverlap || TargetVerts == ResultVerts)
        {
            bSubtractNoOp = true;
            bBooleanSucceeded = false;
        }
    }

    // Keep the tool on failure as well as when keepTool was requested: a failed
    // boolean (empty result / no effect) is almost always a placement problem,
    // and the caller needs the actor to still exist in order to move it. Destroy
    // it only on success when the caller did not ask to keep it.
    if (!bKeepTool && bBooleanSucceeded)
    {
        ToolActor->Destroy();
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("targetActor"), TargetActorName);
    Result->SetStringField(TEXT("operation"), OpName);
    Result->SetBoolField(TEXT("success"), bBooleanSucceeded);
    Result->SetNumberField(TEXT("targetTriangles"), TargetTriCount);
    Result->SetNumberField(TEXT("toolTriangles"), ToolTriCount);
    Result->SetBoolField(TEXT("toolKept"), bKeepTool || !bBooleanSucceeded);
    if (bBooleanSucceeded)
    {
        Result->SetNumberField(TEXT("resultTriangles"), ResultTriCount);
    }

    if (!bBooleanSucceeded && bEmptyResult)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(
                TEXT("Boolean %s produced an empty result — the meshes do not overlap in a way this operation keeps, so the target mesh was left unchanged (%d triangles). Reposition the tool actor (it was kept) so the volumes intersect."),
                *OpName, TargetTriCount),
            Result, TEXT("EMPTY_RESULT"));
        return true;
    }

    if (!bBooleanSucceeded && bSubtractNoOp)
    {
        // Two literal formats (the checked-format string is consteval and cannot
        // take a runtime-selected format pointer): overlapping bounds means the
        // subtract was inert, disjoint bounds means the volumes never met.
        const FString NoEffectMessage = bSubtractBoundsOverlap
            ? FString::Printf(
                  TEXT("Boolean Subtract had no effect — the target mesh is unchanged at %d triangles even though the tool bounds overlap it. The subtract removed nothing (e.g. a tool entirely inside solid geometry with no surface crossing, or a failed intersection). Reposition or resize the tool actor so its surface crosses the target's surface. The tool actor was kept so you can move it."),
                  TargetTriCount)
            : FString::Printf(
                  TEXT("Boolean Subtract had no effect — the tool volume does not overlap the target mesh (%d triangles, unchanged). Move the tool actor so the volumes intersect (the tool actor was kept), or choose a different operation."),
                  TargetTriCount);
        Self->SendAutomationResponse(Socket, RequestId, false,
            NoEffectMessage, Result, TEXT("NO_EFFECT"));
        return true;
    }

    Self->SendAutomationResponse(Socket, RequestId, bBooleanSucceeded,
        bBooleanSucceeded ? FString::Printf(TEXT("Boolean %s completed"), *OpName) : FString::Printf(TEXT("Boolean %s failed - operation produced empty geometry"), *OpName),
        Result);
    return true;
}

bool HandleBooleanUnion(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                               const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return HandleBooleanOperation(Self, RequestId, Payload, Socket, EGeometryScriptBooleanOperation::Union, TEXT("Union"));
}

bool HandleBooleanSubtract(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                  const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return HandleBooleanOperation(Self, RequestId, Payload, Socket, EGeometryScriptBooleanOperation::Subtract, TEXT("Subtract"));
}

bool HandleBooleanIntersection(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                      const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return HandleBooleanOperation(Self, RequestId, Payload, Socket, EGeometryScriptBooleanOperation::Intersection, TEXT("Intersection"));
}

} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
