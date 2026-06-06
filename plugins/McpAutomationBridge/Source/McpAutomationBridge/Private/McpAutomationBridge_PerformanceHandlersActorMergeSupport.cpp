#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_PerformanceHandlersPrivate.h"

#if WITH_EDITOR
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

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!Actor)
        {
            continue;
        }

        if (Actor->GetActorLabel().Equals(Name, ESearchCase::IgnoreCase) ||
            Actor->GetName().Equals(Name, ESearchCase::IgnoreCase))
        {
            return Actor;
        }
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
#endif
