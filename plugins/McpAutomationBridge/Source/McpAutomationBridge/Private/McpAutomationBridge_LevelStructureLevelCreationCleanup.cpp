#include "McpAutomationBridge_LevelStructureActions.h"

#include "Components/ActorComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "RenderingThread.h"
#include "UObject/Package.h"

#if WITH_EDITOR
namespace McpLevelStructure
{

void CleanupCreatedLevelWorldAfterSave(UWorld* NewWorld, UPackage* Package, const FString& FullPath)
{
    UE_LOG(LogMcpLevelStructureHandlers, Log, TEXT("HandleCreateLevel: Cleaning up created world from memory after save: %s"), *FullPath);

    // STEP 1: Call CleanupWorld() if the world was initialized
    // This is CRITICAL for UE 5.7 - without this, HasEverBeenInitialized() remains true
    // and the world can't be reused during LoadMap, causing "World Memory Leaks" crash.
    // See World.cpp BeginDestroy() for reference.
    // Note: Using bIsWorldInitialized directly for UE 5.0 compatibility (IsInitialized() added in 5.1)
    if (NewWorld->bIsWorldInitialized)
    {
        UE_LOG(LogMcpLevelStructureHandlers, Log, TEXT("HandleCreateLevel: Calling CleanupWorld() for initialized world"));
        NewWorld->CleanupWorld();
    }

    // STEP 2: Mark the world for destruction
    NewWorld->bIsTearingDown = true;

    // STEP 3: Disable all ticking on this world to prevent tick assertions
    if (NewWorld->PersistentLevel)
    {
        // Mark level as invisible
        NewWorld->PersistentLevel->bIsVisible = false;

        for (AActor* Actor : NewWorld->PersistentLevel->Actors)
        {
            if (Actor)
            {
                if (Actor->PrimaryActorTick.IsTickFunctionRegistered())
                {
                    Actor->PrimaryActorTick.UnRegisterTickFunction();
                }
                Actor->PrimaryActorTick.GetPrerequisites().Empty();

                for (UActorComponent* Component : Actor->GetComponents())
                {
                    if (Component && Component->PrimaryComponentTick.IsTickFunctionRegistered())
                    {
                        Component->PrimaryComponentTick.UnRegisterTickFunction();
                    }
                }
            }
        }
    }

    // STEP 4: Remove from root if the world was added to root
    // The "(root)" flag in error messages indicates RF_RootSet - must clear this
    if (NewWorld->IsRooted())
    {
        UE_LOG(LogMcpLevelStructureHandlers, Log, TEXT("HandleCreateLevel: Removing world from root"));
        NewWorld->RemoveFromRoot();
    }

    // STEP 5: Mark the world and its package as transient so GC will collect them
    NewWorld->SetFlags(RF_Transient);
    if (Package)
    {
        // Also remove package from root if needed
        if (Package->IsRooted())
        {
            Package->RemoveFromRoot();
        }
        Package->SetFlags(RF_Transient);
    }

    // STEP 6: Force garbage collection to remove the world from memory
    // This allows the level to be cleanly loaded later via LoadMap
    CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
    FlushRenderingCommands();

    UE_LOG(LogMcpLevelStructureHandlers, Log, TEXT("HandleCreateLevel: World cleaned up from memory: %s"), *FullPath);
}

}
#endif
