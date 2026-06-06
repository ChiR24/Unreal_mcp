#include "McpAutomationBridge_LevelStructureActions.h"
#include "McpAutomationBridge_LevelStructureEditorWorld.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/ActorComponent.h"
#include "EditorLevelUtils.h"
#include "Engine/LevelStreaming.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "McpHandlerUtils.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"

#if WITH_EDITOR
namespace McpLevelStructure
{

bool HandleCreateSublevel(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace LevelStructureHelpers;

    // CRITICAL: sublevelName is required - no default fallback to prevent hidden errors
    FString SublevelName;
    if (Payload.IsValid())
    {
        Payload->TryGetStringField(TEXT("sublevelName"), SublevelName);
    }

    if (SublevelName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("sublevelName is required for create_sublevel"), nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString SublevelPath = GetJsonStringField(Payload, TEXT("sublevelPath"), TEXT(""));
    FString ParentLevel = GetJsonStringField(Payload, TEXT("parentLevel"), TEXT(""));
    bool bSave = GetJsonBoolField(Payload, TEXT("save"), true);

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr, TEXT("NO_EDITOR_WORLD"));
        return true;
    }

    // Validate parentLevel if specified
    if (!ParentLevel.IsEmpty())
    {
        // Normalize the parent level path
        FString NormalizedParentPath = ParentLevel;
        if (!NormalizedParentPath.StartsWith(TEXT("/Game/")))
        {
            NormalizedParentPath = TEXT("/Game/") + NormalizedParentPath;
        }
        // Remove .umap extension if present
        NormalizedParentPath.RemoveFromEnd(TEXT(".umap"));

        // Check if the parent level exists
        if (!FPackageName::DoesPackageExist(NormalizedParentPath))
        {
            Subsystem->SendAutomationResponse(Socket, RequestId, false,
                FString::Printf(TEXT("Parent level not found: %s"), *ParentLevel), nullptr, TEXT("LEVEL_NOT_FOUND"));
            return true;
        }
    }

    // Build full sublevel path
    FString FullSublevelPath;
    if (SublevelPath.IsEmpty())
    {
        FString WorldPath = World->GetOutermost()->GetName();
        FullSublevelPath = FPaths::GetPath(WorldPath) / SublevelName;
    }
    else
    {
        // Security: Validate sublevel path format to prevent traversal attacks
        FString SafePath = SanitizeProjectRelativePath(SublevelPath);
        if (SafePath.IsEmpty())
        {
            Subsystem->SendAutomationResponse(Socket, RequestId, false,
                FString::Printf(TEXT("Invalid or unsafe sublevel path: %s"), *SublevelPath),
                nullptr, TEXT("SECURITY_VIOLATION"));
            return true;
        }
        FullSublevelPath = SafePath;
    }

    // Ensure path starts with /Game/
    if (!FullSublevelPath.StartsWith(TEXT("/Game/")))
    {
        FullSublevelPath = TEXT("/Game/") + FullSublevelPath;
    }

    // IDEMPOTENT: Check if sublevel already exists
    if (FPackageName::DoesPackageExist(FullSublevelPath))
    {
        // Sublevel already exists - find or create streaming reference
        ULevelStreaming* ExistingStreamingLevel = nullptr;
        for (ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
        {
            if (StreamingLevel && StreamingLevel->GetWorldAssetPackageFName().ToString() == FullSublevelPath)
            {
                ExistingStreamingLevel = StreamingLevel;
                break;
            }
        }

        TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
        ResponseJson->SetStringField(TEXT("sublevelName"), SublevelName);
        ResponseJson->SetStringField(TEXT("sublevelPath"), FullSublevelPath);
        ResponseJson->SetStringField(TEXT("parentLevel"), World->GetMapName());
        ResponseJson->SetBoolField(TEXT("alreadyExisted"), true);
        ResponseJson->SetBoolField(TEXT("streamingAdded"), ExistingStreamingLevel != nullptr);

        Subsystem->SendAutomationResponse(Socket, RequestId, true,
            FString::Printf(TEXT("Sublevel already exists: %s"), *FullSublevelPath), ResponseJson);
        return true;
    }

    // CRITICAL FIX: Create the actual sublevel asset on disk using UEditorLevelUtils
    // This creates a proper .umap file that can be loaded later
    // See: EditorLevelUtils.h - CreateNewStreamingLevel creates a new level and adds it as streaming

    // Build the package name for the new sublevel
    FString SublevelPackageName = FullSublevelPath;

    // Create a new level package
    UPackage* SublevelPackage = CreatePackage(*SublevelPackageName);
    if (!SublevelPackage)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Failed to create package for sublevel: %s"), *SublevelPackageName), nullptr, TEXT("PACKAGE_CREATION_FAILED"));
        return true;
    }

