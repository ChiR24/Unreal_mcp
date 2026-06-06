#include "McpAutomationBridge_GeometryHandlers.h"

#if WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
UDynamicMesh* GetOrCreateDynamicMesh(UObject* Outer)
{
    return NewObject<UDynamicMesh>(Outer);
}

// Helper to spawn DynamicMeshActor and assign mesh - REDUCES CODE DUPLICATION

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
    NewActor->SetActorLabel(Name);

    if (ADynamicMeshActor* DMActor = Cast<ADynamicMeshActor>(NewActor))
    {
        if (UDynamicMeshComponent* DMComp = DMActor->GetDynamicMeshComponent())
        {
            DMComp->SetDynamicMesh(DynMesh);
        }
    }

    return NewActor;
}

ADynamicMeshActor* FindDynamicMeshActorForGeometry(const FString& ActorName)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return nullptr;
    }

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName || It->GetName() == ActorName)
        {
            return *It;
        }
    }

    return nullptr;
}

bool ResolveDynamicMeshForGeometry(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                          const FString& ActorName, TSharedPtr<FMcpBridgeWebSocket> Socket,
                                          ADynamicMeshActor*& OutActor, UDynamicMeshComponent*& OutComponent,
                                          UDynamicMesh*& OutMesh)
{
    OutActor = nullptr;
    OutComponent = nullptr;
    OutMesh = nullptr;

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return false;
    }

    OutActor = FindDynamicMeshActorForGeometry(ActorName);
    if (!OutActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return false;
    }

    OutComponent = OutActor->GetDynamicMeshComponent();
    if (!OutComponent || !OutComponent->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return false;
    }

    OutMesh = OutComponent->GetDynamicMesh();
    return true;
}
} // namespace McpGeometryHandlers

#endif // WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT
