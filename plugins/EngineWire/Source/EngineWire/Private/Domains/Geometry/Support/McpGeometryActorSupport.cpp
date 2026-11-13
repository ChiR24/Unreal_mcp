#include "Domains/Geometry/McpGeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

#include "Foundation/McpScopedEditorTransaction.h"

namespace McpGeometryHandlers
{
AActor* SpawnPrimitiveOrReply(UEngineWireSubsystem* Self, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket,
                              const TSharedPtr<FJsonObject>& Payload, const TCHAR* DefaultName, UDynamicMesh* DynMesh,
                              TSharedPtr<FJsonObject>& OutResult)
{
    const FTransform Transform = ReadTransformFromPayload(Payload);
    const FString GivenName = GetJsonStringField(Payload, TEXT("name"));
    const FString Name = GivenName.IsEmpty() ? FString(DefaultName) : GivenName;
    // One undo step: control_editor undo takes the new actor back out of the level.
    const FMcpScopedEditorTransaction Transaction(FText::Format(NSLOCTEXT("McpGeometry", "CreatePrimitive", "Create {0}"), FText::FromString(Name)),
                                                  EMcpMutationDurability::EditorStateOnly, TArray<UObject*>());
    AActor* NewActor = SpawnDynamicMeshActorWithMesh(Transform, Name, DynMesh);
    if (!NewActor)
    {
        DynMesh->MarkAsGarbage();
        Self->SendAutomationError(Socket, RequestId, TEXT("Failed to spawn DynamicMeshActor"), TEXT("SPAWN_FAILED"));
        return nullptr;
    }
    OutResult = McpHandlerUtils::CreateResultObject();
    Transaction.DescribeInto(OutResult);
    // A label another actor already carries stays ambiguous: the reply names this actor by its unique object name,
    // and says why, so a later call by the label does not reach the older actor.
    const FString Ref = McpActorRef(NewActor);
    OutResult->SetStringField(TEXT("name"), Ref);
    if (!Name.IsEmpty() && !Ref.Equals(Name, ESearchCase::IgnoreCase))
    {
        OutResult->SetArrayField(TEXT("warnings"), TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueString>(FString::Printf(
            TEXT("Another actor is already labelled '%s'; name this one '%s' in later calls."), *Name, *Ref))});
    }
    OutResult->SetStringField(TEXT("class"), TEXT("DynamicMeshActor"));
    return NewActor;
}

AActor* SpawnDynamicMeshActorWithMesh(
    const FTransform& Transform,
    const FString& Name,
    UDynamicMesh* DynMesh)
{
    ADynamicMeshActor* NewActor = SpawnActorInActiveWorld<ADynamicMeshActor>(
        ADynamicMeshActor::StaticClass(), Transform.GetLocation(), Transform.Rotator(), Name);
    if (!NewActor)
    {
        return nullptr;
    }
    // Apply the FULL requested transform. Location+rotation were set above but
    // scale was silently dropped, so a scaled primitive request produced a
    // unit-scale actor with no warning. With meshes now built in local space
    // (see the primitive handlers), the actor transform is the single source
    // of placement, rotation AND scale.
    NewActor->SetActorScale3D(Transform.GetScale3D());

    if (UDynamicMeshComponent* DMComp = NewActor->GetDynamicMeshComponent())
    {
        DMComp->SetDynamicMesh(DynMesh);
        // ADynamicMeshActor's template component is created inactive, so a
        // freshly spawned primitive rendered nothing and carried no physics
        // body until the caller manually activated it (pawns fell straight
        // through generated geometry in PIE). Activate + register so the
        // mesh renders and collides immediately, matching a mesh the editor
        // user would see after the ToolIsActive workflow.
        if (!DMComp->IsRegistered())
        {
            DMComp->RegisterComponent();
        }
        if (!DMComp->IsActive())
        {
            DMComp->SetActive(true);
        }
        DMComp->SetMobility(EComponentMobility::Movable);
        DMComp->MarkRenderStateDirty();
    }

    return NewActor;
}

TOptional<FMcpGeometryTarget> ResolveGeometryTarget(UEngineWireSubsystem* Self, const FString& RequestId,
                                                     const FString& ActorName, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return {};
    }
    FMcpGeometryTarget Target;
    Target.Actor = FindActorOfClassForMcp<ADynamicMeshActor>(McpHandlerUtils::GetEditorWorld(), ActorName);
    if (!Target.Actor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return {};
    }
    Target.Component = Target.Actor->GetDynamicMeshComponent();
    Target.Mesh = Target.Component ? Target.Component->GetDynamicMesh() : nullptr;
    if (!Target.Mesh)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return {};
    }
    return Target;
}

USplineComponent* ResolveGeometrySpline(UEngineWireSubsystem* Self, const FString& RequestId,
                                        TSharedPtr<FMcpBridgeWebSocket> Socket, const FString& SplineActorName)
{
    AActor* SplineActor = FindActorOfClassForMcp<AActor>(McpHandlerUtils::GetEditorWorld(), SplineActorName);
    if (!SplineActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Spline actor not found: %s"), *SplineActorName), TEXT("SPLINE_NOT_FOUND"));
        return nullptr;
    }
    USplineComponent* Spline = SplineActor->FindComponentByClass<USplineComponent>();
    if (!Spline)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("%s has no spline component"), *SplineActorName), TEXT("SPLINE_COMPONENT_NOT_FOUND"));
    }
    return Spline;
}

} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
