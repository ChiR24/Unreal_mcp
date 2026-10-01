#include "Domains/LevelStructure/McpAutomationBridge_LevelStructureActions.h"
#include "Domains/LevelStructure/McpAutomationBridge_LevelStructureEditorWorld.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "GameFramework/WorldSettings.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Safety/McpSafeOperations.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"
#include "WorldPartition/WorldPartition.h"

namespace McpLevelStructure
{

bool HandleCreateLevel(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace LevelStructureHelpers;

    // CRITICAL: levelName is required - check if explicitly provided, not just if empty
    FString LevelName;
    bool bHasLevelName = false;
    if (Payload.IsValid())
    {
        bHasLevelName = Payload->TryGetStringField(TEXT("levelName"), LevelName);
    }

    if (!bHasLevelName || LevelName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("levelName is required for create_level"), nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Object- and package-name characters (a level name is both), reserved device names (CON, LPT1, ...) and length.
    FText InvalidNameReason;
    if (!FName::IsValidXName(LevelName, FString(INVALID_OBJECTNAME_CHARACTERS) + INVALID_LONGPACKAGE_CHARACTERS, &InvalidNameReason) ||
        !FFileHelper::IsFilenameValidForSaving(LevelName, InvalidNameReason))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Invalid levelName '%s': %s"), *LevelName, *InvalidNameReason.ToString()),
            nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString LevelPath = GetJsonStringField(Payload, TEXT("levelPath"), TEXT("/Game/Maps"));
    bool bCreateWorldPartition = GetJsonBoolField(Payload, TEXT("bCreateWorldPartition"), false);
    bool bUseExternalActors = GetJsonBoolField(Payload, TEXT("bUseExternalActors"), false);
    bool bSave = GetJsonBoolField(Payload, TEXT("save"), true);
    bool bLoadAfterCreate = GetJsonBoolField(Payload, TEXT("loadAfterCreate"), false);

    // CRITICAL: When creating a World Partition level, OFPA (External Actors) should be enabled
    // for data layer support. If bCreateWorldPartition is true but bUseExternalActors is not specified,
    // automatically enable OFPA for better compatibility with data layers.
    // This can be overridden by explicitly setting bUseExternalActors to false.
    if (bCreateWorldPartition && !Payload->HasField(TEXT("bUseExternalActors"))) {
        bUseExternalActors = true;
    }

    // Security: Validate level path format to prevent traversal attacks
    FString SafeLevelPath = SanitizeProjectRelativePath(LevelPath);
    if (SafeLevelPath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            McpPathRefusalMessage(TEXT("level path"), LevelPath),
            nullptr, TEXT("SECURITY_VIOLATION"));
        return true;
    }
    LevelPath = SafeLevelPath;

    // levelPath may already name the level (".../Maps/MCP_Arena" + levelName
    // "MCP_Arena"); do not nest it a second time (dogfood #15).
    FString FullPath = LevelPath.EndsWith(TEXT("/") + LevelName, ESearchCase::IgnoreCase) ? LevelPath : LevelPath / LevelName;
    if (!FullPath.StartsWith(TEXT("/")))
    {
        FullPath = TEXT("/Game/") + FullPath;
    }

    // With loadAfterCreate, opens FullPath and reports where the editor ended up; false (refusal sent) when it would not load.
    auto LoadIfRequested = [&](const TSharedPtr<FJsonObject>& Result, const TCHAR* FailurePrefix)
    {
        Result->SetBoolField(TEXT("loaded"), false);
        if (!bLoadAfterCreate)
        {
            return true;
        }
        const bool bLoaded = McpSafeLoadMap(FullPath, true);
        Result->SetBoolField(TEXT("loaded"), bLoaded);
        if (UWorld* EditorWorld = GetEditorWorld())
        {
            Result->SetStringField(TEXT("currentLevelPath"), EditorWorld->GetOutermost()->GetName());
        }
        if (!bLoaded)
        {
            Subsystem->SendAutomationResponse(Socket, RequestId, false,
                FString::Printf(TEXT("%s could not be loaded: %s"), FailurePrefix, *FullPath), Result, TEXT("LOAD_FAILED"));
        }
        return bLoaded;
    };

    // IDEMPOTENT: Check if level already exists and return success if so
    // This makes create_level idempotent - calling it multiple times with the same path succeeds
    // The level is not recreated if it already exists (prevents WorldSettings collision crash)

    // A level that already exists in memory (created earlier this session, before
    // the asset registry synced) or on disk is reported idempotently.
    bool bAlreadyExists = false;
    if (UPackage* ExistingPackage = FindObject<UPackage>(nullptr, *FullPath))
    {
        bAlreadyExists = FindObject<UWorld>(ExistingPackage, *LevelName) != nullptr;
    }
    if (bAlreadyExists || FPackageName::DoesPackageExist(FullPath))
    {
        // IDEMPOTENT: Level exists on disk - return success with exists flag
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("levelPath"), FullPath);
        Result->SetBoolField(TEXT("exists"), true);
        Result->SetBoolField(TEXT("alreadyExisted"), true);
        if (!LoadIfRequested(Result, TEXT("Level exists but")))
        {
            return true;
        }
        Subsystem->SendAutomationResponse(Socket, RequestId, true,
            FString::Printf(TEXT("Level already exists: %s"), *FullPath),
            Result, FString());
        return true;
    }

    UPackage* Package = CreatePackage(*FullPath);
    if (!Package)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Failed to create package for level: %s"), *FullPath), nullptr);
        return true;
    }

    UWorld* NewWorld = UWorld::CreateWorld(EWorldType::Inactive, false, FName(*LevelName), Package);
    if (!NewWorld)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Failed to create world for level: %s"), *FullPath), nullptr);
        return true;
    }

    // CreateWorld may already initialize it in some UE versions
    if (!NewWorld->bIsWorldInitialized)
    {
        NewWorld->InitWorld();
    }

    bool bWorldPartitionActuallyEnabled = false;
    if (bCreateWorldPartition)
    {
        // World Partition is enabled via WorldSettings using CreateOrRepairWorldPartition
        AWorldSettings* WorldSettings = NewWorld->GetWorldSettings(true);
        if (WorldSettings)
        {
            // Use the editor-only API to create World Partition
            // This properly initializes the WorldPartition subsystem, RuntimeHash, and related structures
            UWorldPartition* NewWorldPartition = UWorldPartition::CreateOrRepairWorldPartition(WorldSettings);
            if (NewWorldPartition)
            {
                bWorldPartitionActuallyEnabled = true;
                UE_LOG(LogMcpLevelStructureHandlers, Log, TEXT("Created World Partition for level: %s"), *FullPath);
            }
            else
            {
                UE_LOG(LogMcpLevelStructureHandlers, Warning, TEXT("Failed to create World Partition for level: %s"), *FullPath);
            }
        }
        else
        {
            UE_LOG(LogMcpLevelStructureHandlers, Warning, TEXT("Failed to get WorldSettings for World Partition creation: %s"), *FullPath);
        }
    }

    // This is required for Data Layer support in World Partition levels
    bool bExternalActorsActuallyEnabled = false;
    if (bUseExternalActors && NewWorld->PersistentLevel)
    {
        // This enables actors to be stored as external packages, which is required
        // for Data Layer compatibility in World Partition levels
        NewWorld->PersistentLevel->bUseExternalActors = true;
        bExternalActorsActuallyEnabled = true;
        UE_LOG(LogMcpLevelStructureHandlers, Log, TEXT("Enabled External Actors (OFPA) for level: %s"), *FullPath);
    }

    Package->MarkPackageDirty();

    bool bSaveSucceeded = true;
    if (bSave)
    {
        // CRITICAL: Use McpSafeLevelSave to avoid Intel GPU driver crashes.
        // FEditorFileUtils::SaveLevel() directly can trigger MONZA DdiThreadingContext
        // exceptions on Intel GPUs due to render thread race conditions.
        // The safe wrapper suspends rendering during save and implements retry logic.
        // Explicitly use 5 retries for Intel GPU resilience (max 7.75s total retry time).
        bSaveSucceeded = McpSafeLevelSave(NewWorld->PersistentLevel, FullPath);

        if (bSaveSucceeded)
        {
            // Flush asset registry so the new level is immediately discoverable
            IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

            FString LevelFilename;
            if (FPackageName::TryConvertLongPackageNameToFilename(FullPath, LevelFilename, FPackageName::GetMapPackageExtension()))
            {
                TArray<FString> FilesToScan;
                FilesToScan.Add(LevelFilename);
                AssetRegistry.ScanFilesSynchronous(FilesToScan, true);
            }
        }
        else
        {
            UE_LOG(LogMcpLevelStructureHandlers, Error, TEXT("McpSafeLevelSave failed for: %s"), *FullPath);
        }
    }

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(ResponseJson, NewWorld);
    ResponseJson->SetStringField(TEXT("levelName"), LevelName);
    ResponseJson->SetStringField(TEXT("levelPath"), FullPath);
    ResponseJson->SetBoolField(TEXT("worldPartitionEnabled"), bWorldPartitionActuallyEnabled);
    ResponseJson->SetBoolField(TEXT("worldPartitionRequested"), bCreateWorldPartition);
    ResponseJson->SetBoolField(TEXT("externalActorsEnabled"), bExternalActorsActuallyEnabled);
    ResponseJson->SetBoolField(TEXT("externalActorsRequested"), bUseExternalActors);
    ResponseJson->SetBoolField(TEXT("saved"), bSave && bSaveSucceeded);
    if (bCreateWorldPartition && !bWorldPartitionActuallyEnabled)
    {
        ResponseJson->SetStringField(TEXT("worldPartitionNote"), TEXT("World Partition must be enabled via editor UI or project settings for new levels"));
    }

    // If save was requested but failed, report error
    // NOTE: We do NOT clean up the level from memory because:
    // 1. McpSafeLevelSave now uses FPackageName::DoesPackageExist as fallback verification
    // 2. The file might actually exist on disk even if file verification timed out
    // 3. The idempotent check will find it on retry and return success
    // 4. Cleaning up causes race conditions where the level exists on disk but not in memory
    if (bSave && !bSaveSucceeded)
    {
        UE_LOG(LogMcpLevelStructureHandlers, Warning, TEXT("Save verification reported failure, but level may exist on disk: %s"), *FullPath);

        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Level created but save verification failed: %s"), *FullPath),
            ResponseJson, TEXT("SAVE_VERIFICATION_FAILED"));
        return true;
    }

    // CRITICAL FIX for UE 5.7 World Memory Leaks:
    // After saving, clean up the created world from memory. If we leave it in memory,
    // subsequent LoadMap calls will crash with "World Memory Leaks" because the world
    // package has root flags and can't be garbage collected.
    //
    // Root Cause: UWorld::CreateWorld(EWorldType::Inactive, ...) creates a standalone
    // world that stays in memory as a root object. When FEditorFileUtils::LoadMap()
    // tries to load the same package, UE 5.7 detects the existing package → Fatal Error.
    //
    // Reference: EditorServer.cpp line 2524 - "World Memory Leaks: %d leaks objects"
    // Reference: World.cpp line 1488-1491 - CleanupWorld must be called for initialized Inactive worlds
    if (bSaveSucceeded && NewWorld)
    {
        CleanupCreatedLevelWorldAfterSave(NewWorld, Package, FullPath);
    }

    // Creating does not switch the editor to the new level unless
    // loadAfterCreate is set; say so explicitly (dogfood #162).
    if (!LoadIfRequested(ResponseJson, TEXT("Level created but")))
    {
        return true;
    }

    FString Message = FString::Printf(TEXT("Created level: %s"), *FullPath);
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

}
