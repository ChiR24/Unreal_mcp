#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsActionsPaths.h"

#include "Domains/Performance/McpAutomationBridge_PerformanceHandlersPrivate.h"

#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"

namespace McpPerformanceHandlers
{
AActor* ResolveMergeActorByName(UWorld* World, const FString& Name)
{
    if (Name.IsEmpty())
    {
        return nullptr;
    }

    if (AActor* ByPath = FindObject<AActor>(nullptr, *Name))
    {
        return ByPath;
    }

    if (AActor* Actor = FindActorByNameInWorldForMcp(World, Name, true))
    {
        return Actor;
    }

    return nullptr;
}

void CollectMergeComponents(
    const TArray<AActor*>& ActorsToMerge,
    TArray<UPrimitiveComponent*>& ComponentsToMerge)
{
    for (AActor* Actor : ActorsToMerge)
    {
        if (!Actor)
        {
            continue;
        }

        TArray<UStaticMeshComponent*> StaticMeshComponents;
        Actor->GetComponents<UStaticMeshComponent>(StaticMeshComponents);
        for (UStaticMeshComponent* Component : StaticMeshComponents)
        {
            if (Component && Component->GetStaticMesh())
            {
                ComponentsToMerge.Add(Component);
            }
        }
    }
}
}
