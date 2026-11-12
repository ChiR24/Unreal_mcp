#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsActionsPaths.h"

#include "Domains/Performance/McpPerfPrivate.h"

#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"

namespace McpPerformanceHandlers
{
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
