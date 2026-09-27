#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleBooleanTrim(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                              const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    // A trim is an intersection (keepInside) or a subtract that always keeps the trim actor.
    const TSharedPtr<FJsonObject> BooleanPayload = MakeShared<FJsonObject>(*Payload);
    BooleanPayload->SetStringField(TEXT("targetActor"), GetJsonStringField(Payload, TEXT("actorName")));
    BooleanPayload->SetStringField(TEXT("toolActor"), GetJsonStringField(Payload, TEXT("trimActorName")));
    BooleanPayload->SetBoolField(TEXT("keepTool"), true);
    const bool bKeepInside = GetJsonBoolField(Payload, TEXT("keepInside"), false);
    return HandleBooleanOperation(Self, RequestId, BooleanPayload, Socket,
        bKeepInside ? EGeometryScriptBooleanOperation::Intersection : EGeometryScriptBooleanOperation::Subtract,
        bKeepInside ? TEXT("Intersection") : TEXT("Subtract"));
}

bool HandleSelfUnion(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                            const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));

    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;
    auto [TargetActor, DMC, Mesh] = *Target;
    int32 TrisBefore = Mesh->GetTriangleCount();

    FGeometryScriptMeshSelfUnionOptions SelfUnionOptions;
    SelfUnionOptions.bFillHoles = true;
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

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
