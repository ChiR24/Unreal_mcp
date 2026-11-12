#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/McpCapabilityPrincipal.h"
#include "EngineUtils.h"

#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

AActor* FindActorByNameInWorldForMcp(UWorld* World, const FString& Target, bool bExactMatchOnly)
{
    if (!World || Target.IsEmpty())
    {
        return nullptr;
    }

    TArray<AActor*> FuzzyMatches;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        // A label names an actor in any level; a path-restricted caller finds only those in a level inside its prefixes.
        if (!Actor || !McpCapabilityPrincipal::ActiveRequestPermitsObject(Actor))
        {
            continue;
        }

        if (Actor->GetActorLabel().Equals(Target, ESearchCase::IgnoreCase) ||
            Actor->GetName().Equals(Target, ESearchCase::IgnoreCase) ||
            Actor->GetPathName().Equals(Target, ESearchCase::IgnoreCase))
        {
            return Actor;
        }

        if (!bExactMatchOnly && Actor->GetActorLabel().Contains(Target, ESearchCase::IgnoreCase))
        {
            FuzzyMatches.Add(Actor);
        }
    }

    return FuzzyMatches.Num() == 1 ? FuzzyMatches[0] : nullptr;
}

namespace McpHandlerUtils
{

UWorld* GetEditorWorld()
{
    return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
}

UWorld* GetPlayOrEditorWorld()
{
    return GEditor && GEditor->PlayWorld ? GEditor->PlayWorld.Get() : GetEditorWorld();
}

AActor* FindActorByName(const FString& ActorName)
{
    UWorld* World = GetPlayOrEditorWorld();
    if (AActor* Actor = FindActorByNameInWorldForMcp(World, ActorName, true))
    {
        return Actor;
    }
    // While PIE runs, PlayerPawn, PlayerController, GameMode ... name the running game's actors by role, as control_actor
    // takes them: raycast_world ignoreActors refused PlayerPawn.
    return Cast<AActor>(ResolveRuntimeRole(ActorName));
}

void AddMeshAssetFields(const UActorComponent* Component, const TSharedPtr<FJsonObject>& Entry)
{
    if (const UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Component))
    {
        Entry->SetStringField(TEXT("staticMesh"), Mesh->GetStaticMesh() ? Mesh->GetStaticMesh()->GetPathName() : TEXT(""));
    }
    if (const UMeshComponent* MeshComponent = Cast<UMeshComponent>(Component))
    {
        TArray<TSharedPtr<FJsonValue>> Materials;
        for (UMaterialInterface* Material : MeshComponent->GetMaterials())
        {
            Materials.Add(MakeShared<FJsonValueString>(Material ? Material->GetPathName() : TEXT("")));
        }
        Entry->SetArrayField(TEXT("materials"), Materials);
    }
}
}
