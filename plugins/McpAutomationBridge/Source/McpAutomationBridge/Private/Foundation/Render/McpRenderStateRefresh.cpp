#include "Foundation/Render/McpRenderStateRefresh.h"

#if WITH_EDITOR
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"

namespace McpRenderRefresh
{
void McpRefreshRenderState(UActorComponent* Component)
{
    if (!Component)
    {
        return;
    }
    Component->MarkRenderStateDirty();
    if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
    {
        Primitive->RecreateRenderState_Concurrent();
    }
}

void McpRefreshActorRenderState(AActor* Actor)
{
    if (!Actor)
    {
        return;
    }
    TInlineComponentArray<UActorComponent*> Components;
    Actor->GetComponents(Components);
    for (UActorComponent* Component : Components)
    {
        McpRefreshRenderState(Component);
    }
}
}
#endif
