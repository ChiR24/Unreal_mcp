#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
AActor* SpawnPrimitiveOrReply(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket,
                              const FTransform& Transform, const FString& Name, UDynamicMesh* DynMesh, TSharedPtr<FJsonObject>& OutResult)
{
    FString SpawnError;
    AActor* NewActor = SpawnDynamicMeshActorWithMesh(Transform, Name, DynMesh, SpawnError);
    if (!NewActor)
    {
        DynMesh->MarkAsGarbage();
        Self->SendAutomationError(Socket, RequestId, SpawnError.IsEmpty() ? TEXT("Failed to spawn DynamicMeshActor") : SpawnError, TEXT("SPAWN_FAILED"));
        return nullptr;
    }
    OutResult = McpHandlerUtils::CreateResultObject();
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
    UDynamicMesh* DynMesh,
    FString& OutError)
{
    UWorld* World = nullptr;
    if (GEditor && GEditor->PlayWorld)
    {
        World = GEditor->PlayWorld.Get();
    }
    else if (GEditor)
    {
        World = GEditor->GetEditorWorldContext().World();
    }

    if (!World)
    {
        OutError = TEXT("Editor world unavailable");
        return nullptr;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    SpawnParams.ObjectFlags |= RF_Transactional;
    if (!GEditor->PlayWorld)
    {
        SpawnParams.OverrideLevel = World->GetCurrentLevel();
        World->Modify();
    }

    const FVector SpawnLocation = Transform.GetLocation();
    const FRotator SpawnRotation = Transform.Rotator();
    AActor* NewActor = World->SpawnActor(
        ADynamicMeshActor::StaticClass(),
        &SpawnLocation,
        &SpawnRotation,
        SpawnParams);

    if (!NewActor)
    {
        OutError = TEXT("Failed to spawn DynamicMeshActor");
        return nullptr;
    }

    NewActor->Modify();
    NewActor->SetActorLocationAndRotation(SpawnLocation,
                                          SpawnRotation, false, nullptr,
                                          ETeleportType::TeleportPhysics);
    // Apply the FULL requested transform. Location+rotation were set above but
    // scale was silently dropped, so a scaled primitive request produced a
    // unit-scale actor with no warning. With meshes now built in local space
    // (see the primitive handlers), the actor transform is the single source
    // of placement, rotation AND scale.
    NewActor->SetActorScale3D(Transform.GetScale3D());
    NewActor->SetActorLabel(Name);

    if (ADynamicMeshActor* DMActor = Cast<ADynamicMeshActor>(NewActor))
    {
        if (UDynamicMeshComponent* DMComp = DMActor->GetDynamicMeshComponent())
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
    }

    return NewActor;
}

TOptional<FMcpGeometryTarget> ResolveGeometryTarget(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                                     const FString& ActorName, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return {};
    }
    FMcpGeometryTarget Target;
    Target.Actor = FindGeometryActor<ADynamicMeshActor>(ActorName);
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

USplineComponent* ResolveGeometrySpline(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                        TSharedPtr<FMcpBridgeWebSocket> Socket, const FString& SplineActorName)
{
    AActor* SplineActor = FindGeometryActor<AActor>(SplineActorName);
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
