#pragma once

#include "CoreMinimal.h"

class UActorComponent;
class AActor;

// Shared render-state invalidation after successful render-affecting mutations
// (BB-061 / BB-063). Invoked ONLY after a mutation that actually changed a
// render-affecting property (applied-count > 0 and the handler is returning
// success). Never for reads, refusals, no-ops, or non-render properties. Scope
// is the named BB-061/063 call-site families; pre-existing ad-hoc
// MarkRenderStateDirty call sites outside those families are out of scope.
namespace McpRenderRefresh
{
#if WITH_EDITOR
    void McpRefreshRenderState(UActorComponent* Component);
    void McpRefreshActorRenderState(AActor* Actor);
#endif
}
