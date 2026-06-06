#include "McpAutomationBridge_LevelStructureActions.h"
#include "McpAutomationBridge_LevelStructureEditorWorld.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "GameFramework/WorldSettings.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "McpHandlerUtils.h"
#include "McpSafeOperations.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"
#include "WorldPartition/WorldPartition.h"

#if WITH_EDITOR
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

    // Fail if levelName was not provided OR if it's empty
    if (!bHasLevelName || LevelName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("levelName is required for create_level"), nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Validate levelName for invalid characters
    // These characters are not allowed in Windows filenames and UE asset names
    const FString InvalidChars = TEXT("\\/:*?\"<>|");
    for (const TCHAR& Char : LevelName)
    {
        if (InvalidChars.Contains(FString(1, &Char)))
        {
            Subsystem->SendAutomationResponse(Socket, RequestId, false,
                FString::Printf(TEXT("levelName contains invalid character: '%c'. Cannot use: \\ / : * ? \" < > |"), Char),
                nullptr, TEXT("INVALID_ARGUMENT"));
            return true;
        }
    }

    // Check length (max 255 chars)
    if (LevelName.Len() > 255)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("levelName exceeds maximum length of 255 characters"),
            nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Check for reserved Windows filenames
    const TArray<FString> ReservedNames = {
        TEXT("CON"), TEXT("PRN"), TEXT("AUX"), TEXT("NUL"),
        TEXT("COM1"), TEXT("COM2"), TEXT("COM3"), TEXT("COM4"), TEXT("COM5"),
        TEXT("COM6"), TEXT("COM7"), TEXT("COM8"), TEXT("COM9"),
        TEXT("LPT1"), TEXT("LPT2"), TEXT("LPT3"), TEXT("LPT4"), TEXT("LPT5"),
        TEXT("LPT6"), TEXT("LPT7"), TEXT("LPT8"), TEXT("LPT9")
    };
    FString UpperLevelName = LevelName.ToUpper();
    if (ReservedNames.Contains(UpperLevelName))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("levelName cannot be a reserved Windows device name: %s"), *LevelName),
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
            FString::Printf(TEXT("Invalid or unsafe level path: %s"), *LevelPath),
            nullptr, TEXT("SECURITY_VIOLATION"));
        return true;
    }
    LevelPath = SafeLevelPath;

    // Build full path
    FString FullPath = LevelPath / LevelName;
    if (!FullPath.StartsWith(TEXT("/")))
    {
        FullPath = TEXT("/Game/") + FullPath;
    }

    // IDEMPOTENT: Check if level already exists and return success if so
    // This makes create_level idempotent - calling it multiple times with the same path succeeds
    // The level is not recreated if it already exists (prevents WorldSettings collision crash)

    // Check 1: Check if package exists IN MEMORY (from previous operations in same session)
    // This catches cases where a level was created but the asset registry hasn't synced yet
    UPackage* ExistingPackage = FindObject<UPackage>(nullptr, *FullPath);
    if (ExistingPackage)
    {
        // Check if there's already a world in this package
        UWorld* ExistingWorld = FindObject<UWorld>(ExistingPackage, *LevelName);
        if (ExistingWorld)
        {
            // IDEMPOTENT: Level exists in memory - return success with exists flag
            TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
            Result->SetStringField(TEXT("levelPath"), FullPath);
            Result->SetBoolField(TEXT("exists"), true);
            Result->SetBoolField(TEXT("alreadyExisted"), true);
            if (bLoadAfterCreate) {
                const bool bLoaded = McpSafeLoadMap(FullPath, true);
                Result->SetBoolField(TEXT("loaded"), bLoaded);
                if (GEditor && GEditor->GetEditorWorldContext().World()) {
                    Result->SetStringField(TEXT("currentLevelPath"), GEditor->GetEditorWorldContext().World()->GetOutermost()->GetName());
                }
                if (!bLoaded) {
                    Subsystem->SendAutomationResponse(Socket, RequestId, false,
                        FString::Printf(TEXT("Level exists but could not be loaded: %s"), *FullPath),
                        Result, TEXT("LOAD_FAILED"));
                    return true;
                }
            }
            Subsystem->SendAutomationResponse(Socket, RequestId, true,
                FString::Printf(TEXT("Level already exists: %s"), *FullPath),
                Result, FString());
            return true;
        }
    }

    // Check 2: Check if package exists ON DISK (covers previously saved levels)
    if (FPackageName::DoesPackageExist(FullPath))
    {
        // IDEMPOTENT: Level exists on disk - return success with exists flag
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("levelPath"), FullPath);
        Result->SetBoolField(TEXT("exists"), true);
        Result->SetBoolField(TEXT("alreadyExisted"), true);
        if (bLoadAfterCreate) {
            const bool bLoaded = McpSafeLoadMap(FullPath, true);
            Result->SetBoolField(TEXT("loaded"), bLoaded);
            if (GEditor && GEditor->GetEditorWorldContext().World()) {
                Result->SetStringField(TEXT("currentLevelPath"), GEditor->GetEditorWorldContext().World()->GetOutermost()->GetName());
            }
            if (!bLoaded) {
                Subsystem->SendAutomationResponse(Socket, RequestId, false,
                    FString::Printf(TEXT("Level exists but could not be loaded: %s"), *FullPath),
                    Result, TEXT("LOAD_FAILED"));
                return true;
            }
        }
        Subsystem->SendAutomationResponse(Socket, RequestId, true,
            FString::Printf(TEXT("Level already exists: %s"), *FullPath),
            Result, FString());
        return true;
    }

    // Create the level package
    UPackage* Package = CreatePackage(*FullPath);
    if (!Package)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Failed to create package for level: %s"), *FullPath), nullptr);
        return true;
    }

    // Create a new world
    UWorld* NewWorld = UWorld::CreateWorld(EWorldType::Inactive, false, FName(*LevelName), Package);
    if (!NewWorld)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Failed to create world for level: %s"), *FullPath), nullptr);
        return true;
    }

    // Initialize the world only if not already initialized
    // CreateWorld may already initialize it in some UE versions
    if (!NewWorld->bIsWorldInitialized)
    {
        NewWorld->InitWorld();
    }

    // Enable World Partition if requested
    bool bWorldPartitionActuallyEnabled = false;
#if ENGINE_MAJOR_VERSION >= 5
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
#endif

    // Enable One File Per Actor (OFPA/External Actors) if requested
    // This is required for Data Layer support in World Partition levels
    bool bExternalActorsActuallyEnabled = false;
    if (bUseExternalActors && NewWorld->PersistentLevel)
    {
#if WITH_EDITORONLY_DATA
        // Set the bUseExternalActors flag on the persistent level
        // This enables actors to be stored as external packages, which is required
        // for Data Layer compatibility in World Partition levels
        NewWorld->PersistentLevel->bUseExternalActors = true;
        bExternalActorsActuallyEnabled = true;
        UE_LOG(LogMcpLevelStructureHandlers, Log, TEXT("Enabled External Actors (OFPA) for level: %s"), *FullPath);
#endif
    }

    // Mark package dirty
    Package->MarkPackageDirty();

    // Save if requested
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

            // Convert package path to filename for scanning
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

    if (bLoadAfterCreate)
    {
        const bool bLoaded = McpSafeLoadMap(FullPath, true);
        ResponseJson->SetBoolField(TEXT("loaded"), bLoaded);
        if (GEditor && GEditor->GetEditorWorldContext().World())
        {
            ResponseJson->SetStringField(TEXT("currentLevelPath"), GEditor->GetEditorWorldContext().World()->GetOutermost()->GetName());
        }
        if (!bLoaded)
        {
            Subsystem->SendAutomationResponse(Socket, RequestId, false,
                FString::Printf(TEXT("Level created but could not be loaded: %s"), *FullPath),
                ResponseJson, TEXT("LOAD_FAILED"));
            return true;
        }
    }

    FString Message = FString::Printf(TEXT("Created level: %s"), *FullPath);
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

}
#endif
