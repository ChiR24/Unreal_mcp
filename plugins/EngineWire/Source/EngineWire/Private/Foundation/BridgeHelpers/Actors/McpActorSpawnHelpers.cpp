#include "Foundation/BridgeHelpers/Actors/McpActorSpawnHelpers.h"

#include "EngineUtils.h"
#include "Foundation/McpCapabilityPrincipal.h"

AActor *McpFindActorByNameOrClass(UClass *ActorClass, const FString &ActorName)
{
    UWorld *World = McpHandlerUtils::GetEditorWorld();
    if (!World)
    {
        return nullptr;
    }

    AActor *FirstClassMatch = nullptr;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor *Actor = *It;
        // A path-restricted caller finds only actors in a level inside its prefixes.
        if (!Actor || (ActorClass && !Actor->IsA(ActorClass)) || !McpCapabilityPrincipal::ActiveRequestPermitsObject(Actor))
        {
            continue;
        }

        if (!FirstClassMatch)
        {
            FirstClassMatch = Actor;
        }

        if (!ActorName.IsEmpty() &&
            (Actor->GetActorLabel().Equals(ActorName, ESearchCase::IgnoreCase) ||
             Actor->GetName().Equals(ActorName, ESearchCase::IgnoreCase)))
        {
            return Actor;
        }
    }

    return ActorName.IsEmpty() ? FirstClassMatch : nullptr;
}

AActor *McpFindOrSpawnActor(UClass *ActorClass, const FString &ActorName, const FVector &Location,
                            const FRotator &Rotation, bool *bOutSpawned)
{
    if (bOutSpawned)
    {
        *bOutSpawned = false;
    }
    if (!ActorClass)
    {
        return nullptr;
    }

    if (AActor *Existing = McpFindActorByNameOrClass(ActorClass, ActorName))
    {
        return Existing;
    }

    const FString Label = ActorName.IsEmpty() ? ActorClass->GetName() : ActorName;
    AActor *Spawned = SpawnActorInActiveWorld<AActor>(ActorClass, Location, Rotation, Label);
    if (bOutSpawned)
    {
        *bOutSpawned = Spawned != nullptr;
    }
    return Spawned;
}
