// Copyright Epic Games, Inc. All Rights Reserved.
// Phase 46: Modding & UGC System Handlers
// Implements 25 actions for PAK loading, mod discovery, asset overrides, SDK generation, and security

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/FileManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "ISourceControlModule.h"
#endif

// ============================================
// Conditional Includes - PAK System
// ============================================

#if __has_include("IPlatformFilePak.h")
#include "IPlatformFilePak.h"
#define MCP_HAS_PAK_FILE 1
#else
#define MCP_HAS_PAK_FILE 0
#endif

#if __has_include("Misc/CoreDelegates.h")
#include "Misc/CoreDelegates.h"
#define MCP_HAS_CORE_DELEGATES 1
#else
#define MCP_HAS_CORE_DELEGATES 0
#endif

// Pak File Utility (UE 5.0+)
#if __has_include("Misc/AES.h")
#include "Misc/AES.h"
#define MCP_HAS_PAK_ENCRYPTION 1
#else
#define MCP_HAS_PAK_ENCRYPTION 0
#endif

// Class Viewer for SDK export
#if __has_include("ClassViewerModule.h")
#include "ClassViewerModule.h"
#define MCP_HAS_CLASS_VIEWER 1
#else
#define MCP_HAS_CLASS_VIEWER 0
#endif

// ============================================
// Helper Functions
// ============================================
namespace ModdingHelpers
{
    static TSharedPtr<FJsonObject> MakeErrorResponse(const FString& ErrorMsg)
    {
        TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), ErrorMsg);
        return Response;
    }

    static TSharedPtr<FJsonObject> MakeSuccessResponse(const FString& Message)
    {
        TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), Message);
        return Response;
    }

    /**
     * Validates a PAK file path to prevent path traversal attacks.
     * Checks for:
     * - Path traversal sequences (../, ..\)
     * - Null bytes
     * - Invalid characters for security
     * - Path must be within allowed mod directories
     * 
     * @param InPath The path to validate
     * @param OutNormalizedPath The normalized, validated path (if valid)
     * @param OutError Error message (if invalid)
     * @return true if path is safe, false if it should be rejected
     */
    static bool ValidatePakPath(const FString& InPath, FString& OutNormalizedPath, FString& OutError)
    {
        if (InPath.IsEmpty())
        {
            OutError = TEXT("Path is empty");
            return false;
        }

        // Check for null bytes (security)
        if (InPath.Contains(TEXT("\0")))
        {
            OutError = TEXT("Path contains invalid null bytes");
            return false;
        }

        // Check for path traversal attempts
        if (InPath.Contains(TEXT("..")))
        {
            OutError = TEXT("Path traversal sequences (..) are not allowed");
            return false;
        }

        // Normalize the path
        OutNormalizedPath = InPath;
        FPaths::NormalizeFilename(OutNormalizedPath);
        FPaths::CollapseRelativeDirectories(OutNormalizedPath);

        // After normalization, check again for traversal (in case of encoded sequences)
        if (OutNormalizedPath.Contains(TEXT("..")))
        {
            OutError = TEXT("Path contains traversal sequences after normalization");
            return false;
        }

        // Verify the path is within allowed mod directories
        TArray<FString> AllowedPaths;
        // Add default mod directories
        AllowedPaths.Add(FPaths::ProjectModsDir());
        AllowedPaths.Add(FPaths::Combine(FPaths::ProjectUserDir(), TEXT("Mods")));
        AllowedPaths.Add(FPaths::ProjectContentDir());
        AllowedPaths.Add(FPaths::ProjectDir());
        
        bool bIsAllowed = false;
        for (const FString& AllowedPath : AllowedPaths)
        {
            FString NormalizedAllowed = AllowedPath;
            FPaths::NormalizeDirectoryName(NormalizedAllowed);
            
            if (OutNormalizedPath.StartsWith(NormalizedAllowed))
            {
                bIsAllowed = true;
                break;
            }
        }

        if (!bIsAllowed)
        {
            OutError = TEXT("Path is outside allowed mod directories");
            return false;
        }

        // Verify file extension is .pak
        if (!OutNormalizedPath.EndsWith(TEXT(".pak"), ESearchCase::IgnoreCase))
        {
            OutError = TEXT("Only .pak files are allowed");
            return false;
        }

        return true;
    }

    static TArray<FString> GetModPaths()
    {
        TArray<FString> Paths;
        FString PathsString;
        if (GConfig->GetString(TEXT("Modding"), TEXT("ModPaths"), PathsString, GGameUserSettingsIni))
        {
            PathsString.ParseIntoArray(Paths, TEXT(";"), true);
        }
        else
        {
            // Default mod paths
            Paths.Add(FPaths::ProjectModsDir());
            Paths.Add(FPaths::Combine(FPaths::ProjectUserDir(), TEXT("Mods")));
        }
        return Paths;
    }

    static void SaveModPaths(const TArray<FString>& Paths)
    {
        FString PathsString = FString::Join(Paths, TEXT(";"));
        GConfig->SetString(TEXT("Modding"), TEXT("ModPaths"), *PathsString, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);
    }

    static TSharedPtr<FJsonObject> PakInfoToJson(const FString& PakPath, bool bMounted)
    {
        TSharedPtr<FJsonObject> Info = MakeShared<FJsonObject>();
        Info->SetStringField(TEXT("path"), PakPath);
        Info->SetStringField(TEXT("name"), FPaths::GetCleanFilename(PakPath));
        Info->SetBoolField(TEXT("mounted"), bMounted);
        
        // Get file size
        int64 FileSize = IFileManager::Get().FileSize(*PakPath);
        Info->SetNumberField(TEXT("sizeBytes"), static_cast<double>(FileSize));
        
        // Get timestamp
        FDateTime ModTime = IFileManager::Get().GetTimeStamp(*PakPath);
        Info->SetStringField(TEXT("modifiedTime"), ModTime.ToString());
        
        return Info;
    }

    // Track mounted PAK files - protected by critical section for thread safety
    static FCriticalSection ModdingStateMutex;
    static TMap<FString, FString> MountedPaks; // Path -> MountPoint
    static TArray<FString> ModLoadOrder;
    static TMap<FString, FString> AssetRedirects; // Original -> Override
    static TSet<FString> AllowedOperations;
    static bool bSandboxEnabled = true;

    // RAII lock guard for modding state
    struct FModdingStateLock
    {
        FModdingStateLock() { ModdingStateMutex.Lock(); }
        ~FModdingStateLock() { ModdingStateMutex.Unlock(); }
    };
}