    // Create the new world for the sublevel
    UWorld* NewSublevelWorld = UWorld::CreateWorld(EWorldType::Inactive, false, FName(*SublevelName), SublevelPackage);
    if (!NewSublevelWorld)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Failed to create world for sublevel: %s"), *SublevelName), nullptr, TEXT("WORLD_CREATION_FAILED"));
        return true;
    }

    // Initialize the world if not already initialized
    if (!NewSublevelWorld->bIsWorldInitialized)
    {
        NewSublevelWorld->InitWorld();
    }

    // Mark package dirty
    SublevelPackage->MarkPackageDirty();

    // Save the sublevel to disk
    bool bSaveSucceeded = true;
    if (bSave)
    {
        bSaveSucceeded = McpSafeLevelSave(NewSublevelWorld->PersistentLevel, SublevelPackageName);

        if (bSaveSucceeded)
        {
            // Flush asset registry so the new level is immediately discoverable
            IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
            FString LevelFilename;
            if (FPackageName::TryConvertLongPackageNameToFilename(SublevelPackageName, LevelFilename, FPackageName::GetMapPackageExtension()))
            {
                TArray<FString> FilesToScan;
                FilesToScan.Add(LevelFilename);
                AssetRegistry.ScanFilesSynchronous(FilesToScan, true);
            }
        }
    }

    // Create streaming level to add to parent world
    ULevelStreamingDynamic* StreamingLevel = NewObject<ULevelStreamingDynamic>(World, ULevelStreamingDynamic::StaticClass());
    if (StreamingLevel)
    {
        StreamingLevel->SetWorldAssetByPackageName(FName(*SublevelPackageName));
        StreamingLevel->LevelTransform = FTransform::Identity;
        StreamingLevel->SetShouldBeVisible(true);
        StreamingLevel->SetShouldBeLoaded(true);

        // Add to world's streaming levels
        World->AddStreamingLevel(StreamingLevel);
    }

    // Mark parent world dirty
    World->MarkPackageDirty();

    // Save parent world if requested (to persist streaming level reference)
    if (bSave && StreamingLevel)
    {
        McpSafeAssetSave(World);
    }

    // CRITICAL: Clean up the created sublevel world from memory to prevent "World Memory Leaks" crash
    // Same fix as HandleCreateLevel - see that function for detailed comments
    // Note: Using bIsWorldInitialized directly for UE 5.0 compatibility (IsInitialized() added in 5.1)
    if (bSaveSucceeded && NewSublevelWorld)
    {
        if (NewSublevelWorld->bIsWorldInitialized)
        {
            NewSublevelWorld->CleanupWorld();
        }

        NewSublevelWorld->bIsTearingDown = true;

        if (NewSublevelWorld->PersistentLevel)
        {
            NewSublevelWorld->PersistentLevel->bIsVisible = false;
            for (AActor* Actor : NewSublevelWorld->PersistentLevel->Actors)
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

        if (NewSublevelWorld->IsRooted())
        {
            NewSublevelWorld->RemoveFromRoot();
        }

        NewSublevelWorld->SetFlags(RF_Transient);
        if (SublevelPackage && SublevelPackage->IsRooted())
        {
            SublevelPackage->RemoveFromRoot();
        }
        if (SublevelPackage)
        {
            SublevelPackage->SetFlags(RF_Transient);
        }

        CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
        FlushRenderingCommands();
    }

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetStringField(TEXT("sublevelName"), SublevelName);
    ResponseJson->SetStringField(TEXT("sublevelPath"), FullSublevelPath);
    ResponseJson->SetStringField(TEXT("parentLevel"), World->GetMapName());
    ResponseJson->SetBoolField(TEXT("saved"), bSave && bSaveSucceeded);
    ResponseJson->SetBoolField(TEXT("streamingAdded"), StreamingLevel != nullptr);

    if (bSave && !bSaveSucceeded)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Sublevel created but save failed: %s"), *SublevelName),
            ResponseJson, TEXT("SAVE_FAILED"));
        return true;
    }

    FString Message = FString::Printf(TEXT("Created sublevel: %s at %s"), *SublevelName, *FullSublevelPath);
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

}
#endif
