#include "Domains/PCG/McpPCGPrivate.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsActionsPaths.h"

#if MCP_HAS_PCG
namespace McpPCGHandlers
{
UPCGComponent* FindPCGComponentOnActor(AActor* Actor, const FString& ComponentName)
{
    if (!Actor)
    {
        return nullptr;
    }

    TArray<UPCGComponent*> Components;
    Actor->GetComponents<UPCGComponent>(Components);
    for (UPCGComponent* Component : Components)
    {
        if (!Component)
        {
            continue;
        }
        const bool bIdentifierLooksLikePath = ComponentName.Contains(TEXT(".")) || ComponentName.Contains(TEXT("/"));
        if (ComponentName.IsEmpty() || Component->GetName().Equals(ComponentName, ESearchCase::IgnoreCase) ||
            Component->GetPathName().Equals(ComponentName, ESearchCase::IgnoreCase) ||
            Component->GetFullName().Equals(ComponentName, ESearchCase::IgnoreCase) ||
            (bIdentifierLooksLikePath && Component->GetPathName().EndsWith(ComponentName, ESearchCase::IgnoreCase)))
        {
            return Component;
        }
    }

    return nullptr;
}

UPCGComponent* FindPCGComponent(UWorld* World, const FString& ActorName, const FString& ComponentName, AActor*& OutActor)
{
    OutActor = nullptr;
    if (!World)
    {
        return nullptr;
    }

    if (!ActorName.IsEmpty())
    {
        OutActor = FindActorByNameInWorldForMcp(World, ActorName, true);
        return FindPCGComponentOnActor(OutActor, ComponentName);
    }

    if (ComponentName.IsEmpty())
    {
        return nullptr;
    }

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        // A path-restricted caller finds only components on actors in a level inside its prefixes.
        if (!McpCapabilityPrincipal::ActiveRequestPermitsObject(Actor))
        {
            continue;
        }
        if (UPCGComponent* Component = FindPCGComponentOnActor(Actor, ComponentName))
        {
            OutActor = Actor;
            return Component;
        }
    }

    return nullptr;
}

UPCGComponent* CreatePCGComponent(AActor* Actor, const FString& ComponentName)
{
    Actor->Modify();
    const FName NewComponentName = ComponentName.IsEmpty() ? NAME_None : FName(*ComponentName);
    UPCGComponent* Component = NewObject<UPCGComponent>(Actor, NewComponentName, RF_Transactional);
    Actor->AddInstanceComponent(Component);
    Component->RegisterComponent();
    Component->Modify();
    return Component;
}

bool SaveEditorWorldIfRequested(UWorld* World, bool bSave, bool& bOutSaved, FString& OutError)
{
    bOutSaved = false;
    if (!bSave)
    {
        return true;
    }
    // Its callers refuse a null world first, and a world has a persistent level and a package name.
    const FString LevelPath = World->GetOutermost()->GetName();
    World->PersistentLevel->MarkPackageDirty();

    bOutSaved = McpSafeLevelSave(World->PersistentLevel, LevelPath);
    if (!bOutSaved)
    {
        OutError = FString::Printf(TEXT("Failed to save current level '%s' after PCG change."), *LevelPath);
        return false;
    }

    return true;
}
}
#endif