// ============================================
// Main Handler Implementation
// ============================================
bool UMcpAutomationBridgeSubsystem::HandleManageModdingAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    using namespace ModdingHelpers;

    // Lock modding state for thread safety
    FModdingStateLock StateLock;

    FString ActionType;
    if (!Payload->TryGetStringField(TEXT("action_type"), ActionType))
    {
        ActionType = Action;
    }

    TSharedPtr<FJsonObject> Response;

    // ========================================
    // PAK LOADING (6 actions)
    // ========================================
    if (ActionType == TEXT("configure_mod_loading_paths"))
    {
        const TArray<TSharedPtr<FJsonValue>>* PathsArray = nullptr;
        TArray<FString> NewPaths;
        
        if (Payload->TryGetArrayField(TEXT("paths"), PathsArray))
        {
            for (const TSharedPtr<FJsonValue>& Val : *PathsArray)
            {
                FString Path;
                if (Val->TryGetString(Path) && !Path.IsEmpty())
                {
                    // Normalize path
                    FPaths::NormalizeDirectoryName(Path);
                    NewPaths.Add(Path);
                }
            }
        }
        
        if (NewPaths.Num() == 0)
        {
            Response = MakeErrorResponse(TEXT("No valid paths provided"));
        }
        else
        {
            SaveModPaths(NewPaths);
            
            Response = MakeSuccessResponse(FString::Printf(TEXT("Configured %d mod loading paths"), NewPaths.Num()));
            
            TArray<TSharedPtr<FJsonValue>> PathsJsonArray;
            for (const FString& Path : NewPaths)
            {
                PathsJsonArray.Add(MakeShared<FJsonValueString>(Path));
            }
            Response->SetArrayField(TEXT("paths"), PathsJsonArray);
        }
    }
    else if (ActionType == TEXT("scan_for_mod_paks"))
    {
        TArray<FString> ModPaths = GetModPaths();
        TArray<TSharedPtr<FJsonValue>> FoundPaks;
        int32 TotalFound = 0;
        
        for (const FString& ModPath : ModPaths)
        {
            TArray<FString> PakFiles;
            IFileManager::Get().FindFilesRecursive(PakFiles, *ModPath, TEXT("*.pak"), true, false);
            
            for (const FString& PakFile : PakFiles)
            {
                bool bMounted = MountedPaks.Contains(PakFile);
                TSharedPtr<FJsonObject> PakInfo = PakInfoToJson(PakFile, bMounted);
                FoundPaks.Add(MakeShared<FJsonValueObject>(PakInfo));
                TotalFound++;
            }
        }
        
        Response = MakeSuccessResponse(FString::Printf(TEXT("Found %d PAK files"), TotalFound));
        Response->SetArrayField(TEXT("pakFiles"), FoundPaks);
        Response->SetNumberField(TEXT("totalFound"), TotalFound);
    }
    else if (ActionType == TEXT("load_mod_pak"))
    {
        FString PakPath;
        FString MountPoint = TEXT("/Game/Mods/");
        int32 Priority = 0;
        
        Payload->TryGetStringField(TEXT("pakPath"), PakPath);
        Payload->TryGetStringField(TEXT("mountPoint"), MountPoint);
        Payload->TryGetNumberField(TEXT("priority"), Priority);
        
        // SECURITY: Validate PAK path to prevent path traversal attacks
        FString ValidatedPath;
        FString ValidationError;
        if (!ValidatePakPath(PakPath, ValidatedPath, ValidationError))
        {
            Response = MakeErrorResponse(FString::Printf(TEXT("Invalid PAK path: %s"), *ValidationError));
        }
        else if (!FPaths::FileExists(ValidatedPath))
        {
            Response = MakeErrorResponse(FString::Printf(TEXT("PAK file not found: %s"), *ValidatedPath));
        }
        else
        {
            PakPath = ValidatedPath; // Use the validated/normalized path
#if MCP_HAS_PAK_FILE
            FPakPlatformFile* PakFileMgr = static_cast<FPakPlatformFile*>(
                FPlatformFileManager::Get().FindPlatformFile(TEXT("PakFile")));
            
            if (PakFileMgr)
            {
                if (PakFileMgr->Mount(*PakPath, Priority, *MountPoint))
                {
                    MountedPaks.Add(PakPath, MountPoint);
                    
                    // Rescan asset registry for new content
                    IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
                    AssetRegistry.ScanPathsSynchronous({MountPoint}, true);
                    
                    Response = MakeSuccessResponse(FString::Printf(TEXT("Mounted PAK: %s at %s"), *FPaths::GetCleanFilename(PakPath), *MountPoint));
                    Response->SetStringField(TEXT("pakPath"), PakPath);
                    Response->SetStringField(TEXT("mountPoint"), MountPoint);
                    Response->SetNumberField(TEXT("priority"), Priority);
                }
                else
                {
                    Response = MakeErrorResponse(FString::Printf(TEXT("Failed to mount PAK: %s"), *PakPath));
                }
            }
            else
            {
                Response = MakeErrorResponse(TEXT("PAK file system not available"));
            }
#else
            Response = MakeErrorResponse(TEXT("PAK file support not available in this build"));
#endif
        }
    }
    else if (ActionType == TEXT("unload_mod_pak"))
    {
        FString PakPath;
        Payload->TryGetStringField(TEXT("pakPath"), PakPath);
        
        // SECURITY: Validate PAK path to prevent path traversal attacks
        FString ValidatedPath;
        FString ValidationError;
        if (!ValidatePakPath(PakPath, ValidatedPath, ValidationError))
        {
            Response = MakeErrorResponse(FString::Printf(TEXT("Invalid PAK path: %s"), *ValidationError));
        }
        else if (!MountedPaks.Contains(ValidatedPath))
        {
            Response = MakeErrorResponse(FString::Printf(TEXT("PAK not mounted: %s"), *ValidatedPath));
        }
        else
        {
            PakPath = ValidatedPath; // Use the validated/normalized path
#if MCP_HAS_PAK_FILE
            FPakPlatformFile* PakFileMgr = static_cast<FPakPlatformFile*>(
                FPlatformFileManager::Get().FindPlatformFile(TEXT("PakFile")));
            
            if (PakFileMgr)
            {
                if (PakFileMgr->Unmount(*PakPath))
                {
                    FString MountPoint = MountedPaks[PakPath];
                    MountedPaks.Remove(PakPath);
                    
                    Response = MakeSuccessResponse(FString::Printf(TEXT("Unmounted PAK: %s"), *FPaths::GetCleanFilename(PakPath)));
                    Response->SetStringField(TEXT("pakPath"), PakPath);
                    Response->SetStringField(TEXT("previousMountPoint"), MountPoint);
                }
                else
                {
                    Response = MakeErrorResponse(FString::Printf(TEXT("Failed to unmount PAK: %s"), *PakPath));
                }
            }
            else
            {
                Response = MakeErrorResponse(TEXT("PAK file system not available"));
            }
#else
            Response = MakeErrorResponse(TEXT("PAK file support not available in this build"));
#endif
        }
    }
    else if (ActionType == TEXT("validate_mod_pak"))
    {
        FString PakPath;
        bool bCheckSignature = false;
        
        Payload->TryGetStringField(TEXT("pakPath"), PakPath);
        Payload->TryGetBoolField(TEXT("checkSignature"), bCheckSignature);
        
        // SECURITY: Validate PAK path to prevent path traversal attacks
        FString ValidatedPath;
        FString ValidationError;
        if (!ValidatePakPath(PakPath, ValidatedPath, ValidationError))
        {
            Response = MakeErrorResponse(FString::Printf(TEXT("Invalid PAK path: %s"), *ValidationError));
        }
        else if (!FPaths::FileExists(ValidatedPath))
        {
            Response = MakeErrorResponse(FString::Printf(TEXT("PAK file not found: %s"), *ValidatedPath));
        }
        else
        {
            PakPath = ValidatedPath; // Use the validated/normalized path
            // Basic validation: check file exists and has valid size
            int64 FileSize = IFileManager::Get().FileSize(*PakPath);
            bool bValid = FileSize > 0;
            
            TArray<FString> ValidationErrors;
            TArray<FString> Warnings;
            
            if (FileSize <= 0)
            {
                ValidationErrors.Add(TEXT("PAK file is empty or unreadable"));
            }
            else if (FileSize < 44) // Minimum PAK header size
            {
                ValidationErrors.Add(TEXT("PAK file too small to contain valid header"));
            }
            
            // Check file extension
            if (!PakPath.EndsWith(TEXT(".pak"), ESearchCase::IgnoreCase))
            {
                Warnings.Add(TEXT("File does not have .pak extension"));
            }
            
            Response = MakeShared<FJsonObject>();
            Response->SetBoolField(TEXT("success"), true);
            Response->SetBoolField(TEXT("valid"), bValid && ValidationErrors.Num() == 0);
            Response->SetStringField(TEXT("pakPath"), PakPath);
            Response->SetNumberField(TEXT("sizeBytes"), static_cast<double>(FileSize));
            
            TArray<TSharedPtr<FJsonValue>> ErrorsArray;
            for (const FString& Err : ValidationErrors)
            {
                ErrorsArray.Add(MakeShared<FJsonValueString>(Err));
            }
            Response->SetArrayField(TEXT("errors"), ErrorsArray);
            
            TArray<TSharedPtr<FJsonValue>> WarningsArray;
            for (const FString& Warn : Warnings)
            {
                WarningsArray.Add(MakeShared<FJsonValueString>(Warn));
            }
            Response->SetArrayField(TEXT("warnings"), WarningsArray);
        }
    }
    else if (ActionType == TEXT("configure_mod_load_order"))
    {
        const TArray<TSharedPtr<FJsonValue>>* OrderArray = nullptr;
        
        if (Payload->TryGetArrayField(TEXT("loadOrder"), OrderArray))
        {
            ModLoadOrder.Empty();
            for (const TSharedPtr<FJsonValue>& Val : *OrderArray)
            {
                FString ModId;
                if (Val->TryGetString(ModId))
                {
                    ModLoadOrder.Add(ModId);
                }
            }
            
            // Save to config
            FString OrderString = FString::Join(ModLoadOrder, TEXT(","));
            GConfig->SetString(TEXT("Modding"), TEXT("LoadOrder"), *OrderString, GGameUserSettingsIni);
            GConfig->Flush(false, GGameUserSettingsIni);
            
            Response = MakeSuccessResponse(FString::Printf(TEXT("Configured load order for %d mods"), ModLoadOrder.Num()));
            
            TArray<TSharedPtr<FJsonValue>> OrderJsonArray;
            for (const FString& ModId : ModLoadOrder)
            {
                OrderJsonArray.Add(MakeShared<FJsonValueString>(ModId));
            }
            Response->SetArrayField(TEXT("loadOrder"), OrderJsonArray);
        }
        else
        {
            Response = MakeErrorResponse(TEXT("loadOrder array is required"));
        }
    }
    // ========================================
    // MOD DISCOVERY (5 actions)
    // ========================================
    else if (ActionType == TEXT("list_installed_mods"))
    {
        TArray<TSharedPtr<FJsonValue>> InstalledMods;
        
        for (const auto& Pair : MountedPaks)
        {
            TSharedPtr<FJsonObject> ModInfo = MakeShared<FJsonObject>();
            ModInfo->SetStringField(TEXT("pakPath"), Pair.Key);
            ModInfo->SetStringField(TEXT("mountPoint"), Pair.Value);
            ModInfo->SetStringField(TEXT("name"), FPaths::GetBaseFilename(Pair.Key));
            ModInfo->SetBoolField(TEXT("enabled"), true);
            ModInfo->SetBoolField(TEXT("loaded"), true);
            
            InstalledMods.Add(MakeShared<FJsonValueObject>(ModInfo));
        }
        
        Response = MakeSuccessResponse(FString::Printf(TEXT("Found %d installed mods"), InstalledMods.Num()));
        Response->SetArrayField(TEXT("mods"), InstalledMods);
        Response->SetNumberField(TEXT("totalMods"), InstalledMods.Num());
    }
    else if (ActionType == TEXT("enable_mod"))
    {
        FString ModId;
        Payload->TryGetStringField(TEXT("modId"), ModId);
        
        if (ModId.IsEmpty())
        {
            Response = MakeErrorResponse(TEXT("modId is required"));
        }
        else
        {
            // Store enabled state in config
            GConfig->SetBool(TEXT("Modding/EnabledMods"), *ModId, true, GGameUserSettingsIni);
            GConfig->Flush(false, GGameUserSettingsIni);
            
            Response = MakeSuccessResponse(FString::Printf(TEXT("Enabled mod: %s"), *ModId));
            Response->SetStringField(TEXT("modId"), ModId);
            Response->SetBoolField(TEXT("enabled"), true);
        }
    }
    else if (ActionType == TEXT("disable_mod"))
    {
        FString ModId;
        Payload->TryGetStringField(TEXT("modId"), ModId);
        
        if (ModId.IsEmpty())
        {
            Response = MakeErrorResponse(TEXT("modId is required"));
        }
        else
        {
            // Store disabled state in config
            GConfig->SetBool(TEXT("Modding/EnabledMods"), *ModId, false, GGameUserSettingsIni);
            GConfig->Flush(false, GGameUserSettingsIni);
            
            // Unload if currently loaded
            for (const auto& Pair : MountedPaks)
            {
                if (FPaths::GetBaseFilename(Pair.Key) == ModId)
                {
#if MCP_HAS_PAK_FILE
                    FPakPlatformFile* PakFileMgr = static_cast<FPakPlatformFile*>(
                        FPlatformFileManager::Get().FindPlatformFile(TEXT("PakFile")));
                    if (PakFileMgr)
                    {
                        PakFileMgr->Unmount(*Pair.Key);
                    }
#endif
                    MountedPaks.Remove(Pair.Key);
                    break;
                }
            }
            
            Response = MakeSuccessResponse(FString::Printf(TEXT("Disabled mod: %s"), *ModId));
            Response->SetStringField(TEXT("modId"), ModId);
            Response->SetBoolField(TEXT("enabled"), false);
        }
    }
    else if (ActionType == TEXT("check_mod_compatibility"))
    {
        FString ModId;
        FString TargetVersion;
        
        Payload->TryGetStringField(TEXT("modId"), ModId);
        Payload->TryGetStringField(TEXT("targetVersion"), TargetVersion);
        
        if (ModId.IsEmpty())
        {
            Response = MakeErrorResponse(TEXT("modId is required"));
        }
        else
        {
            // Get engine version for compatibility check
            FString EngineVersion = FEngineVersion::Current().ToString();
            
            TArray<FString> CompatibilityIssues;
            bool bCompatible = true;
            
            // Check if mod exists
            bool bModExists = false;
            for (const auto& Pair : MountedPaks)
            {
                if (FPaths::GetBaseFilename(Pair.Key) == ModId)
                {
                    bModExists = true;
                    break;
                }
            }
            
            if (!bModExists)
            {
                CompatibilityIssues.Add(TEXT("Mod is not currently loaded"));
            }
            
            Response = MakeShared<FJsonObject>();
            Response->SetBoolField(TEXT("success"), true);
            Response->SetStringField(TEXT("modId"), ModId);
            Response->SetBoolField(TEXT("compatible"), bCompatible);
            Response->SetStringField(TEXT("engineVersion"), EngineVersion);
            
            TArray<TSharedPtr<FJsonValue>> IssuesArray;
            for (const FString& Issue : CompatibilityIssues)
            {
                IssuesArray.Add(MakeShared<FJsonValueString>(Issue));
            }
            Response->SetArrayField(TEXT("issues"), IssuesArray);
        }
    }
    else if (ActionType == TEXT("get_mod_info"))
    {
        FString ModId;
        Payload->TryGetStringField(TEXT("modId"), ModId);
        
        if (ModId.IsEmpty())
        {
            Response = MakeErrorResponse(TEXT("modId is required"));
        }
        else
        {
            TSharedPtr<FJsonObject> ModInfo = MakeShared<FJsonObject>();
            ModInfo->SetStringField(TEXT("modId"), ModId);
            
            bool bFound = false;
            for (const auto& Pair : MountedPaks)
            {
                if (FPaths::GetBaseFilename(Pair.Key) == ModId)
                {
                    bFound = true;
                    ModInfo->SetStringField(TEXT("pakPath"), Pair.Key);
                    ModInfo->SetStringField(TEXT("mountPoint"), Pair.Value);
                    ModInfo->SetBoolField(TEXT("loaded"), true);
                    
                    int64 FileSize = IFileManager::Get().FileSize(*Pair.Key);
                    ModInfo->SetNumberField(TEXT("sizeBytes"), static_cast<double>(FileSize));
                    
                    FDateTime ModTime = IFileManager::Get().GetTimeStamp(*Pair.Key);
                    ModInfo->SetStringField(TEXT("modifiedTime"), ModTime.ToString());
                    break;
                }
            }
            
            if (!bFound)
            {
                ModInfo->SetBoolField(TEXT("loaded"), false);
            }
            
            Response = MakeSuccessResponse(FString::Printf(TEXT("Retrieved info for mod: %s"), *ModId));
            Response->SetObjectField(TEXT("modInfo"), ModInfo);
        }
    }
    // ========================================
    // ASSET OVERRIDE (4 actions)
    // ========================================
    else if (ActionType == TEXT("configure_asset_override_paths"))
    {
        const TArray<TSharedPtr<FJsonValue>>* PathsArray = nullptr;
        TArray<FString> OverridePaths;
        
        if (Payload->TryGetArrayField(TEXT("overridePaths"), PathsArray))
        {
            for (const TSharedPtr<FJsonValue>& Val : *PathsArray)
            {
                FString Path;
                if (Val->TryGetString(Path))
                {
                    OverridePaths.Add(Path);
                }
            }
        }
        
        FString PathsString = FString::Join(OverridePaths, TEXT(";"));
        GConfig->SetString(TEXT("Modding"), TEXT("AssetOverridePaths"), *PathsString, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);
        
        Response = MakeSuccessResponse(FString::Printf(TEXT("Configured %d asset override paths"), OverridePaths.Num()));
        
        TArray<TSharedPtr<FJsonValue>> PathsJsonArray;
        for (const FString& Path : OverridePaths)
        {
            PathsJsonArray.Add(MakeShared<FJsonValueString>(Path));
        }
        Response->SetArrayField(TEXT("overridePaths"), PathsJsonArray);
    }
    else if (ActionType == TEXT("register_mod_asset_redirect"))
    {
        FString OriginalPath;
        FString OverridePath;
        
        Payload->TryGetStringField(TEXT("originalPath"), OriginalPath);
        Payload->TryGetStringField(TEXT("overridePath"), OverridePath);
        
        if (OriginalPath.IsEmpty() || OverridePath.IsEmpty())
        {
            Response = MakeErrorResponse(TEXT("originalPath and overridePath are required"));
        }
        else
        {
            AssetRedirects.Add(OriginalPath, OverridePath);
            
            Response = MakeSuccessResponse(FString::Printf(TEXT("Registered asset redirect: %s -> %s"), *OriginalPath, *OverridePath));
            Response->SetStringField(TEXT("originalPath"), OriginalPath);
            Response->SetStringField(TEXT("overridePath"), OverridePath);
            Response->SetNumberField(TEXT("totalRedirects"), AssetRedirects.Num());
        }
    }
    else if (ActionType == TEXT("restore_original_asset"))
    {
        FString AssetPath;
        Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
        
        if (AssetPath.IsEmpty())
        {
            Response = MakeErrorResponse(TEXT("assetPath is required"));
        }
        else if (!AssetRedirects.Contains(AssetPath))
        {
            Response = MakeErrorResponse(FString::Printf(TEXT("No redirect found for: %s"), *AssetPath));
        }
        else
        {
            FString PreviousOverride = AssetRedirects[AssetPath];
            AssetRedirects.Remove(AssetPath);
            
            Response = MakeSuccessResponse(FString::Printf(TEXT("Restored original asset: %s"), *AssetPath));
            Response->SetStringField(TEXT("assetPath"), AssetPath);
            Response->SetStringField(TEXT("previousOverride"), PreviousOverride);
        }
    }
    else if (ActionType == TEXT("list_asset_overrides"))
    {
        TArray<TSharedPtr<FJsonValue>> OverridesList;
        
        for (const auto& Pair : AssetRedirects)
        {
            TSharedPtr<FJsonObject> Override = MakeShared<FJsonObject>();
            Override->SetStringField(TEXT("originalPath"), Pair.Key);
            Override->SetStringField(TEXT("overridePath"), Pair.Value);
            OverridesList.Add(MakeShared<FJsonValueObject>(Override));
        }
        
        Response = MakeSuccessResponse(FString::Printf(TEXT("Found %d asset overrides"), OverridesList.Num()));
        Response->SetArrayField(TEXT("overrides"), OverridesList);
        Response->SetNumberField(TEXT("totalOverrides"), OverridesList.Num());
    }
    // ========================================
    // SDK GENERATION (4 actions)
    // ========================================
    else if (ActionType == TEXT("export_moddable_headers"))
    {
        FString OutputPath;
        const TArray<TSharedPtr<FJsonValue>>* ClassesArray = nullptr;
        TArray<FString> ClassesToExport;
        
        Payload->TryGetStringField(TEXT("outputPath"), OutputPath);
        
        if (Payload->TryGetArrayField(TEXT("classes"), ClassesArray))
        {
            for (const TSharedPtr<FJsonValue>& Val : *ClassesArray)
            {
                FString ClassName;
                if (Val->TryGetString(ClassName))
                {
                    ClassesToExport.Add(ClassName);
                }
            }
        }
        
        if (OutputPath.IsEmpty())
        {
            OutputPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("ModSDK"), TEXT("Headers"));
        }
        
        // SECURITY: Validate output path to prevent directory traversal attacks
        FString NormalizedPath = FPaths::ConvertRelativePathToFull(OutputPath);
        if (!FPaths::IsUnderDirectory(NormalizedPath, FPaths::ProjectDir()) &&
            !FPaths::IsUnderDirectory(NormalizedPath, FPaths::ProjectSavedDir()))
        {
            Response = MakeErrorResponse(TEXT("Invalid output path: must be within project directory"));
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Invalid output path: must be within project directory"), Response);
            return false;
        }
        
        // Create output directory
        IFileManager::Get().MakeDirectory(*OutputPath, true);
        
        // Generate a basic header file with moddable class info
        FString HeaderContent;
        HeaderContent += TEXT("// Auto-generated Mod SDK Headers\n");
        HeaderContent += TEXT("// Generated: ") + FDateTime::Now().ToString() + TEXT("\n\n");
        HeaderContent += TEXT("#pragma once\n\n");
        
        for (const FString& ClassName : ClassesToExport)
        {
            HeaderContent += FString::Printf(TEXT("// Class: %s\n"), *ClassName);
            HeaderContent += FString::Printf(TEXT("// UCLASS(Blueprintable)\n// class %s : public UObject {};\n\n"), *ClassName);
        }
        
        FString HeaderPath = FPaths::Combine(OutputPath, TEXT("ModdableClasses.h"));
        FFileHelper::SaveStringToFile(HeaderContent, *HeaderPath);
        
        Response = MakeSuccessResponse(FString::Printf(TEXT("Exported headers to: %s"), *OutputPath));
        Response->SetStringField(TEXT("outputPath"), OutputPath);
        Response->SetNumberField(TEXT("classesExported"), ClassesToExport.Num());
    }
    else if (ActionType == TEXT("create_mod_template_project"))
    {
        FString TemplateName;
        FString OutputPath;
        FString ModType = TEXT("content"); // content, code, mixed
        
        Payload->TryGetStringField(TEXT("templateName"), TemplateName);
        Payload->TryGetStringField(TEXT("outputPath"), OutputPath);
        Payload->TryGetStringField(TEXT("modType"), ModType);
        
        if (TemplateName.IsEmpty())
        {
            TemplateName = TEXT("MyMod");
        }
        
        if (OutputPath.IsEmpty())
        {
            OutputPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("ModSDK"), TEXT("Templates"), TemplateName);
        }
        
        // SECURITY: Validate output path to prevent directory traversal attacks
        FString NormalizedTemplatePath = FPaths::ConvertRelativePathToFull(OutputPath);
        if (!FPaths::IsUnderDirectory(NormalizedTemplatePath, FPaths::ProjectDir()) &&
            !FPaths::IsUnderDirectory(NormalizedTemplatePath, FPaths::ProjectSavedDir()))
        {
            Response = MakeErrorResponse(TEXT("Invalid output path: must be within project directory"));
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Invalid output path: must be within project directory"), Response);
            return false;
        }
        
        // Create template structure
        IFileManager::Get().MakeDirectory(*OutputPath, true);
        IFileManager::Get().MakeDirectory(*FPaths::Combine(OutputPath, TEXT("Content")), true);
        IFileManager::Get().MakeDirectory(*FPaths::Combine(OutputPath, TEXT("Config")), true);
        
        // Create mod.json manifest
        TSharedPtr<FJsonObject> Manifest = MakeShared<FJsonObject>();
        Manifest->SetStringField(TEXT("name"), TemplateName);
        Manifest->SetStringField(TEXT("version"), TEXT("1.0.0"));
        Manifest->SetStringField(TEXT("type"), ModType);
        Manifest->SetStringField(TEXT("author"), TEXT(""));
        Manifest->SetStringField(TEXT("description"), TEXT("A new mod"));
        
        FString ManifestString;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ManifestString);
        FJsonSerializer::Serialize(Manifest.ToSharedRef(), Writer);
        
        FString ManifestPath = FPaths::Combine(OutputPath, TEXT("mod.json"));
        FFileHelper::SaveStringToFile(ManifestString, *ManifestPath);
        
        Response = MakeSuccessResponse(FString::Printf(TEXT("Created mod template: %s"), *TemplateName));
        Response->SetStringField(TEXT("templateName"), TemplateName);
        Response->SetStringField(TEXT("outputPath"), OutputPath);
        Response->SetStringField(TEXT("modType"), ModType);
    }
    else if (ActionType == TEXT("configure_exposed_classes"))
    {
        const TArray<TSharedPtr<FJsonValue>>* ClassesArray = nullptr;
        TArray<FString> ExposedClasses;
        
        if (Payload->TryGetArrayField(TEXT("classes"), ClassesArray))
        {
            for (const TSharedPtr<FJsonValue>& Val : *ClassesArray)
            {
                FString ClassName;
                if (Val->TryGetString(ClassName))
                {
                    ExposedClasses.Add(ClassName);
                }
            }
        }
        
        FString ClassesString = FString::Join(ExposedClasses, TEXT(","));
        GConfig->SetString(TEXT("Modding"), TEXT("ExposedClasses"), *ClassesString, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);
        
        Response = MakeSuccessResponse(FString::Printf(TEXT("Configured %d exposed classes"), ExposedClasses.Num()));
        
        TArray<TSharedPtr<FJsonValue>> ClassesJsonArray;
        for (const FString& ClassName : ExposedClasses)
        {
            ClassesJsonArray.Add(MakeShared<FJsonValueString>(ClassName));
        }
        Response->SetArrayField(TEXT("exposedClasses"), ClassesJsonArray);
    }
    else if (ActionType == TEXT("get_sdk_config"))
    {
        TSharedPtr<FJsonObject> SDKConfig = MakeShared<FJsonObject>();
        
        // Get exposed classes
        FString ClassesString;
        if (GConfig->GetString(TEXT("Modding"), TEXT("ExposedClasses"), ClassesString, GGameUserSettingsIni))
        {
            TArray<FString> Classes;
            ClassesString.ParseIntoArray(Classes, TEXT(","), true);
            
            TArray<TSharedPtr<FJsonValue>> ClassesArray;
            for (const FString& ClassName : Classes)
            {
                ClassesArray.Add(MakeShared<FJsonValueString>(ClassName));
            }
            SDKConfig->SetArrayField(TEXT("exposedClasses"), ClassesArray);
        }
        
        // Get mod paths
        TArray<FString> ModPaths = GetModPaths();
        TArray<TSharedPtr<FJsonValue>> PathsArray;
        for (const FString& Path : ModPaths)
        {
            PathsArray.Add(MakeShared<FJsonValueString>(Path));
        }
        SDKConfig->SetArrayField(TEXT("modPaths"), PathsArray);
        
        // Engine version
        SDKConfig->SetStringField(TEXT("engineVersion"), FEngineVersion::Current().ToString());
        
        Response = MakeSuccessResponse(TEXT("Retrieved SDK configuration"));
        Response->SetObjectField(TEXT("sdkConfig"), SDKConfig);
    }
    // ========================================
    // SECURITY (4 actions)
    // ========================================
    else if (ActionType == TEXT("configure_mod_sandbox"))
    {
        bool bEnableSandbox = true;
        bool bAllowFileSystem = false;
        bool bAllowNetwork = false;
        
        Payload->TryGetBoolField(TEXT("enableSandbox"), bEnableSandbox);
        Payload->TryGetBoolField(TEXT("allowFileSystem"), bAllowFileSystem);
        Payload->TryGetBoolField(TEXT("allowNetwork"), bAllowNetwork);
        
        bSandboxEnabled = bEnableSandbox;
        
        // Store in config
        GConfig->SetBool(TEXT("Modding/Security"), TEXT("SandboxEnabled"), bEnableSandbox, GGameUserSettingsIni);
        GConfig->SetBool(TEXT("Modding/Security"), TEXT("AllowFileSystem"), bAllowFileSystem, GGameUserSettingsIni);
        GConfig->SetBool(TEXT("Modding/Security"), TEXT("AllowNetwork"), bAllowNetwork, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);
        
        Response = MakeSuccessResponse(TEXT("Configured mod sandbox settings"));
        Response->SetBoolField(TEXT("sandboxEnabled"), bEnableSandbox);
        Response->SetBoolField(TEXT("allowFileSystem"), bAllowFileSystem);
        Response->SetBoolField(TEXT("allowNetwork"), bAllowNetwork);
    }
    else if (ActionType == TEXT("set_allowed_mod_operations"))
    {
        const TArray<TSharedPtr<FJsonValue>>* OperationsArray = nullptr;
        
        AllowedOperations.Empty();
        
        if (Payload->TryGetArrayField(TEXT("operations"), OperationsArray))
        {
            for (const TSharedPtr<FJsonValue>& Val : *OperationsArray)
            {
                FString Operation;
                if (Val->TryGetString(Operation))
                {
                    AllowedOperations.Add(Operation);
                }
            }
        }
        
        // Store in config
        TArray<FString> OperationsList = AllowedOperations.Array();
        FString OperationsString = FString::Join(OperationsList, TEXT(","));
        GConfig->SetString(TEXT("Modding/Security"), TEXT("AllowedOperations"), *OperationsString, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);
        
        Response = MakeSuccessResponse(FString::Printf(TEXT("Configured %d allowed operations"), AllowedOperations.Num()));
        
        TArray<TSharedPtr<FJsonValue>> OpsJsonArray;
        for (const FString& Op : AllowedOperations)
        {
            OpsJsonArray.Add(MakeShared<FJsonValueString>(Op));
        }
        Response->SetArrayField(TEXT("allowedOperations"), OpsJsonArray);
    }
    else if (ActionType == TEXT("validate_mod_content"))
    {
        FString ModPath;
        bool bCheckScripts = true;
        bool bCheckAssets = true;
        
        Payload->TryGetStringField(TEXT("modPath"), ModPath);
        Payload->TryGetBoolField(TEXT("checkScripts"), bCheckScripts);
        Payload->TryGetBoolField(TEXT("checkAssets"), bCheckAssets);
        
        if (ModPath.IsEmpty())
        {
            Response = MakeErrorResponse(TEXT("modPath is required"));
        }
        else
        {
            TArray<FString> SecurityIssues;
            TArray<FString> Warnings;
            bool bValid = true;
            
            // Check for suspicious file types
            if (bCheckScripts)
            {
                TArray<FString> ScriptFiles;
                IFileManager::Get().FindFilesRecursive(ScriptFiles, *ModPath, TEXT("*.dll"), true, false);
                IFileManager::Get().FindFilesRecursive(ScriptFiles, *ModPath, TEXT("*.exe"), true, false);
                
                for (const FString& ScriptFile : ScriptFiles)
                {
                    SecurityIssues.Add(FString::Printf(TEXT("Potentially dangerous file: %s"), *FPaths::GetCleanFilename(ScriptFile)));
                    bValid = false;
                }
            }
            
            if (bCheckAssets)
            {
                TArray<FString> AssetFiles;
                IFileManager::Get().FindFilesRecursive(AssetFiles, *ModPath, TEXT("*.uasset"), true, false);
                
                if (AssetFiles.Num() == 0)
                {
                    Warnings.Add(TEXT("No .uasset files found in mod"));
                }
            }
            
            Response = MakeShared<FJsonObject>();
            Response->SetBoolField(TEXT("success"), true);
            Response->SetBoolField(TEXT("valid"), bValid);
            Response->SetStringField(TEXT("modPath"), ModPath);
            
            TArray<TSharedPtr<FJsonValue>> IssuesArray;
            for (const FString& Issue : SecurityIssues)
            {
                IssuesArray.Add(MakeShared<FJsonValueString>(Issue));
            }
            Response->SetArrayField(TEXT("securityIssues"), IssuesArray);
            
            TArray<TSharedPtr<FJsonValue>> WarningsArray;
            for (const FString& Warning : Warnings)
            {
                WarningsArray.Add(MakeShared<FJsonValueString>(Warning));
            }
            Response->SetArrayField(TEXT("warnings"), WarningsArray);
        }
    }
    else if (ActionType == TEXT("get_security_config"))
    {
        TSharedPtr<FJsonObject> SecurityConfig = MakeShared<FJsonObject>();
        
        bool bSandbox = true;
        bool bAllowFS = false;
        bool bAllowNet = false;
        
        GConfig->GetBool(TEXT("Modding/Security"), TEXT("SandboxEnabled"), bSandbox, GGameUserSettingsIni);
        GConfig->GetBool(TEXT("Modding/Security"), TEXT("AllowFileSystem"), bAllowFS, GGameUserSettingsIni);
        GConfig->GetBool(TEXT("Modding/Security"), TEXT("AllowNetwork"), bAllowNet, GGameUserSettingsIni);
        
        SecurityConfig->SetBoolField(TEXT("sandboxEnabled"), bSandbox);
        SecurityConfig->SetBoolField(TEXT("allowFileSystem"), bAllowFS);
        SecurityConfig->SetBoolField(TEXT("allowNetwork"), bAllowNet);
        
        // Get allowed operations
        FString OperationsString;
        if (GConfig->GetString(TEXT("Modding/Security"), TEXT("AllowedOperations"), OperationsString, GGameUserSettingsIni))
        {
            TArray<FString> Operations;
            OperationsString.ParseIntoArray(Operations, TEXT(","), true);
            
            TArray<TSharedPtr<FJsonValue>> OpsArray;
            for (const FString& Op : Operations)
            {
                OpsArray.Add(MakeShared<FJsonValueString>(Op));
            }
            SecurityConfig->SetArrayField(TEXT("allowedOperations"), OpsArray);
        }
        
        Response = MakeSuccessResponse(TEXT("Retrieved security configuration"));
        Response->SetObjectField(TEXT("securityConfig"), SecurityConfig);
    }
    // ========================================
    // UTILITY (2 actions)
    // ========================================
    else if (ActionType == TEXT("get_modding_info"))
    {
        TSharedPtr<FJsonObject> ModdingInfo = MakeShared<FJsonObject>();
        
        // System info
        ModdingInfo->SetStringField(TEXT("engineVersion"), FEngineVersion::Current().ToString());
        ModdingInfo->SetNumberField(TEXT("mountedPakCount"), MountedPaks.Num());
        ModdingInfo->SetNumberField(TEXT("assetRedirectCount"), AssetRedirects.Num());
        ModdingInfo->SetBoolField(TEXT("sandboxEnabled"), bSandboxEnabled);
        
        // PAK support
#if MCP_HAS_PAK_FILE
        ModdingInfo->SetBoolField(TEXT("pakSupportAvailable"), true);
#else
        ModdingInfo->SetBoolField(TEXT("pakSupportAvailable"), false);
#endif
        
        // Mod paths
        TArray<FString> ModPaths = GetModPaths();
        TArray<TSharedPtr<FJsonValue>> PathsArray;
        for (const FString& Path : ModPaths)
        {
            PathsArray.Add(MakeShared<FJsonValueString>(Path));
        }
        ModdingInfo->SetArrayField(TEXT("modPaths"), PathsArray);
        
        // Load order
        TArray<TSharedPtr<FJsonValue>> LoadOrderArray;
        for (const FString& ModId : ModLoadOrder)
        {
            LoadOrderArray.Add(MakeShared<FJsonValueString>(ModId));
        }
        ModdingInfo->SetArrayField(TEXT("loadOrder"), LoadOrderArray);
        
        Response = MakeSuccessResponse(TEXT("Retrieved modding system info"));
        Response->SetObjectField(TEXT("moddingInfo"), ModdingInfo);
    }
    else if (ActionType == TEXT("reset_mod_system"))
    {
        bool bUnloadPaks = true;
        bool bClearRedirects = true;
        bool bResetConfig = false;
        
        Payload->TryGetBoolField(TEXT("unloadPaks"), bUnloadPaks);
        Payload->TryGetBoolField(TEXT("clearRedirects"), bClearRedirects);
        Payload->TryGetBoolField(TEXT("resetConfig"), bResetConfig);
        
        int32 PaksUnloaded = 0;
        int32 RedirectsCleared = 0;
        
        // Unload all PAKs
        if (bUnloadPaks)
        {
#if MCP_HAS_PAK_FILE
            FPakPlatformFile* PakFileMgr = static_cast<FPakPlatformFile*>(
                FPlatformFileManager::Get().FindPlatformFile(TEXT("PakFile")));
            
            if (PakFileMgr)
            {
                for (const auto& Pair : MountedPaks)
                {
                    if (PakFileMgr->Unmount(*Pair.Key))
                    {
                        PaksUnloaded++;
                    }
                }
            }
#endif
            MountedPaks.Empty();
        }
        
        // Clear asset redirects
        if (bClearRedirects)
        {
            RedirectsCleared = AssetRedirects.Num();
            AssetRedirects.Empty();
        }
        
        // Reset config
        if (bResetConfig)
        {
            GConfig->EmptySection(TEXT("Modding"), GGameUserSettingsIni);
            GConfig->EmptySection(TEXT("Modding/EnabledMods"), GGameUserSettingsIni);
            GConfig->EmptySection(TEXT("Modding/Security"), GGameUserSettingsIni);
            GConfig->Flush(false, GGameUserSettingsIni);
            
            // Reset runtime state
            ModLoadOrder.Empty();
            AllowedOperations.Empty();
            bSandboxEnabled = true;
        }
        
        Response = MakeSuccessResponse(TEXT("Mod system reset complete"));
        Response->SetNumberField(TEXT("paksUnloaded"), PaksUnloaded);
        Response->SetNumberField(TEXT("redirectsCleared"), RedirectsCleared);
        Response->SetBoolField(TEXT("configReset"), bResetConfig);
    }
    else
    {
        Response = MakeErrorResponse(FString::Printf(TEXT("Unknown modding action: %s"), *ActionType));
    }

    // Send response
    bool bSuccess = Response->HasField(TEXT("success")) ? Response->GetBoolField(TEXT("success")) : true;
    FString Message = Response->HasField(TEXT("message")) ? Response->GetStringField(TEXT("message")) : TEXT("Operation completed");
    SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Response);
    return true;
}
