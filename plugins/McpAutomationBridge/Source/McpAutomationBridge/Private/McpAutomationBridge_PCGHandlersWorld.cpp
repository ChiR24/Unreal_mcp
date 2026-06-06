#include "McpAutomationBridge_PCGHandlersPrivate.h"

#if WITH_EDITOR && MCP_HAS_PCG
namespace McpPCGHandlers
{
UWorld* GetPCGEditorWorld()
{
    return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
}

AActor* FindPCGActor(UWorld* World, const FString& ActorName)
{
    if (!World || ActorName.IsEmpty())
    {
        return nullptr;
    }

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (Actor && (Actor->GetName().Equals(ActorName, ESearchCase::IgnoreCase) ||
            Actor->GetActorLabel().Equals(ActorName, ESearchCase::IgnoreCase)))
        {
            return Actor;
        }
    }

    return nullptr;
}

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
            Component->GetFName().ToString().Equals(ComponentName, ESearchCase::IgnoreCase) ||
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
        OutActor = FindPCGActor(World, ActorName);
        return FindPCGComponentOnActor(OutActor, ComponentName);
    }

    if (ComponentName.IsEmpty())
    {
        return nullptr;
    }

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (UPCGComponent* Component = FindPCGComponentOnActor(Actor, ComponentName))
        {
            OutActor = Actor;
            return Component;
        }
    }

    return nullptr;
}

bool HasPCGComponentSelector(const FString& ActorName, const FString& ComponentName)
{
    return !ActorName.IsEmpty() || !ComponentName.IsEmpty();
}

UPCGComponent* CreatePCGComponent(AActor* Actor, const FString& ComponentName)
{
    if (!Actor)
    {
        return nullptr;
    }

    Actor->Modify();
    const FName NewComponentName = ComponentName.IsEmpty() ? NAME_None : FName(*ComponentName);
    UPCGComponent* Component = NewObject<UPCGComponent>(Actor, NewComponentName, RF_Transactional);
    if (!Component)
    {
        return nullptr;
    }

    Actor->AddInstanceComponent(Component);
    Component->RegisterComponent();
    Component->Modify();
    return Component;
}

bool SaveEditorWorldIfRequested(UWorld* World, bool bSave, bool& bOutSaved, FString& OutError)
{
    bOutSaved = false;
    if (!World)
    {
        OutError = TEXT("Could not resolve the editor world for level save.");
        return false;
    }

    if (!bSave)
    {
        return true;
    }

    if (!World->PersistentLevel)
    {
        OutError = TEXT("Could not resolve the persistent level for PCG save.");
        return false;
    }

    UPackage* WorldPackage = World->GetOutermost();
    const FString LevelPath = WorldPackage ? WorldPackage->GetName() : FString();
    if (LevelPath.IsEmpty())
    {
        OutError = TEXT("Could not resolve the current level package path for PCG save.");
        return false;
    }

    World->Modify();
    World->MarkPackageDirty();
    World->PersistentLevel->Modify();
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
