#include "Domains/PCG/Execution/McpPCGManagedInventory.h"
#if MCP_HAS_PCG_EXECUTION
#include "PCGManagedResource.h"
#include "Components/ChildActorComponent.h"

namespace McpPCGHandlers
{
bool ReadBoundedPCGManagedInventory(UPCGComponent* Component, FPCGReadBudget& Budget, FPCGManagedInventory& Out)
{
    if (!Component || !Component->AreManagedResourcesAccessible()) return false;
    int32 Visits = 0;
    bool bComplete = true;
    const auto Visit = [&]() { return ++Visits <= FPCGReadBudget::MaxObjects && Budget.IsWithinTime(); };
    // Native resource enumeration has no breakable public overload. Once refused,
    // remaining callbacks do no work; a single native call cannot be preempted.
    Component->ForEachConstManagedResource([&](const UPCGManagedResource* Resource)
    {
        if (!bComplete) return;
        if (!Visit()) { bComplete = false; return; }
        ++Out.ResourceCount;
        if (const auto* Managed = Cast<UPCGManagedComponent>(Resource))
        {
            if (UActorComponent* Value = Managed->GeneratedComponent.Get()) Out.Components.Add(Value);
            else if (!Managed->GeneratedComponent.IsNull()) ++Out.Unresolved;
        }
        if (const auto* Managed = Cast<UPCGManagedComponentList>(Resource))
        {
            if (Managed->GeneratedComponents.Num() > FPCGReadBudget::MaxObjects - Visits) { bComplete = false; return; }
            for (const auto& Reference : Managed->GeneratedComponents)
            {
                if (!Visit()) { bComplete = false; return; }
                if (UActorComponent* Value = Reference.Get()) Out.Components.Add(Value);
                else if (!Reference.IsNull()) ++Out.Unresolved;
            }
        }
        if (const auto* Managed = Cast<UPCGManagedActors>(Resource))
        {
            const auto& References = Managed->GetConstGeneratedActors();
            if (References.Num() > FPCGReadBudget::MaxObjects - Visits) { bComplete = false; return; }
            for (const auto& Reference : References)
            {
                if (!Visit()) { bComplete = false; return; }
                if (AActor* Value = Reference.Get()) Out.RootActors.Add(Value);
                else if (!Reference.IsNull()) ++Out.Unresolved;
            }
        }
    });
    if (!bComplete) return false;
    TArray<AActor*> Pending = Out.RootActors.Array();
    for (int32 Index = 0; Index < Pending.Num(); ++Index)
    {
        if (!Visit()) return false;
        AActor* Actor = Pending[Index];
        if (Out.Actors.Contains(Actor)) continue;
        Out.Actors.Add(Actor);
        const auto& Owned = Actor->GetComponents();
        if (Owned.Num() > FPCGReadBudget::MaxObjects - Visits) return false;
        for (UActorComponent* Value : Owned)
        {
            if (!Visit()) return false;
            if (!Value) continue;
            Out.Components.Add(Value);
            if (const auto* Child = Cast<UChildActorComponent>(Value))
                if (AActor* ChildActor = Child->GetChildActor()) Pending.Add(ChildActor);
        }
    }
    return Budget.IsWithinTime();
}
}
#endif
