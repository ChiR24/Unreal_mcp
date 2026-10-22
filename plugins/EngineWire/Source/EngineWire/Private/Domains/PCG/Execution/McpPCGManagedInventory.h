#pragma once

#include "Domains/PCG/Execution/McpPCGExecution.h"
#include "Domains/PCG/Discovery/McpPCGBudget.h"

#if MCP_HAS_PCG_EXECUTION
namespace McpPCGHandlers
{
struct FPCGManagedInventory
{
    TSet<UActorComponent*> Components;
    TSet<AActor*> Actors, RootActors;
    int32 ResourceCount = 0, Unresolved = 0;
};
bool ReadBoundedPCGManagedInventory(UPCGComponent* Component, FPCGReadBudget& Budget, FPCGManagedInventory& Out);
}
#endif
