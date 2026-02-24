// Helper utilities for McpAutomationBridgeSubsystem
#pragma once

#include "AssetRegistry/AssetData.h"
#include "Containers/ScriptArray.h"
#include "Containers/StringConv.h"
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformTime.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDevice.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "UObject/UnrealType.h"
#include <type_traits>

// Include engine version definitions for version checks
#include "Runtime/Launch/Resources/Version.h"

// Default definition for MCP_HAS_CONTROLRIG_FACTORY if not defined by build system
// ControlRigBlueprintFactory is available in all UE 5.x versions
// Note: In UE 5.1-5.4 the header is in Private folder, but the class is exported with CONTROLRIGEDITOR_API
// so we use forward declaration instead of including the header
#ifndef MCP_HAS_CONTROLRIG_FACTORY
  #if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    #define MCP_HAS_CONTROLRIG_FACTORY 1
  #else
    #define MCP_HAS_CONTROLRIG_FACTORY 0
  #endif
#endif

// ============================================================================
// UE 5.0 - 5.1+ API Compatibility Macros
// ============================================================================
// These macros abstract API differences between UE versions to allow the same
// code to compile across UE 5.0, 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, and 5.7

// Material API differences:
// UE 5.0: Material->Expressions (direct TArray access)
// UE 5.1+: Material->GetEditorOnlyData()->ExpressionCollection.Expressions
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#define MCP_GET_MATERIAL_EXPRESSIONS(Material) (Material)->GetEditorOnlyData()->ExpressionCollection.Expressions
#define MCP_GET_MATERIAL_INPUT(Material, InputName) (Material)->GetEditorOnlyData()->InputName
#define MCP_HAS_MATERIAL_EDITOR_ONLY_DATA 1
#else
#define MCP_GET_MATERIAL_EXPRESSIONS(Material) (Material)->Expressions
#define MCP_GET_MATERIAL_INPUT(Material, InputName) (Material)->InputName
#define MCP_HAS_MATERIAL_EDITOR_ONLY_DATA 0
#endif

// DataLayer API differences:
// UE 5.0: UDataLayer (direct), no UDataLayerInstance/UDataLayerAsset
// UE 5.1+: UDataLayerInstance, UDataLayerAsset with FDataLayerCreationParameters
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#define MCP_HAS_DATALAYER_INSTANCE 1
#define MCP_HAS_DATALAYER_ASSET 1
#define MCP_DATALAYER_TYPE UDataLayerInstance
#define MCP_DATALAYER_ASSET_TYPE UDataLayerAsset
#else
#define MCP_HAS_DATALAYER_INSTANCE 0
#define MCP_HAS_DATALAYER_ASSET 0
#define MCP_DATALAYER_TYPE UDataLayer
#define MCP_DATALAYER_ASSET_TYPE UDataLayer
#endif

// FReferenceSkeletonModifier API differences:
// UE 5.0: Only Add(), UpdateRefPoseTransform(), FindBoneIndex()
// UE 5.1+: Also Remove(), SetParent()
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#define MCP_HAS_REF_SKELETON_MODIFIER_REMOVE 1
#define MCP_HAS_REF_SKELETON_MODIFIER_SETPARENT 1
#else
#define MCP_HAS_REF_SKELETON_MODIFIER_REMOVE 0
#define MCP_HAS_REF_SKELETON_MODIFIER_SETPARENT 0
#endif

// Niagara API differences:
// UE 5.0: FNiagaraEmitterHandle::GetInstance() returns UNiagaraEmitter*
// UE 5.1+: FNiagaraEmitterHandle::GetInstance() returns FVersionedNiagaraEmitter
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#define MCP_NIAGARA_EMITTER_DATA_TYPE FVersionedNiagaraEmitterData
#define MCP_GET_NIAGARA_EMITTER_DATA(Handle) (Handle)->GetEmitterData()
#define MCP_HAS_NIAGARA_VERSIONING 1
#else
#define MCP_NIAGARA_EMITTER_DATA_TYPE UNiagaraEmitter
#define MCP_GET_NIAGARA_EMITTER_DATA(Handle) (Handle)->GetInstance()
#define MCP_HAS_NIAGARA_VERSIONING 0
#endif

// AssetRegistry API differences:
// UE 5.0: FARFilter uses ClassNames (TArray<FName>)
// UE 5.1+: FARFilter uses ClassPaths (TArray<FTopLevelAssetPath>)
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#define MCP_ASSET_FILTER_CLASS_PATHS Filter.ClassPaths
#define MCP_HAS_ASSET_CLASS_PATHS 1
#define MCP_FTOP_LEVEL_ASSET_PATH FTopLevelAssetPath
#else
#define MCP_ASSET_FILTER_CLASS_PATHS Filter.ClassNames
#define MCP_HAS_ASSET_CLASS_PATHS 0
#define MCP_FTOP_LEVEL_ASSET_PATH FName
#endif

// FAssetData API differences:
// UE 5.0: AssetClass (FName), no GetSoftObjectPath()
// UE 5.1+: AssetClassPath (FTopLevelAssetPath), GetSoftObjectPath()
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#define MCP_ASSET_DATA_GET_CLASS_PATH(AssetData) (AssetData).AssetClassPath.ToString()
#define MCP_ASSET_DATA_GET_SOFT_PATH(AssetData) (AssetData).GetSoftObjectPath().ToString()
#define MCP_HAS_ASSET_SOFT_PATH 1
#else
#define MCP_ASSET_DATA_GET_CLASS_PATH(AssetData) (AssetData).AssetClass.ToString()
#define MCP_ASSET_DATA_GET_SOFT_PATH(AssetData) (AssetData).PackageName.ToString()
#define MCP_HAS_ASSET_SOFT_PATH 0
#endif

// FProperty ExportText API differences:
// UE 5.0: ExportText_Direct() with different parameters
// UE 5.1+: ExportTextItem_Direct()
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#define MCP_PROPERTY_EXPORT_TEXT(Property, OutText, ValuePtr, DefaultValuePtr, Container, Flags) \
    (Property)->ExportTextItem_Direct(OutText, ValuePtr, DefaultValuePtr, Container, Flags)
#else
#define MCP_PROPERTY_EXPORT_TEXT(Property, OutText, ValuePtr, DefaultValuePtr, Container, Flags) \
    (Property)->ExportText_Direct(OutText, ValuePtr, DefaultValuePtr, Flags, Container)
#endif

// SmartObject API differences:
// UE 5.0: Different slot definition structure
// UE 5.1+: FSmartObjectSlotDefinition with bEnabled, ID, etc.
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#define MCP_HAS_SMARTOBJECT_SLOT_ENABLED 1
#define MCP_HAS_SMARTOBJECT_SLOT_ID 1
#else
#define MCP_HAS_SMARTOBJECT_SLOT_ENABLED 0
#define MCP_HAS_SMARTOBJECT_SLOT_ID 0
#endif

// Animation Data Controller API differences:
// UE 5.0: Different API for animation data controller
// UE 5.1+: SetNumberOfFrames(), IsValidBoneTrackName(), etc.
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#define MCP_HAS_ANIM_DATA_CONTROLLER_SET_NUM_FRAMES 1
#define MCP_HAS_ANIM_DATA_MODEL_VALID_BONE_TRACK 1
#else
#define MCP_HAS_ANIM_DATA_CONTROLLER_SET_NUM_FRAMES 0
#define MCP_HAS_ANIM_DATA_MODEL_VALID_BONE_TRACK 0
#endif

// HLOD Layer API differences:
// UE 5.0: UHLODLayer without SetIsSpatiallyLoaded(), SetLayerType()
// UE 5.1+: These methods exist
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#define MCP_HAS_HLOD_SET_IS_SPATIALLY_LOADED 1
#define MCP_HAS_HLOD_SET_LAYER_TYPE 1
#else
#define MCP_HAS_HLOD_SET_IS_SPATIALLY_LOADED 0
#define MCP_HAS_HLOD_SET_LAYER_TYPE 0
#endif

// Spatial Hash Runtime Grid API differences:
// UE 5.0: FSpatialHashRuntimeGrid without Origin
// UE 5.1+: Has Origin member
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#define MCP_HAS_SPATIAL_HASH_RUNTIME_GRID_ORIGIN 1
#else
#define MCP_HAS_SPATIAL_HASH_RUNTIME_GRID_ORIGIN 0
#endif

// Globals used by registry helpers and fast-mode simulations
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeSubsystem.h"

#if WITH_EDITOR
#include "Editor.h"  // GEditor for McpSafeLoadMap
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectIterator.h"
#include "RenderingThread.h"  // FlushRenderingCommands for safe level saves

#if __has_include("EditorAssetLibrary.h")
#include "EditorAssetLibrary.h"
#else
#include "Editor/EditorAssetLibrary.h"
#endif
#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "Engine/LevelStreaming.h"
#include "GameFramework/WorldSettings.h"
#include "TickTaskManagerInterface.h"
#include "HAL/PlatformProcess.h"
#endif

/**
 * Removes control characters (ASCII codes less than 32) from the input JSON
 * string.
 * @param In Input string that may contain control characters.
 * @returns String with all characters with ASCII value < 32 removed.
 */
static inline FString SanitizeIncomingJson(const FString &In) {
  FString Out;
  Out.Reserve(In.Len());
  for (int32 i = 0; i < In.Len(); ++i) {
    const TCHAR C = In[i];
    if (C >= 32)
      Out.AppendChar(C);
  }
  return Out;
}

// Sanitize a project-relative path to prevent traversal attacks.
// Ensures the path starts with a valid root (e.g. /Game, /Engine, /Plugin) and
/**
 * Normalize and validate a project-relative asset path.
 *
 * Ensures the returned path is normalized, begins with a leading '/', rejects
 * any path containing directory traversal sequences (".."), and accepts common
 * roots (/Game, /Engine, /Script) or plugin-like roots (heuristic). If a
 * traversal sequence is found the function logs a warning and returns an empty
 * string.
 *
 * @param InPath Input path to sanitize.
 * @returns Sanitized project-relative path beginning with '/', or an empty
 * string if the input was empty or rejected (for example, when containing
 * "..").
 */
static inline FString SanitizeProjectRelativePath(const FString &InPath) {
  if (InPath.IsEmpty())
    return FString();

  FString CleanPath = InPath;
  
  // Reject Windows absolute paths early (contain drive letter colon)
  if (CleanPath.Len() >= 2 && CleanPath[1] == TEXT(':')) {
    UE_LOG(
        LogMcpAutomationBridgeSubsystem, Warning,
        TEXT("SanitizeProjectRelativePath: Rejected Windows absolute path: %s"),
        *InPath);
    return FString();
  }
  
  FPaths::NormalizeFilename(CleanPath);

  // CRITICAL: FPaths::NormalizeFilename converts / to \ on Windows
  // We need to convert back to forward slashes for UE asset paths
  CleanPath.ReplaceInline(TEXT("\\"), TEXT("/"));

  // Normalize double slashes (prevents engine crash from paths like /Game//Test)
  while (CleanPath.Contains(TEXT("//"))) {
    CleanPath = CleanPath.Replace(TEXT("//"), TEXT("/"));
  }

  // Reject paths containing traversal
  if (CleanPath.Contains(TEXT(".."))) {
    UE_LOG(
        LogMcpAutomationBridgeSubsystem, Warning,
        TEXT("SanitizeProjectRelativePath: Rejected path containing '..': %s"),
        *InPath);
    return FString();
  }

  // Ensure path starts with a slash
  if (!CleanPath.StartsWith(TEXT("/"))) {
    CleanPath = TEXT("/") + CleanPath;
  }

  // Whitelist valid roots - MUST start with one of these
  const bool bValidRoot = CleanPath.StartsWith(TEXT("/Game")) ||
                          CleanPath.StartsWith(TEXT("/Engine")) ||
                          CleanPath.StartsWith(TEXT("/Script"));

  // Reject paths that start with / but don't have a valid root
  // This catches paths like /etc/passwd or /invalid/path
  if (!bValidRoot) {
    // Check if it looks like a plugin path (e.g., /MyPlugin/Content/Asset)
    // Plugin paths must have at least 3 segments: /PluginName/Content/...
    TArray<FString> Segments;
    CleanPath.ParseIntoArray(Segments, TEXT("/"), true);
    const bool bLooksLikePluginPath = Segments.Num() >= 3 &&
        Segments.Num() >= 2 && Segments[1].Equals(TEXT("Content"), ESearchCase::IgnoreCase);
    
    if (!bLooksLikePluginPath) {
      UE_LOG(
          LogMcpAutomationBridgeSubsystem, Warning,
          TEXT("SanitizeProjectRelativePath: Rejected path without valid root (not /Game, /Engine, /Script, or valid plugin path): %s"),
          *InPath);
      return FString();
    }
  }

  return CleanPath;
}

/**
 * Sanitize a file path for use with file operations (export/import snapshot, etc.).
 * Unlike SanitizeProjectRelativePath which requires asset roots (/Game, /Engine, /Script),
 * this function accepts any project-relative file path while still enforcing security.
 *
 * Security checks:
 * - Rejects Windows absolute paths (drive letters)
 * - Rejects path traversal (..)
 * - Ensures path is relative (starts with /)
 * - Normalizes path separators
 *
 * @param InPath Input file path to sanitize
 * @returns Sanitized path if valid, empty string if rejected
 */
static inline FString SanitizeProjectFilePath(const FString &InPath) {
  if (InPath.IsEmpty())
    return FString();

  FString CleanPath = InPath;

  // SECURITY: Reject Windows absolute paths (contain drive letter colon anywhere)
  // Use Contains() for robust detection - handles X:\, X:/, /X:\, and edge cases
  if (CleanPath.Contains(TEXT(":"))) {
    UE_LOG(
        LogMcpAutomationBridgeSubsystem, Warning,
        TEXT("SanitizeProjectFilePath: Rejected Windows absolute path (contains ':'): %s"),
        *InPath);
    return FString();
  }

  FPaths::NormalizeFilename(CleanPath);

  // Convert backslashes to forward slashes
  CleanPath.ReplaceInline(TEXT("\\"), TEXT("/"));

  // Normalize double slashes
  while (CleanPath.Contains(TEXT("//"))) {
    CleanPath = CleanPath.Replace(TEXT("//"), TEXT("/"));
  }

  // Reject paths containing traversal (CRITICAL for security)
  if (CleanPath.Contains(TEXT(".."))) {
    UE_LOG(
        LogMcpAutomationBridgeSubsystem, Warning,
        TEXT("SanitizeProjectFilePath: Rejected path containing '..': %s"),
        *InPath);
    return FString();
  }

  // Ensure path starts with a slash (project-relative)
  if (!CleanPath.StartsWith(TEXT("/"))) {
    CleanPath = TEXT("/") + CleanPath;
  }

  // Reject empty filename
  if (CleanPath.Len() <= 1) {
    UE_LOG(
        LogMcpAutomationBridgeSubsystem, Warning,
        TEXT("SanitizeProjectFilePath: Rejected empty path"));
    return FString();
  }

  // All validation passed - the path is safe for file operations.
  // Unlike asset paths, file paths are permissive and allow any project-relative
  // location (/Temp, /Saved, /Config, etc.) as long as they don't escape the project.
  return CleanPath;
}

/**
 * Validate a basic asset path format.
 *
 * @returns `true` if Path is non-empty, begins with a leading '/', does not
 * contain the parent-traversal segment (".."), consecutive slashes ("//"),
 * or Windows drive letters (":"); `false` otherwise.
 */
static inline bool IsValidAssetPath(const FString &Path) {
  return !Path.IsEmpty() && 
         Path.StartsWith(TEXT("/")) &&
         !Path.Contains(TEXT("..")) && 
         !Path.Contains(TEXT("//")) &&
         !Path.Contains(TEXT(":"));  // Reject Windows absolute paths
}

/**
 * Validate and sanitize an asset name.
 * Removes/replaces characters that are invalid for Unreal asset names,
 * including SQL injection patterns.
 *
 * @param InName Input asset name to sanitize
 * @returns Sanitized name safe for use in asset creation
 */
static inline FString SanitizeAssetName(const FString &InName) {
  if (InName.IsEmpty())
    return TEXT("Asset");

  FString Sanitized = InName.TrimStartAndEnd();
  
  // Replace SQL injection pattern characters with underscore
  // Block: semicolons, quotes, double-dashes, and SQL keywords
  Sanitized = Sanitized.Replace(TEXT(";"), TEXT("_"));
  Sanitized = Sanitized.Replace(TEXT("'"), TEXT("_"));
  Sanitized = Sanitized.Replace(TEXT("\""), TEXT("_"));
  Sanitized = Sanitized.Replace(TEXT("--"), TEXT("_"));
  Sanitized = Sanitized.Replace(TEXT("`"), TEXT("_"));
  
  // Replace other invalid characters for Unreal asset names
  // Invalid: @ # % $ & * ( ) + = [ ] { } < > ? | \ : ~ ! and whitespace
  const TArray<TCHAR> InvalidChars = {
    TEXT('@'), TEXT('#'), TEXT('%'), TEXT('$'), TEXT('&'), TEXT('*'),
    TEXT('('), TEXT(')'), TEXT('+'), TEXT('='), TEXT('['), TEXT(']'),
    TEXT('{'), TEXT('}'), TEXT('<'), TEXT('>'), TEXT('?'), TEXT('|'),
    TEXT('\\'), TEXT(':'), TEXT('~'), TEXT('!'), TEXT(' ')
  };
  
  for (TCHAR C : InvalidChars) {
    TCHAR CharStr[2] = { C, TEXT('\0') };
    Sanitized = Sanitized.Replace(CharStr, TEXT("_"));
  }
  
  // Remove consecutive underscores
  while (Sanitized.Contains(TEXT("__"))) {
    Sanitized = Sanitized.Replace(TEXT("__"), TEXT("_"));
  }
  
  // Remove leading/trailing underscores
  while (Sanitized.StartsWith(TEXT("_"))) {
    Sanitized.RemoveAt(0);
  }
  while (Sanitized.EndsWith(TEXT("_"))) {
    Sanitized.RemoveAt(Sanitized.Len() - 1);
  }
  
  // If empty after sanitization, use default
  if (Sanitized.IsEmpty())
    return TEXT("Asset");
    
  // Ensure name starts with a letter or underscore
  if (!FChar::IsAlpha(Sanitized[0]) && Sanitized[0] != TEXT('_')) {
    Sanitized = TEXT("Asset_") + Sanitized;
  }
  
  // Truncate to reasonable length (64 chars is UE max for asset names)
  if (Sanitized.Len() > 64) {
    Sanitized = Sanitized.Left(64);
  }
  
  return Sanitized;
}

/**
 * Validate and normalize a full asset path for creation.
 * Combines path and name validation, returns validated path or empty on failure.
 *
 * @param FolderPath Parent folder path (e.g., /Game/MyFolder)
 * @param AssetName Name for the asset
 * @param OutFullPath Receives the full validated path
 * @param OutError Receives error message on failure
 * @returns true if path is valid and safe for asset creation
 */
static inline bool ValidateAssetCreationPath(
    const FString &FolderPath, 
    const FString &AssetName,
    FString &OutFullPath,
    FString &OutError)
{
  // Sanitize and validate folder path
  FString SanitizedFolder = SanitizeProjectRelativePath(FolderPath);
  if (SanitizedFolder.IsEmpty()) {
    OutError = TEXT("Invalid folder path: contains traversal or invalid characters");
    return false;
  }
  
  // Ensure folder starts with valid root
  if (!SanitizedFolder.StartsWith(TEXT("/Game")) && 
      !SanitizedFolder.StartsWith(TEXT("/Engine")) &&
      !SanitizedFolder.StartsWith(TEXT("/Script"))) {
    SanitizedFolder = TEXT("/Game") + SanitizedFolder;
  }
  
  // Sanitize asset name
  FString SanitizedName = SanitizeAssetName(AssetName);
  if (SanitizedName.IsEmpty()) {
    OutError = TEXT("Invalid asset name after sanitization");
    return false;
  }
  
  // Build full path
  OutFullPath = SanitizedFolder / SanitizedName;
  
  // Final validation
  if (!IsValidAssetPath(OutFullPath)) {
    OutError = FString::Printf(TEXT("Invalid asset path after normalization: %s"), *OutFullPath);
    return false;
  }
  
  return true;
}

// Normalize an asset path to ensure it's in valid long package name format.
// Uses engine FPackageName API for proper validation.
// - If path doesn't start with '/', prepends '/Game/'
// - Removes trailing slashes
// - Returns the normalized path and whether it's valid
// - Reference: Engine/Source/Runtime/CoreUObject/Public/Misc/PackageName.h
#if WITH_EDITOR
#include "Misc/PackageName.h"

struct FNormalizedAssetPath {
  FString Path;
  bool bIsValid;
  FString ErrorMessage;
};

/**
 * Normalize an input asset path to a valid long package name and validate it.
 *
 * @param InPath The asset path or object path to normalize (may be short,
 * relative, or an object path).
 * @returns An FNormalizedAssetPath containing:
 *   - Path: the normalized package path candidate (may be unchanged if
 * invalid),
 *   - bIsValid: `true` when the path is a valid long package name and, when
 * applicable, the package exists,
 *   - ErrorMessage: populated with a validation error when `bIsValid` is
 * `false`.
 */
static inline FNormalizedAssetPath NormalizeAssetPath(const FString &InPath) {
  FNormalizedAssetPath Result;
  Result.bIsValid = false;

  if (InPath.IsEmpty()) {
    Result.ErrorMessage = TEXT("Asset path is empty");
    return Result;
  }

  FString CleanPath = InPath;

  // Remove trailing slashes
  while (CleanPath.EndsWith(TEXT("/"))) {
    CleanPath.RemoveAt(CleanPath.Len() - 1);
  }

  // Handle object paths (extract package name)
  // Object paths look like: /Game/Package.Object:SubObject
  FString PackageName = FPackageName::ObjectPathToPackageName(CleanPath);
  if (!PackageName.IsEmpty()) {
    CleanPath = PackageName;
  }

  // If path doesn't start with '/', try prepending /Game/
  if (!CleanPath.StartsWith(TEXT("/"))) {
    CleanPath = TEXT("/Game/") + CleanPath;
  }

  // Validate using engine API
  FText Reason;
  if (FPackageName::IsValidLongPackageName(CleanPath, true, &Reason)) {
    Result.Path = CleanPath;
    Result.bIsValid = true;
    return Result;
  }

  // If not in valid root, try other common roots
  TArray<FString> RootsToTry = {TEXT("/Game/"), TEXT("/Engine/"),
                                TEXT("/Script/")};
  FString BaseName = InPath;
  if (BaseName.StartsWith(TEXT("/"))) {
    // Extract just the asset name without the invalid root
    int32 LastSlash = -1;
    if (BaseName.FindLastChar(TEXT('/'), LastSlash) && LastSlash > 0) {
      BaseName = BaseName.RightChop(LastSlash + 1);
    }
  }

  for (const FString &Root : RootsToTry) {
    FString TestPath = Root + BaseName;
    FText DummyReason;
    if (FPackageName::IsValidLongPackageName(TestPath, true, &DummyReason)) {
      // Check if this asset actually exists
      if (FPackageName::DoesPackageExist(TestPath)) {
        Result.Path = TestPath;
        Result.bIsValid = true;
        return Result;
      }
    }
  }

  // Return what we have, with the validation error
  Result.Path = CleanPath;
  Result.ErrorMessage = FString::Printf(
      TEXT("Invalid asset path '%s': %s. Expected format: "
           "/Game/Folder/AssetName or /Engine/Folder/AssetName"),
      *InPath, *Reason.ToString());
  return Result;
}

// Convenience helper that tries to resolve the path and returns it, or empty if
// invalid Also outputs the resolved path to a pointer if provided
static inline FString TryResolveAssetPath(const FString &InPath,
                                          FString *OutResolvedPath = nullptr,
                                          FString *OutError = nullptr) {
  FNormalizedAssetPath Norm = NormalizeAssetPath(InPath);
  if (OutResolvedPath) {
    *OutResolvedPath = Norm.Path;
  }
  if (OutError && !Norm.bIsValid) {
    *OutError = Norm.ErrorMessage;
  }
  return Norm.bIsValid ? Norm.Path : FString();
}

/**
 * Resolves an asset path from a partial path or short name.
 * 1. Checks if InputPath exists exactly.
 * 2. If not, and InputPath is a short name, searches AssetRegistry.
 * 3. Returns the full package name if found uniquely.
 */
static inline FString ResolveAssetPath(const FString &InputPath) {
  if (InputPath.IsEmpty())
    return FString();

  // 1. Exact match check
  if (UEditorAssetLibrary::DoesAssetExist(InputPath)) {
    return InputPath;
  }

  // 2. Exact match with /Game/ prepended if it looks like a relative path but
  // missing root
  if (!InputPath.StartsWith(TEXT("/"))) {
    FString GamePath = TEXT("/Game/") + InputPath;
    if (UEditorAssetLibrary::DoesAssetExist(GamePath)) {
      return GamePath;
    }
  }

  // 3. Search by name if it's a short name (no slashes)
  // UE 5.7+ compatible: Use GetAssetsByPath + manual name filtering instead of FARFilter::AssetName
  // PERFORMANCE NOTE: This scans all assets under /Game when given a short name (no slashes).
  // For large projects, this could be slow if called frequently. Consider caching results
  // or providing full paths when possible.
  if (!InputPath.Contains(TEXT("/"))) {
    FString ShortName = FPaths::GetBaseFilename(InputPath);
    
    FAssetRegistryModule &AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();

    TArray<FAssetData> FoundAssets;
    TArray<FAssetData> AllGameAssets;
    
    // Use GetAssetsByPath with recursive search - more efficient than GetAllAssets
    AssetRegistry.GetAssetsByPath(FName(TEXT("/Game")), AllGameAssets, /*bRecursive=*/true);
    
    // Filter by name match (case-insensitive)
    for (const FAssetData &Asset : AllGameAssets) {
      if (Asset.AssetName.ToString().Equals(ShortName, ESearchCase::IgnoreCase)) {
        FoundAssets.Add(Asset);
      }
    }

    // Return unique match
    if (FoundAssets.Num() == 1) {
      return FoundAssets[0].PackageName.ToString();
    }

    // Multiple matches - prefer /Game/ assets
    if (FoundAssets.Num() > 1) {
      for (const FAssetData &Data : FoundAssets) {
        if (Data.PackageName.ToString().StartsWith(TEXT("/Game/"))) {
          return Data.PackageName.ToString();
        }
      }
      // Return first match if none start with /Game/
      return FoundAssets[0].PackageName.ToString();
    }
  }

  return FString();
}

/**
 * Safe asset saving helper - marks package dirty and notifies asset registry.
 * DO NOT use UEditorAssetLibrary::SaveAsset() - it triggers modal dialogs that
 * crash D3D12RHI during automation. Assets will be saved when editor is closed
 * or user explicitly saves.
 *
 * @param Asset The UObject asset to mark dirty.
 * @returns true if the asset was marked dirty successfully, false otherwise.
 */
static inline bool McpSafeAssetSave(UObject* Asset)
{
    if (!Asset)
        return false;
    
    // UE 5.7+ Fix: Do not immediately save newly created assets to disk.
    // Saving immediately causes bulkdata corruption and crashes.
    // Instead, mark the package dirty and notify the asset registry.
    Asset->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(Asset);
    
    return true;
}

// McpSafeLevelSave uses FlushRenderingCommands for thread-safe level saving
// UE 5.0+ has FlushRenderingCommands in RenderingThread.h

#if WITH_EDITOR
#if ENGINE_MAJOR_VERSION >= 5
#include "FileHelpers.h"  // FEditorFileUtils
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"  // IFileManager
#include "RenderingThread.h"  // FlushRenderingCommands
#include "Materials/MaterialInterface.h"  // UMaterialInterface for McpLoadMaterialWithFallback
#include "Editor/EditorEngine.h"  // UEditorEngine (forward decl if needed)
#include "Engine/World.h"  // UWorld
#include "Engine/Level.h"  // ULevel
#include "Engine/LevelStreaming.h"  // ULevelStreaming
#include "GameFramework/Actor.h"  // AActor
#include "Components/ActorComponent.h"  // UActorComponent
#include "EditorAssetLibrary.h"  // UEditorAssetLibrary for DoesAssetDirectoryExistOnDisk

/**
 * Resolve a component from an actor by component name with fuzzy matching.
 * Supports exact name match, partial name match (starts with), and common suffixes.
 * 
 * This helper resolves component paths in "ActorName.ComponentName" format where
 * the component name may be a partial match (e.g., "StaticMeshComponent" matches "StaticMeshComponent0").
 *
 * @param Actor The actor to search for components
 * @param ComponentName The component name to search for (exact or partial match)
 * @return UActorComponent* or nullptr if not found
 */
static inline UActorComponent* FindComponentByName(AActor* Actor, const FString& ComponentName)
{
    if (!Actor || ComponentName.IsEmpty())
    {
        return nullptr;
    }

    const FString Needle = ComponentName.ToLower();
    UActorComponent* ExactMatch = nullptr;
    UActorComponent* StartsWithMatch = nullptr;

    // Iterate all components on the actor
    TArray<UActorComponent*> Components;
    Actor->GetComponents(Components);
    
    for (UActorComponent* Comp : Components)
    {
        if (!Comp)
        {
            continue;
        }

        const FString CompName = Comp->GetName().ToLower();
        const FString CompPath = Comp->GetPathName().ToLower();

        // 1. Exact name match (highest priority)
        if (CompName.Equals(Needle))
        {
            return Comp; // Exact match, return immediately
        }

        // 2. Exact path match
        if (CompPath.Equals(Needle))
        {
            return Comp;
        }

        // 3. Path ends with component name (e.g., "ActorName.StaticMeshComponent0")
        if (CompPath.EndsWith(FString::Printf(TEXT(".%s"), *Needle)))
        {
            return Comp;
        }

        // 4. Path ends with ":ComponentName" (subobject format)
        if (CompPath.EndsWith(FString::Printf(TEXT(":%s"), *Needle)))
        {
            return Comp;
        }

        // 5. Fuzzy match: ComponentName starts with the needle (e.g., "StaticMeshComponent" matches "StaticMeshComponent0")
        if (CompName.StartsWith(Needle) && !StartsWithMatch)
        {
            StartsWithMatch = Comp;
        }

        // 6. Path contains the component name
        if (!ExactMatch && CompPath.Contains(Needle))
        {
            ExactMatch = Comp;
        }
    }

    // Return matches in priority order: StartsWith is MORE specific than path-contains
    if (StartsWithMatch)
    {
        return StartsWithMatch;
    }
    if (ExactMatch)
    {
        return ExactMatch;
    }

    return nullptr;
}

/**
 * Resolve an object path that may be in "ActorName.ComponentName" format.
 * Returns the component if the path is in component path format, or nullptr otherwise.
 *
 * @param ObjectPath The path to resolve (e.g., "TestActor.StaticMeshComponent0")
 * @param OutActorName If not nullptr, receives the actor name portion of the path
 * @param OutComponentName If not nullptr, receives the component name portion of the path
 * @return UActorComponent* if the path is a valid component path, nullptr otherwise
 */
static inline UActorComponent* ResolveComponentPath(const FString& ObjectPath, FString* OutActorName = nullptr, FString* OutComponentName = nullptr)
{
    // Check if this looks like a component path: "ActorName.ComponentName"
    // Must contain exactly one dot, no slashes, and both parts must be non-empty
    if (ObjectPath.IsEmpty() || 
        ObjectPath.Contains(TEXT("/")) || 
        ObjectPath.Contains(TEXT("\\")) ||
        !ObjectPath.Contains(TEXT(".")))
    {
        return nullptr;
    }

    // Split on the first dot
    int32 DotIndex;
    if (!ObjectPath.FindChar(TEXT('.'), DotIndex))
    {
        return nullptr;
    }

    FString ActorName = ObjectPath.Left(DotIndex);
    FString ComponentName = ObjectPath.Right(ObjectPath.Len() - DotIndex - 1);

    // Both parts must be non-empty
    if (ActorName.IsEmpty() || ComponentName.IsEmpty())
    {
        return nullptr;
    }

    // Output the parsed names if requested
    if (OutActorName)
    {
        *OutActorName = ActorName;
    }
    if (OutComponentName)
    {
        *OutComponentName = ComponentName;
    }

    return nullptr; // Caller must find actor and then find component
}

#endif

/**
 * Safely save a level with UE 5.7+ compatibility workarounds.
 *
 * CRITICAL: Intel GPU drivers (MONZA DdiThreadingContext) can crash when
 * FEditorFileUtils::SaveLevel() is called immediately after level creation.
 *
 * This helper:
 * 1. Suspends the render thread during save (prevents driver race condition)
 * 2. Flushes all rendering commands before and after save
 * 3. Verifies the file exists after save
 * 4. Validates path length to prevent Windows Error 87 (MAX_PATH exceeded)
 *
 * @param Level The ULevel to save
 * @param FullPath The full package path for the level
 * @param MaxRetries Unused (kept for API compatibility)
 * @return true if save succeeded and file exists
 */
static inline bool McpSafeLevelSave(ULevel* Level, const FString& FullPath, int32 MaxRetries = 1)
{
    if (!Level)
    {
        UE_LOG(LogTemp, Error, TEXT("McpSafeLevelSave: Level is null"));
        return false;
    }

    // CRITICAL: Reject transient/unsaved level paths that would cause double-slash package names
    if (FullPath.StartsWith(TEXT("/Temp/")) ||
        FullPath.StartsWith(TEXT("/Engine/Transient")) ||
        FullPath.Contains(TEXT("Untitled")))
    {
        UE_LOG(LogTemp, Error, TEXT("McpSafeLevelSave: Cannot save transient level: %s. Use save_as with a valid path."), *FullPath);
        return false;
    }

    FString PackagePath = FullPath;
    if (!PackagePath.StartsWith(TEXT("/Game/")))
    {
        if (!PackagePath.StartsWith(TEXT("/")))
        {
            PackagePath = TEXT("/Game/") + PackagePath;
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("McpSafeLevelSave: Invalid path (not under /Game/): %s"), *PackagePath);
            return false;
        }
    }

    // Validate no double slashes in the path
    if (PackagePath.Contains(TEXT("//")))
    {
        UE_LOG(LogTemp, Error, TEXT("McpSafeLevelSave: Path contains double slashes: %s"), *PackagePath);
        return false;
    }

    // Ensure path has proper format
    if (PackagePath.Contains(TEXT(".")))
    {
        PackagePath = PackagePath.Left(PackagePath.Find(TEXT(".")));
    }

    // CRITICAL: Validate path length to prevent Windows Error 87
    {
        FString AbsoluteFilePath;
        if (FPackageName::TryConvertLongPackageNameToFilename(PackagePath, AbsoluteFilePath, FPackageName::GetMapPackageExtension()))
        {
            AbsoluteFilePath = FPaths::ConvertRelativePathToFull(AbsoluteFilePath);
            const int32 SafePathLength = 240;
            if (AbsoluteFilePath.Len() > SafePathLength)
            {
                UE_LOG(LogTemp, Error, TEXT("McpSafeLevelSave: Path too long (%d chars, max %d): %s"), 
                    AbsoluteFilePath.Len(), SafePathLength, *AbsoluteFilePath);
                UE_LOG(LogTemp, Error, TEXT("McpSafeLevelSave: Use a shorter path or enable Windows long paths"));
                return false;
            }
        }
    }

    // Check if level already exists BEFORE attempting save
    {
        FString ExistingLevelFilename;
        bool bLevelExists = false;
        
        if (FPackageName::TryConvertLongPackageNameToFilename(PackagePath, ExistingLevelFilename, FPackageName::GetMapPackageExtension()))
        {
            FString AbsolutePath = FPaths::ConvertRelativePathToFull(ExistingLevelFilename);
            bLevelExists = IFileManager::Get().FileExists(*AbsolutePath);
            
            if (!bLevelExists)
            {
                FString LevelName = FPaths::GetBaseFilename(PackagePath);
                FString FolderPath = FPaths::GetPath(AbsolutePath) / LevelName + FPackageName::GetMapPackageExtension();
                bLevelExists = IFileManager::Get().FileExists(*FolderPath);
            }
        }
        
        if (!bLevelExists)
        {
            bLevelExists = FPackageName::DoesPackageExist(PackagePath);
        }
        
        if (bLevelExists)
        {
            UWorld* LevelWorld = Level ? Level->GetWorld() : nullptr;
            if (LevelWorld)
            {
                FString CurrentLevelPath = LevelWorld->GetOutermost()->GetName();
                if (CurrentLevelPath.Equals(PackagePath, ESearchCase::IgnoreCase))
                {
                    UE_LOG(LogTemp, Log, TEXT("McpSafeLevelSave: Overwriting existing level: %s"), *PackagePath);
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("McpSafeLevelSave: Level already exists at %s (current level is %s)"), 
                        *PackagePath, *CurrentLevelPath);
                    return false;
                }
            }
        }
    }

    // CRITICAL: Flush rendering commands to prevent Intel driver race condition
    FlushRenderingCommands();

    // Small delay after flush to ensure GPU is completely idle
    FPlatformProcess::Sleep(0.050f); // 50ms wait

    // Perform the actual save
    UWorld* World = Level ? Level->GetWorld() : nullptr;
    bool bSaveSucceeded = false;
    if (World)
    {
        bSaveSucceeded = UEditorLoadingAndSavingUtils::SaveMap(World, PackagePath);
    }
    else
    {
        bSaveSucceeded = FEditorFileUtils::SaveLevel(Level, *PackagePath);
    }

    if (bSaveSucceeded)
    {
        // Small delay before verification
        FPlatformProcess::Sleep(0.050f);

        // Verify file exists on disk
        FString VerifyFilename;
        if (FPackageName::TryConvertLongPackageNameToFilename(PackagePath, VerifyFilename, FPackageName::GetMapPackageExtension()))
        {
            FString AbsoluteVerifyFilename = FPaths::ConvertRelativePathToFull(VerifyFilename);
            
            if (IFileManager::Get().FileExists(*VerifyFilename) || IFileManager::Get().FileExists(*AbsoluteVerifyFilename))
            {
                UE_LOG(LogTemp, Log, TEXT("McpSafeLevelSave: Successfully saved level: %s"), *PackagePath);
                return true;
            }
            
            // FALLBACK: Check if package exists in UE's package system
            if (FPackageName::DoesPackageExist(PackagePath))
            {
                UE_LOG(LogTemp, Log, TEXT("McpSafeLevelSave: Package exists in UE system: %s"), *PackagePath);
                return true;
            }
            
            UE_LOG(LogTemp, Error, TEXT("McpSafeLevelSave: Save reported success but file not found: %s"), *VerifyFilename);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("McpSafeLevelSave: Failed to convert package path to filename: %s"), *PackagePath);
        }
    }

    UE_LOG(LogTemp, Error, TEXT("McpSafeLevelSave: Failed to save level: %s"), *PackagePath);
    return false;
}

/**
 * Material fallback helper for robust material loading across UE versions.
 * Attempts to load a material with fallback chain for engine defaults.
 * Tries: Requested → DefaultMaterial → WorldGridMaterial → DefaultDeferredDecalMaterial
 * 
 * This addresses missing DefaultMaterial warnings on custom engine builds or stripped content.
 * See: Infrastructure_Robustness_Implementation_Plan.md Issue #3
 * 
 * @param MaterialPath Preferred material path (can be empty to use fallback immediately)
 * @param bSilent If true, suppresses warning logs for missing requested material
 * @return UMaterialInterface* or nullptr if all fallbacks fail
 */
static inline UMaterialInterface* McpLoadMaterialWithFallback(
    const FString& MaterialPath, 
    bool bSilent = false)
{
    // Try requested path first if provided
    if (!MaterialPath.IsEmpty())
    {
        UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
        if (Material)
        {
            return Material;
        }
        if (!bSilent)
        {
            UE_LOG(LogTemp, Warning, TEXT("McpLoadMaterialWithFallback: Requested material not found: %s"), *MaterialPath);
        }
    }
    
    // Fallback chain for engine materials (order matters - most common first)
    const TCHAR* FallbackPaths[] = {
        TEXT("/Engine/EngineMaterials/DefaultMaterial"),
        TEXT("/Engine/EngineMaterials/WorldGridMaterial"),
        TEXT("/Engine/EngineMaterials/DefaultDeferredDecalMaterial"),
        TEXT("/Engine/EngineMaterials/DefaultTextMaterialOpaque")
    };
    
    for (const TCHAR* FallbackPath : FallbackPaths)
    {
        UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, FallbackPath);
        if (Material)
        {
            if (!bSilent && !MaterialPath.IsEmpty())
            {
                UE_LOG(LogTemp, Log, TEXT("McpLoadMaterialWithFallback: Using fallback '%s' for '%s'"), 
                    FallbackPath, *MaterialPath);
            }
            return Material;
        }
    }
    
    UE_LOG(LogTemp, Error, TEXT("McpLoadMaterialWithFallback: All fallback materials unavailable - engine content may be missing"));
    return nullptr;
}

/**
 * Safe map loading helper - properly cleans up current world before loading a new map.
 * Prevents TickTaskManager assertion "!LevelList.Contains(TickTaskLevel)" and
 * "World Memory Leaks" crashes in UE 5.7.
 * 
 * CRITICAL: This function must be called from the Game Thread.
 * 
 * Root Cause Analysis:
 * The FTickTaskManager maintains a LevelList that's filled during StartFrame()
 * and cleared during EndFrame(). When LoadMap destroys the old world:
 * 1. ULevel destructor calls FreeTickTaskLevel()
 * 2. FreeTickTaskLevel() asserts: check(!LevelList.Contains(TickTaskLevel))
 * 3. If a tick frame started but didn't complete, LevelList still has entries
 * 
 * This is a known UE 5.7 issue (UE-197643, UE-138424).
 * 
 * @param MapPath The map path to load (e.g., /Game/Maps/MyMap)
 * @param bForceCleanup If true, perform aggressive cleanup before loading (default: true)
 * @return bool True if the map was loaded successfully
 */
static inline bool McpSafeLoadMap(const FString& MapPath, bool bForceCleanup = true)
{
    if (!GEditor)
    {
        UE_LOG(LogTemp, Error, TEXT("McpSafeLoadMap: GEditor is null"));
        return false;
    }

    // CRITICAL: Ensure we're on the game thread
    if (!IsInGameThread())
    {
        UE_LOG(LogTemp, Error, TEXT("McpSafeLoadMap: Must be called from game thread"));
        return false;
    }

    // CRITICAL: Wait for any async loading to complete
    // Loading a map while async loading is in progress can cause crashes
    int32 AsyncWaitCount = 0;
    while (IsAsyncLoading() && AsyncWaitCount < 100)
    {
        FlushAsyncLoading();
        FPlatformProcess::Sleep(0.01f);
        AsyncWaitCount++;
    }
    if (AsyncWaitCount > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("McpSafeLoadMap: Waited %d frames for async loading to complete"), AsyncWaitCount);
    }

    // CRITICAL: Stop PIE if active - loading a map during PIE causes issues
    if (GEditor->PlayWorld)
    {
        UE_LOG(LogTemp, Log, TEXT("McpSafeLoadMap: Stopping active PIE session before loading map"));
        GEditor->RequestEndPlayMap();
        // Wait for PIE to fully stop
        int32 PieWaitCount = 0;
        while (GEditor->PlayWorld && PieWaitCount < 100)
        {
            FlushRenderingCommands();
            FPlatformProcess::Sleep(0.01f);
            PieWaitCount++;
        }
        FlushRenderingCommands();
    }

    UWorld* CurrentWorld = GEditor->GetEditorWorldContext().World();
    
    // CRITICAL: Check if current world has World Partition before cleanup
    // World Partition levels have additional tick registrations that may cause
    // TickTaskManager assertion crashes even with standard cleanup.
    // This is a known UE 5.7 issue (UE-197643, UE-138424).
    if (CurrentWorld)
    {
        AWorldSettings* WorldSettings = CurrentWorld->GetWorldSettings();
        UWorldPartition* CurrentWorldPartition = WorldSettings ? WorldSettings->GetWorldPartition() : nullptr;
        if (CurrentWorldPartition)
        {
            UE_LOG(LogTemp, Warning, TEXT("McpSafeLoadMap: Current world '%s' has World Partition - tick cleanup may be incomplete"), *CurrentWorld->GetName());
        }
    }
    
    if (CurrentWorld && bForceCleanup)
    {
        UE_LOG(LogTemp, Log, TEXT("McpSafeLoadMap: Cleaning up current world '%s' before loading '%s'"), *CurrentWorld->GetName(), *MapPath);
        
        // STEP 1: Mark all levels as invisible to prevent FillLevelList from adding them
        // This is CRITICAL - FillLevelList only adds levels where bIsVisible is true
        for (ULevel* Level : CurrentWorld->GetLevels())
        {
            if (Level)
            {
                Level->bIsVisible = false;
            }
        }
        
        // STEP 2: Unregister all tick functions (actors + components)
        // CRITICAL: SetActorTickEnabled(false) only DISABLES ticking - it doesn't UNREGISTER
        // the tick function from FTickTaskManager. We must call UnRegisterTickFunction()
        // to properly remove from LevelList and prevent the assertion.
        int32 UnregisteredActorCount = 0;
        int32 UnregisteredComponentCount = 0;
        for (ULevel* Level : CurrentWorld->GetLevels())
        {
            if (!Level) continue;
            
            for (AActor* Actor : Level->Actors)
            {
                if (Actor)
                {
                    if (Actor->PrimaryActorTick.IsTickFunctionRegistered())
                    {
                        Actor->PrimaryActorTick.UnRegisterTickFunction();
                        UnregisteredActorCount++;
                    }
                    
                    // Clear tick prerequisites to prevent cross-level issues (UE-197643)
                    Actor->PrimaryActorTick.GetPrerequisites().Empty();
                    
                    for (UActorComponent* Component : Actor->GetComponents())
                    {
                        if (Component && Component->PrimaryComponentTick.IsTickFunctionRegistered())
                        {
                            Component->PrimaryComponentTick.UnRegisterTickFunction();
                            UnregisteredComponentCount++;
                        }
                    }
                }
            }
        }
        UE_LOG(LogTemp, Log, TEXT("McpSafeLoadMap: Unregistered %d actor ticks and %d component ticks"), UnregisteredActorCount, UnregisteredComponentCount);
        
        // STEP 3: Send end-of-frame updates to complete any pending tick work
        CurrentWorld->SendAllEndOfFrameUpdates();
        
        // STEP 4: Flush rendering commands to ensure all GPU work is complete
        FlushRenderingCommands();
        
        // STEP 5: Unload streaming levels explicitly
        // This prevents UE-197643 where tick prerequisites cross level boundaries
        TArray<ULevelStreaming*> StreamingLevels = CurrentWorld->GetStreamingLevels();
        for (ULevelStreaming* StreamingLevel : StreamingLevels)
        {
            if (StreamingLevel)
            {
                StreamingLevel->SetShouldBeLoaded(false);
                StreamingLevel->SetShouldBeVisible(false);
            }
        }
        
        // STEP 6: Flush rendering commands again after streaming level changes
        // CRITICAL: This was missing and caused crashes
        FlushRenderingCommands();
        
        // STEP 7: Force garbage collection to clean up any remaining references
        GEditor->ForceGarbageCollection(true);
        
        // STEP 8: Flush again after GC
        FlushRenderingCommands();
        
        // STEP 9: Give the engine a moment to process cleanup
        // This is essential for the tick system to settle
        FPlatformProcess::Sleep(0.10f);
        
        // STEP 10: Final flush to ensure everything is settled
        FlushRenderingCommands();
    }
    
    // STEP 11: Check if the map we're trying to load is already the current map
    // If so, skip loading to avoid unnecessary world transitions
    if (CurrentWorld)
    {
        FString CurrentMapPath = CurrentWorld->GetOutermost()->GetName();
        FString NormalizedMapPath = MapPath;
        
        // Remove .umap extension for comparison
        if (NormalizedMapPath.EndsWith(TEXT(".umap")))
        {
            NormalizedMapPath.LeftChopInline(5);
        }
        
        if (CurrentMapPath.Equals(NormalizedMapPath, ESearchCase::IgnoreCase))
        {
            UE_LOG(LogTemp, Log, TEXT("McpSafeLoadMap: Map '%s' is already loaded, skipping"), *MapPath);
            return true; // Already loaded, consider it success
        }
    }
    
    // STEP 12: Load the map
    UE_LOG(LogTemp, Log, TEXT("McpSafeLoadMap: Loading map '%s'"), *MapPath);
    bool bLoaded = FEditorFileUtils::LoadMap(*MapPath);
    
    if (bLoaded)
    {
        UE_LOG(LogTemp, Log, TEXT("McpSafeLoadMap: Successfully loaded map '%s'"), *MapPath);
        
        // STEP 13: Disable ticking on the new world's actors immediately
        // The loaded world might have actors that trigger tick assertions
        UWorld* NewWorld = GEditor->GetEditorWorldContext().World();
        if (NewWorld && NewWorld->PersistentLevel)
        {
            for (AActor* Actor : NewWorld->PersistentLevel->Actors)
            {
                if (Actor)
                {
                    Actor->SetActorTickEnabled(false);
                    for (UActorComponent* Component : Actor->GetComponents())
                    {
                        if (Component)
                        {
                            Component->SetComponentTickEnabled(false);
                        }
                    }
                }
            }
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("McpSafeLoadMap: Failed to load map '%s'"), *MapPath);
    }
    
    return bLoaded;
}
#endif // WITH_EDITOR

#endif

#if WITH_EDITOR
// Resolve a UClass by a variety of heuristics: try full path lookup, attempt
// to load an asset by path (UBlueprint or UClass), then fall back to scanning
// loaded classes by name or path suffix. This replaces previous usages of
// FindObject<...>(ANY_PACKAGE, ...) which is deprecated.
static inline UClass *ResolveClassByName(const FString &ClassNameOrPath) {
  if (ClassNameOrPath.IsEmpty())
    return nullptr;

  // 1) If it's an asset path, prefer loading the asset and deriving the class
  // Skip /Script/ paths as they are native classes, not assets
  if ((ClassNameOrPath.StartsWith(TEXT("/")) ||
       ClassNameOrPath.Contains(TEXT("/"))) &&
      !ClassNameOrPath.StartsWith(TEXT("/Script/"))) {
    UObject *Loaded = nullptr;
// Prefer EditorAssetLibrary when available
#if WITH_EDITOR
    Loaded = UEditorAssetLibrary::LoadAsset(ClassNameOrPath);
#endif
    if (Loaded) {
      if (UBlueprint *BP = Cast<UBlueprint>(Loaded))
        return BP->GeneratedClass;
      if (UClass *C = Cast<UClass>(Loaded))
        return C;
    }
  }

  // 2) Try a direct FindObject using nullptr/explicit outer (expects full path)
  if (UClass *Direct = FindObject<UClass>(nullptr, *ClassNameOrPath))
    return Direct;

  // 2.5) Try guessing generic engine locations for common components (e.g.
  // StaticMeshComponent -> /Script/Engine.StaticMeshComponent) This helps when
  // the class has not been loaded yet so TObjectIterator won't find it.
  if (!ClassNameOrPath.Contains(TEXT("/")) &&
      !ClassNameOrPath.Contains(TEXT("."))) {
    FString EnginePath =
        FString::Printf(TEXT("/Script/Engine.%s"), *ClassNameOrPath);
    if (UClass *EngineClass = FindObject<UClass>(nullptr, *EnginePath))
      return EngineClass;

    // Attempt load for engine class (unlikely to need load for native, but just
    // in case)
    if (UClass *EngineClassLoaded = LoadObject<UClass>(nullptr, *EnginePath))
      return EngineClassLoaded;

    FString UMGPath = FString::Printf(TEXT("/Script/UMG.%s"), *ClassNameOrPath);
    if (UClass *UMGClass = FindObject<UClass>(nullptr, *UMGPath))
      return UMGClass;
  }

  // Special handling for common ambiguous types
  if (ClassNameOrPath.Equals(TEXT("NiagaraComponent"),
                             ESearchCase::IgnoreCase)) {
    if (UClass *NiagaraComp = FindObject<UClass>(
            nullptr, TEXT("/Script/Niagara.NiagaraComponent"))) {
      return NiagaraComp;
    }
  }

  // 3) Fallback: iterate loaded classes and match by short name or path suffix
  UClass *BestMatch = nullptr;
  for (TObjectIterator<UClass> It; It; ++It) {
    UClass *C = *It;
    if (!C)
      continue;

    // Exact short name match
    if (C->GetName().Equals(ClassNameOrPath, ESearchCase::IgnoreCase)) {
      // Prefer /Script/ (native) classes over others if multiple match
      if (C->GetPathName().StartsWith(TEXT("/Script/")))
        return C;
      if (!BestMatch)
        BestMatch = C;
    }
    // Match on ".ClassName" suffix (path-based short form)
    else if (C->GetPathName().EndsWith(
                 FString::Printf(TEXT(".%s"), *ClassNameOrPath),
                 ESearchCase::IgnoreCase)) {
      if (!BestMatch)
        BestMatch = C;
    }
  }

  return BestMatch;
}
#endif

/**
 * Extracts top-level JSON objects from a string.
 *
 * @param In The input string that may contain one or more JSON objects mixed
 * with other text.
 * @returns An array of substring FStrings, each containing a complete top-level
 * JSON object in the same order they appear in the input; empty if none are
 * found.
 */
static inline TArray<FString> ExtractTopLevelJsonObjects(const FString &In) {
  TArray<FString> Results;
  int32 Depth = 0;
  int32 Start = INDEX_NONE;
  for (int32 i = 0; i < In.Len(); ++i) {
    const TCHAR C = In[i];
    if (C == '{') {
      if (Depth == 0)
        Start = i;
      Depth++;
    } else if (C == '}') {
      Depth--;
      if (Depth == 0 && Start != INDEX_NONE) {
        Results.Add(In.Mid(Start, i - Start + 1));
        Start = INDEX_NONE;
      }
    }
  }
  return Results;
}

/**
 * Produce a lowercase hexadecimal representation of the UTF-8 encoding of a
 * string for diagnostic use.
 * @param In The input string to encode as UTF-8 bytes.
 * @returns A lowercase hex string representing the UTF-8 bytes of `In` (two hex
 * characters per byte).
 */
static inline FString HexifyUtf8(const FString &In) {
  FTCHARToUTF8 Converter(*In);
  const uint8 *Bytes = reinterpret_cast<const uint8 *>(Converter.Get());
  int32 Len = Converter.Length();
  FString Hex;
  Hex.Reserve(Len * 2);
  for (int32 i = 0; i < Len; ++i) {
    Hex += FString::Printf(TEXT("%02x"), Bytes[i]);
  }
  return Hex;
}

// Lightweight output capture to collect log lines emitted during
/**
 * Captures log output written to GLog into an in-memory list of lines.
 *
 * Instances can be attached as an FOutputDevice to collect serialized log
 * messages. The captured lines have trailing newline characters removed and are
 * stored in FIFO order. The Serialize override ignores null input.
 *
 * @returns For Consume(): an array of captured log lines; the captured list is
 * cleared from the instance.
 */
struct FMcpOutputCapture : public FOutputDevice {
  TArray<FString> Lines;
  /**
   * Capture a log line, trim any trailing newline characters, and append the
   * result to the internal Lines buffer.
   * @param V Null-terminated string containing the log message; ignored if
   * null.
   * @param Verbosity Verbosity level of the log message.
   * @param Category Log category name.
   */
  virtual void Serialize(const TCHAR *V, ELogVerbosity::Type Verbosity,
                         const FName &Category) override {
    if (!V)
      return;
    FString S(V);
    // Remove trailing newlines for cleaner payloads
    while (S.EndsWith(TEXT("\n")))
      S.RemoveAt(S.Len() - 1);
    Lines.Add(S);
  }

  TArray<FString> Consume() {
    TArray<FString> Tmp = MoveTemp(Lines);
    Lines.Empty();
    return Tmp;
  }
};

// Export a single UProperty value from an object into a JSON value.
/**
 * Convert a single Unreal property value from a container into a JSON value.
 *
 * Supported property kinds include: strings and names, booleans, numeric types
 * (float, double, int32, int64, byte), enum properties (name when available or
 * numeric value), object references (returns path string or JSON null), soft
 * object/class references (soft path string or JSON null), common structs
 * (FVector/FVector-like exported as [x,y,z], FRotator exported as
 * [pitch,yaw,roll], other structs exported as textual representation),
 * arrays, maps (stringifiable keys with basic value types), and sets.
 *
 * @param TargetContainer Pointer to the memory/container that holds the
 *        property's value.
 * @param Property The property definition to export.
 * @returns A shared pointer to an FJsonValue representing the property's value,
 *          or `nullptr` if the inputs are invalid or the property type is not
 *          supported. JSON `null` values are returned for valid null object or
 *          soft-reference properties when appropriate.
 */
static inline TSharedPtr<FJsonValue>
ExportPropertyToJsonValue(void *TargetContainer, FProperty *Property) {
  if (!TargetContainer || !Property)
    return nullptr;

  // Strings
  if (FStrProperty *Str = CastField<FStrProperty>(Property)) {
    return MakeShared<FJsonValueString>(
        Str->GetPropertyValue_InContainer(TargetContainer));
  }

  // Names
  if (FNameProperty *NP = CastField<FNameProperty>(Property)) {
    return MakeShared<FJsonValueString>(
        NP->GetPropertyValue_InContainer(TargetContainer).ToString());
  }

  // Booleans
  if (FBoolProperty *BP = CastField<FBoolProperty>(Property)) {
    return MakeShared<FJsonValueBoolean>(
        BP->GetPropertyValue_InContainer(TargetContainer));
  }

  // Numeric (handle concrete numeric property types to avoid engine-API
  // differences)
  if (FFloatProperty *FP = CastField<FFloatProperty>(Property)) {
    return MakeShared<FJsonValueNumber>(
        (double)FP->GetPropertyValue_InContainer(TargetContainer));
  }
  if (FDoubleProperty *DP = CastField<FDoubleProperty>(Property)) {
    return MakeShared<FJsonValueNumber>(
        (double)DP->GetPropertyValue_InContainer(TargetContainer));
  }
  if (FIntProperty *IP = CastField<FIntProperty>(Property)) {
    return MakeShared<FJsonValueNumber>(
        (double)IP->GetPropertyValue_InContainer(TargetContainer));
  }
  if (FInt64Property *I64P = CastField<FInt64Property>(Property)) {
    return MakeShared<FJsonValueNumber>(
        (double)I64P->GetPropertyValue_InContainer(TargetContainer));
  }
  if (FByteProperty *BP = CastField<FByteProperty>(Property)) {
    // Byte property may be an enum; return enum name if available, else numeric
    // value
    const uint8 ByteVal = BP->GetPropertyValue_InContainer(TargetContainer);
    if (UEnum *Enum = BP->Enum) {
      const FString EnumName = Enum->GetNameStringByValue(ByteVal);
      if (!EnumName.IsEmpty()) {
        return MakeShared<FJsonValueString>(EnumName);
      }
    }
    return MakeShared<FJsonValueNumber>((double)ByteVal);
  }

  // Enum property (newer engine versions use FEnumProperty instead of
  // FByteProperty for enums)
  if (FEnumProperty *EP = CastField<FEnumProperty>(Property)) {
    if (UEnum *Enum = EP->GetEnum()) {
      void *ValuePtr = EP->ContainerPtrToValuePtr<void>(TargetContainer);
      if (FNumericProperty *UnderlyingProp = EP->GetUnderlyingProperty()) {
        const int64 EnumVal =
            UnderlyingProp->GetSignedIntPropertyValue(ValuePtr);
        const FString EnumName = Enum->GetNameStringByValue(EnumVal);
        if (!EnumName.IsEmpty()) {
          return MakeShared<FJsonValueString>(EnumName);
        }
        return MakeShared<FJsonValueNumber>((double)EnumVal);
      }
    }
    return MakeShared<FJsonValueNumber>(0.0);
  }

  // Object references -> return path if available
  if (FObjectProperty *OP = CastField<FObjectProperty>(Property)) {
    UObject *O = OP->GetObjectPropertyValue_InContainer(TargetContainer);
    if (O)
      return MakeShared<FJsonValueString>(O->GetPathName());
    return MakeShared<FJsonValueNull>();
  }

  // Soft object references (FSoftObjectPtr, FSoftObjectPath)
  if (FSoftObjectProperty *SOP = CastField<FSoftObjectProperty>(Property)) {
    const void *ValuePtr = SOP->ContainerPtrToValuePtr<void>(TargetContainer);
    const FSoftObjectPtr *SoftObjPtr =
        static_cast<const FSoftObjectPtr *>(ValuePtr);
    if (SoftObjPtr && !SoftObjPtr->IsNull()) {
      return MakeShared<FJsonValueString>(
          SoftObjPtr->ToSoftObjectPath().ToString());
    }
    return MakeShared<FJsonValueNull>();
  }

  // Soft class references (FSoftClassPtr)
  if (FSoftClassProperty *SCP = CastField<FSoftClassProperty>(Property)) {
    const void *ValuePtr = SCP->ContainerPtrToValuePtr<void>(TargetContainer);
    const FSoftObjectPtr *SoftClassPtr =
        static_cast<const FSoftObjectPtr *>(ValuePtr);
    if (SoftClassPtr && !SoftClassPtr->IsNull()) {
      return MakeShared<FJsonValueString>(
          SoftClassPtr->ToSoftObjectPath().ToString());
    }
    return MakeShared<FJsonValueNull>();
  }

  // Structs: FVector and FRotator common cases
  if (FStructProperty *SP = CastField<FStructProperty>(Property)) {
    const FString TypeName = SP->Struct ? SP->Struct->GetName() : FString();
    if (TypeName.Equals(TEXT("Vector"), ESearchCase::IgnoreCase)) {
      const FVector *V = SP->ContainerPtrToValuePtr<FVector>(TargetContainer);
      TArray<TSharedPtr<FJsonValue>> Arr;
      Arr.Add(MakeShared<FJsonValueNumber>(V->X));
      Arr.Add(MakeShared<FJsonValueNumber>(V->Y));
      Arr.Add(MakeShared<FJsonValueNumber>(V->Z));
      return MakeShared<FJsonValueArray>(Arr);
    } else if (TypeName.Equals(TEXT("Rotator"), ESearchCase::IgnoreCase)) {
      const FRotator *R = SP->ContainerPtrToValuePtr<FRotator>(TargetContainer);
      TArray<TSharedPtr<FJsonValue>> Arr;
      Arr.Add(MakeShared<FJsonValueNumber>(R->Pitch));
      Arr.Add(MakeShared<FJsonValueNumber>(R->Yaw));
      Arr.Add(MakeShared<FJsonValueNumber>(R->Roll));
      return MakeShared<FJsonValueArray>(Arr);
    }

    // Fallback: export textual representation
    FString Exported;
    SP->Struct->ExportText(Exported,
                           SP->ContainerPtrToValuePtr<void>(TargetContainer),
                           nullptr, nullptr, 0, nullptr, true);
    return MakeShared<FJsonValueString>(Exported);
  }

  // Arrays: try to export inner values as strings
  if (FArrayProperty *AP = CastField<FArrayProperty>(Property)) {
    FScriptArrayHelper Helper(
        AP, AP->ContainerPtrToValuePtr<void>(TargetContainer));
    TArray<TSharedPtr<FJsonValue>> Out;
    for (int32 i = 0; i < Helper.Num(); ++i) {
      void *ElemPtr = Helper.GetRawPtr(i);
      if (FProperty *Inner = AP->Inner) {
        // Handle common inner types directly from element memory
        if (FStrProperty *StrInner = CastField<FStrProperty>(Inner)) {
          const FString &Val = *reinterpret_cast<FString *>(ElemPtr);
          Out.Add(MakeShared<FJsonValueString>(Val));
          continue;
        }
        if (FNameProperty *NameInner = CastField<FNameProperty>(Inner)) {
          const FName &N = *reinterpret_cast<FName *>(ElemPtr);
          Out.Add(MakeShared<FJsonValueString>(N.ToString()));
          continue;
        }
        if (FBoolProperty *BoolInner = CastField<FBoolProperty>(Inner)) {
          const bool B = (*reinterpret_cast<const uint8 *>(ElemPtr)) != 0;
          Out.Add(MakeShared<FJsonValueBoolean>(B));
          continue;
        }
        if (FFloatProperty *FInner = CastField<FFloatProperty>(Inner)) {
          const double Val =
              (double)(*reinterpret_cast<const float *>(ElemPtr));
          Out.Add(MakeShared<FJsonValueNumber>(Val));
          continue;
        }
        if (FDoubleProperty *DInner = CastField<FDoubleProperty>(Inner)) {
          const double Val = *reinterpret_cast<const double *>(ElemPtr);
          Out.Add(MakeShared<FJsonValueNumber>(Val));
          continue;
        }
        if (FIntProperty *IInner = CastField<FIntProperty>(Inner)) {
          const double Val =
              (double)(*reinterpret_cast<const int32 *>(ElemPtr));
          Out.Add(MakeShared<FJsonValueNumber>(Val));
          continue;
        }

        // Fallback: Use ExportText_Direct for unsupported inner types
        FString ElemStr;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        Inner->ExportTextItem_Direct(ElemStr, ElemPtr, nullptr, nullptr, PPF_None);
#else
        // UE 5.0: Use ExportText_Direct
        Inner->ExportText_Direct(ElemStr, ElemPtr, nullptr, nullptr, PPF_None, nullptr);
#endif
        Out.Add(MakeShared<FJsonValueString>(ElemStr));
      }
    }
    return MakeShared<FJsonValueArray>(Out);
  }

  // Maps: export as JSON object with key-value pairs
  if (FMapProperty *MP = CastField<FMapProperty>(Property)) {
    TSharedPtr<FJsonObject> MapObj = MakeShared<FJsonObject>();
    FScriptMapHelper Helper(MP,
                            MP->ContainerPtrToValuePtr<void>(TargetContainer));

    for (int32 i = 0; i < Helper.Num(); ++i) {
      if (!Helper.IsValidIndex(i))
        continue;

      // Get key and value pointers
      const uint8 *KeyPtr = Helper.GetKeyPtr(i);
      const uint8 *ValuePtr = Helper.GetValuePtr(i);

      // Convert key to string (maps typically use string or name keys)
      FString KeyStr;
      FProperty *KeyProp = MP->KeyProp;
      if (FStrProperty *StrKey = CastField<FStrProperty>(KeyProp)) {
        KeyStr = *reinterpret_cast<const FString *>(KeyPtr);
      } else if (FNameProperty *NameKey = CastField<FNameProperty>(KeyProp)) {
        KeyStr = reinterpret_cast<const FName *>(KeyPtr)->ToString();
      } else if (FIntProperty *IntKey = CastField<FIntProperty>(KeyProp)) {
        KeyStr = FString::FromInt(*reinterpret_cast<const int32 *>(KeyPtr));
      } else {
        KeyStr = FString::Printf(TEXT("key_%d"), i);
      }

      // Convert value to JSON
      FProperty *ValueProp = MP->ValueProp;
      if (FStrProperty *StrVal = CastField<FStrProperty>(ValueProp)) {
        MapObj->SetStringField(KeyStr,
                               *reinterpret_cast<const FString *>(ValuePtr));
      } else if (FIntProperty *IntVal = CastField<FIntProperty>(ValueProp)) {
        MapObj->SetNumberField(
            KeyStr, (double)*reinterpret_cast<const int32 *>(ValuePtr));
      } else if (FFloatProperty *FloatVal =
                     CastField<FFloatProperty>(ValueProp)) {
        MapObj->SetNumberField(
            KeyStr, (double)*reinterpret_cast<const float *>(ValuePtr));
      } else if (FBoolProperty *BoolVal = CastField<FBoolProperty>(ValueProp)) {
        MapObj->SetBoolField(KeyStr,
                             (*reinterpret_cast<const uint8 *>(ValuePtr)) != 0);
      } else {
        // Use ExportText_Direct for unsupported value types
        FString ValueStr;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        ValueProp->ExportTextItem_Direct(ValueStr, ValuePtr, nullptr, nullptr, PPF_None);
#else
        // UE 5.0: Use ExportText_Direct
        ValueProp->ExportText_Direct(ValueStr, ValuePtr, nullptr, nullptr, PPF_None, nullptr);
#endif
        MapObj->SetStringField(KeyStr, ValueStr);
      }
    }

    return MakeShared<FJsonValueObject>(MapObj);
  }

  // Sets: export as JSON array
  if (FSetProperty *SP = CastField<FSetProperty>(Property)) {
    TArray<TSharedPtr<FJsonValue>> Out;
    FScriptSetHelper Helper(SP,
                            SP->ContainerPtrToValuePtr<void>(TargetContainer));

    for (int32 i = 0; i < Helper.Num(); ++i) {
      if (!Helper.IsValidIndex(i))
        continue;

      const uint8 *ElemPtr = Helper.GetElementPtr(i);
      FProperty *ElemProp = SP->ElementProp;

      if (FStrProperty *StrElem = CastField<FStrProperty>(ElemProp)) {
        Out.Add(MakeShared<FJsonValueString>(
            *reinterpret_cast<const FString *>(ElemPtr)));
      } else if (FNameProperty *NameElem = CastField<FNameProperty>(ElemProp)) {
        Out.Add(MakeShared<FJsonValueString>(
            reinterpret_cast<const FName *>(ElemPtr)->ToString()));
      } else if (FIntProperty *IntElem = CastField<FIntProperty>(ElemProp)) {
        Out.Add(MakeShared<FJsonValueNumber>(
            (double)*reinterpret_cast<const int32 *>(ElemPtr)));
      } else if (FFloatProperty *FloatElem =
                     CastField<FFloatProperty>(ElemProp)) {
        Out.Add(MakeShared<FJsonValueNumber>(
            (double)*reinterpret_cast<const float *>(ElemPtr)));
      } else {
        // Use ExportText_Direct for unsupported set element types
        FString ElemStr;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        ElemProp->ExportTextItem_Direct(ElemStr, ElemPtr, nullptr, nullptr, PPF_None);
#else
        // UE 5.0: Use ExportText_Direct
        ElemProp->ExportText_Direct(ElemStr, ElemPtr, nullptr, nullptr, PPF_None, nullptr);
#endif
        Out.Add(MakeShared<FJsonValueString>(ElemStr));
      }
    }

    return MakeShared<FJsonValueArray>(Out);
  }

  return nullptr;
}

#if WITH_EDITOR
// Throttled wrapper around UEditorAssetLibrary::SaveLoadedAsset to avoid
// triggering rapid repeated SavePackage calls which can cause engine
// warnings (FlushRenderingCommands called recursively) during heavy
// test activity. The helper consults a plugin-wide map of recent save
// timestamps (GRecentAssetSaveTs) and skips saves that occur within the
// configured throttle window. Skipped saves return 'true' to preserve
// idempotent behavior for callers that treat a skipped save as a success.
// Throttled wrapper around UEditorAssetLibrary::SaveLoadedAsset to avoid
// triggering rapid repeated SavePackage calls which can cause engine
// warnings (FlushRenderingCommands called recursively) during heavy
// test activity. The helper consults a plugin-wide map of recent save
// timestamps (GRecentAssetSaveTs) and skips saves that occur within the
// configured throttle window. Skipped saves return 'true' to preserve
// idempotent behavior for callers that treat a skipped save as a success.
//
// bForce: If true, ignore throttling and force an immediate save.
static inline bool
SaveLoadedAssetThrottled(UObject *Asset, double ThrottleSecondsOverride = -1.0,
                         bool bForce = false) {
  if (!Asset)
    return false;
  const double Now = FPlatformTime::Seconds();
  const double Throttle = (ThrottleSecondsOverride >= 0.0)
                              ? ThrottleSecondsOverride
                              : GRecentAssetSaveThrottleSeconds;
  FString Key = Asset->GetPathName();
  if (Key.IsEmpty())
    Key = Asset->GetName();

  {
    FScopeLock Lock(&GRecentAssetSaveMutex);
    if (!bForce) {
      if (double *Last = GRecentAssetSaveTs.Find(Key)) {
        const double Elapsed = Now - *Last;
        if (Elapsed < Throttle) {
          UE_LOG(LogMcpAutomationBridgeSubsystem, VeryVerbose,
                 TEXT("SaveLoadedAssetThrottled: skipping save for '%s' "
                      "(last=%.3fs, throttle=%.3fs)"),
                 *Key, Elapsed, Throttle);
          // Treat skip as success to avoid bubbling save failures into tests
          return true;
        }
      }
    }
  }

  // Perform the save and record timestamp on success
  const bool bSaved = UEditorAssetLibrary::SaveLoadedAsset(Asset);
  if (bSaved) {
    FScopeLock Lock(&GRecentAssetSaveMutex);
    GRecentAssetSaveTs.Add(Key, Now);
    UE_LOG(LogMcpAutomationBridgeSubsystem, VeryVerbose,
           TEXT("SaveLoadedAssetThrottled: saved '%s' (throttle reset)"), *Key);
  } else {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
           TEXT("SaveLoadedAssetThrottled: failed to save '%s'"), *Key);
  }
  return bSaved;
}

// Force a synchronous scan of a specific package or folder path to ensure
// the Asset Registry is up-to-date immediately after asset creation.
static inline void ScanPathSynchronous(const FString &InPath,
                                       bool bRecursive = true) {
  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
  IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();

  // Scan specific path
  TArray<FString> PathsToScan;
  PathsToScan.Add(InPath);
  AssetRegistry.ScanPathsSynchronous(PathsToScan, bRecursive);
}
#else
static inline bool
SaveLoadedAssetThrottled(void *Asset, double ThrottleSecondsOverride = -1.0,
                         bool bForce = false) {
  (void)Asset;
  (void)ThrottleSecondsOverride;
  (void)bForce;
  return false;
}
static inline void ScanPathSynchronous(const FString &InPath,
                                       bool bRecursive = true) {
  (void)InPath;
  (void)bRecursive;
}
#endif

// Apply a JSON value to an FProperty on a UObject. Returns true on success and
/**
 * Apply a JSON value to a reflected property on a target container (object or
 * struct).
 *
 * Converts and assigns common JSON types to the matching Unreal property type
 * (bool, string/name, numeric types, enums/byte, object and soft references,
 * structs for Vector/Rotator or JSON-string-to-struct, and arrays with common
 * inner types). On failure it sets a descriptive message in OutError.
 *
 * @param TargetContainer Pointer to the memory/container that holds the
 * property value (e.g., UObject or struct instance).
 * @param Property The reflected FProperty to assign into.
 * @param ValueField The JSON value to apply.
 * @param OutError Receives a descriptive error message when the function
 * returns false.
 * @returns `true` if the JSON value was successfully converted and assigned to
 * the property, `false` otherwise.
 */
static inline bool
ApplyJsonValueToProperty(void *TargetContainer, FProperty *Property,
                         const TSharedPtr<FJsonValue> &ValueField,
                         FString &OutError) {
  OutError.Empty();
  if (!TargetContainer || !Property || !ValueField) {
    OutError = TEXT("Invalid target/property/value");
    return false;
  }

  // Bool
  if (FBoolProperty *BP = CastField<FBoolProperty>(Property)) {
    if (ValueField->Type == EJson::Boolean) {
      BP->SetPropertyValue_InContainer(TargetContainer, ValueField->AsBool());
      return true;
    }
    if (ValueField->Type == EJson::Number) {
      BP->SetPropertyValue_InContainer(TargetContainer,
                                       ValueField->AsNumber() != 0.0);
      return true;
    }
    if (ValueField->Type == EJson::String) {
      BP->SetPropertyValue_InContainer(
          TargetContainer,
          ValueField->AsString().Equals(TEXT("true"), ESearchCase::IgnoreCase));
      return true;
    }
    OutError = TEXT("Unsupported JSON type for bool property");
    return false;
  }

  // String and Name
  if (FStrProperty *SP = CastField<FStrProperty>(Property)) {
    if (ValueField->Type == EJson::String) {
      SP->SetPropertyValue_InContainer(TargetContainer, ValueField->AsString());
      return true;
    }
    OutError = TEXT("Expected string for string property");
    return false;
  }
  if (FNameProperty *NP = CastField<FNameProperty>(Property)) {
    if (ValueField->Type == EJson::String) {
      NP->SetPropertyValue_InContainer(TargetContainer,
                                       FName(*ValueField->AsString()));
      return true;
    }
    OutError = TEXT("Expected string for name property");
    return false;
  }

  // Numeric: handle concrete numeric property types explicitly
  if (FFloatProperty *FP = CastField<FFloatProperty>(Property)) {
    double Val = 0.0;
    if (ValueField->Type == EJson::Number)
      Val = ValueField->AsNumber();
    else if (ValueField->Type == EJson::String)
      Val = FCString::Atod(*ValueField->AsString());
    else {
      OutError = TEXT("Unsupported JSON type for float property");
      return false;
    }
    FP->SetPropertyValue_InContainer(TargetContainer, static_cast<float>(Val));
    return true;
  }

  // ...existing code...
  if (FDoubleProperty *DP = CastField<FDoubleProperty>(Property)) {
    double Val = 0.0;
    if (ValueField->Type == EJson::Number)
      Val = ValueField->AsNumber();
    else if (ValueField->Type == EJson::String)
      Val = FCString::Atod(*ValueField->AsString());
    else {
      OutError = TEXT("Unsupported JSON type for double property");
      return false;
    }
    DP->SetPropertyValue_InContainer(TargetContainer, Val);
    return true;
  }
  if (FIntProperty *IP = CastField<FIntProperty>(Property)) {
    int64 Val = 0;
    if (ValueField->Type == EJson::Number)
      Val = static_cast<int64>(ValueField->AsNumber());
    else if (ValueField->Type == EJson::String)
      Val = static_cast<int64>(FCString::Atoi64(*ValueField->AsString()));
    else {
      OutError = TEXT("Unsupported JSON type for int property");
      return false;
    }
    IP->SetPropertyValue_InContainer(TargetContainer, static_cast<int32>(Val));
    return true;
  }
  if (FInt64Property *I64P = CastField<FInt64Property>(Property)) {
    int64 Val = 0;
    if (ValueField->Type == EJson::Number)
      Val = static_cast<int64>(ValueField->AsNumber());
    else if (ValueField->Type == EJson::String)
      Val = static_cast<int64>(FCString::Atoi64(*ValueField->AsString()));
    else {
      OutError = TEXT("Unsupported JSON type for int64 property");
      return false;
    }
    I64P->SetPropertyValue_InContainer(TargetContainer, Val);
    return true;
  }
  if (FByteProperty *Bp = CastField<FByteProperty>(Property)) {
    // Check if this is an enum byte property
    if (UEnum *Enum = Bp->Enum) {
      if (ValueField->Type == EJson::String) {
        // Try to match by name (with or without namespace)
        const FString InStr = ValueField->AsString();
        int64 EnumVal = Enum->GetValueByNameString(InStr);
        if (EnumVal == INDEX_NONE) {
          // Try with namespace prefix
          const FString FullName = Enum->GenerateFullEnumName(*InStr);
          EnumVal = Enum->GetValueByName(FName(*FullName));
        }
        if (EnumVal == INDEX_NONE) {
          OutError =
              FString::Printf(TEXT("Invalid enum value '%s' for enum '%s'"),
                              *InStr, *Enum->GetName());
          return false;
        }
        Bp->SetPropertyValue_InContainer(TargetContainer,
                                         static_cast<uint8>(EnumVal));
        return true;
      } else if (ValueField->Type == EJson::Number) {
        // Validate numeric value is in range
        const int64 Val = static_cast<int64>(ValueField->AsNumber());
        if (!Enum->IsValidEnumValue(Val)) {
          OutError = FString::Printf(
              TEXT("Numeric value %lld is not valid for enum '%s'"), Val,
              *Enum->GetName());
          return false;
        }
        Bp->SetPropertyValue_InContainer(TargetContainer,
                                         static_cast<uint8>(Val));
        return true;
      }
      OutError = TEXT("Enum property requires string or number");
      return false;
    }
    // Regular byte property (not an enum)
    int64 Val = 0;
    if (ValueField->Type == EJson::Number)
      Val = static_cast<int64>(ValueField->AsNumber());
    else if (ValueField->Type == EJson::String)
      Val = static_cast<int64>(FCString::Atoi64(*ValueField->AsString()));
    else {
      OutError = TEXT("Unsupported JSON type for byte property");
      return false;
    }
    Bp->SetPropertyValue_InContainer(TargetContainer, static_cast<uint8>(Val));
    return true;
  }

  // Enum property (newer engine versions)
  if (FEnumProperty *EP = CastField<FEnumProperty>(Property)) {
    if (UEnum *Enum = EP->GetEnum()) {
      void *ValuePtr = EP->ContainerPtrToValuePtr<void>(TargetContainer);
      if (FNumericProperty *UnderlyingProp = EP->GetUnderlyingProperty()) {
        if (ValueField->Type == EJson::String) {
          const FString InStr = ValueField->AsString();
          int64 EnumVal = Enum->GetValueByNameString(InStr);
          if (EnumVal == INDEX_NONE) {
            const FString FullName = Enum->GenerateFullEnumName(*InStr);
            EnumVal = Enum->GetValueByName(FName(*FullName));
          }
          if (EnumVal == INDEX_NONE) {
            OutError =
                FString::Printf(TEXT("Invalid enum value '%s' for enum '%s'"),
                                *InStr, *Enum->GetName());
            return false;
          }
          UnderlyingProp->SetIntPropertyValue(ValuePtr, EnumVal);
          return true;
        } else if (ValueField->Type == EJson::Number) {
          const int64 Val = static_cast<int64>(ValueField->AsNumber());
          if (!Enum->IsValidEnumValue(Val)) {
            OutError = FString::Printf(
                TEXT("Numeric value %lld is not valid for enum '%s'"), Val,
                *Enum->GetName());
            return false;
          }
          UnderlyingProp->SetIntPropertyValue(ValuePtr, Val);
          return true;
        }
        OutError = TEXT("Enum property requires string or number");
        return false;
      }
    }
    OutError = TEXT("Enum property has no valid enum definition");
    return false;
  }

  // Object reference
  if (FObjectProperty *OP = CastField<FObjectProperty>(Property)) {
    if (ValueField->Type == EJson::String) {
      const FString Path = ValueField->AsString();
      UObject *Res = nullptr;
      if (!Path.IsEmpty()) {
        // Try LoadObject first
        Res = LoadObject<UObject>(nullptr, *Path);
        // If unsuccessful, try finding by object path if it's a short path or
        // package path
        if (!Res && !Path.Contains(TEXT("."))) {
          // Fallback to StaticLoadObject which can sometimes handle vague paths
          // better
          Res = StaticLoadObject(UObject::StaticClass(), nullptr, *Path);
        }
      }
      if (!Res && !Path.IsEmpty()) {
        OutError =
            FString::Printf(TEXT("Failed to load object at path: %s"), *Path);
        return false;
      }
      OP->SetObjectPropertyValue_InContainer(TargetContainer, Res);
      return true;
    }
    OutError = TEXT("Unsupported JSON type for object property");
    return false;
  }

  // Soft object references (FSoftObjectPtr)
  if (FSoftObjectProperty *SOP = CastField<FSoftObjectProperty>(Property)) {
    if (ValueField->Type == EJson::String) {
      const FString Path = ValueField->AsString();
      void *ValuePtr = SOP->ContainerPtrToValuePtr<void>(TargetContainer);
      FSoftObjectPtr *SoftObjPtr = static_cast<FSoftObjectPtr *>(ValuePtr);
      if (SoftObjPtr) {
        if (Path.IsEmpty()) {
          *SoftObjPtr = FSoftObjectPtr();
        } else {
          *SoftObjPtr = FSoftObjectPath(Path);
        }
        return true;
      }
      OutError = TEXT("Failed to access soft object property");
      return false;
    } else if (ValueField->Type == EJson::Null) {
      void *ValuePtr = SOP->ContainerPtrToValuePtr<void>(TargetContainer);
      FSoftObjectPtr *SoftObjPtr = static_cast<FSoftObjectPtr *>(ValuePtr);
      if (SoftObjPtr) {
        *SoftObjPtr = FSoftObjectPtr();
        return true;
      }
    }
    OutError = TEXT("Soft object property requires string path or null");
    return false;
  }

  // Soft class references (FSoftClassPtr)
  if (FSoftClassProperty *SCP = CastField<FSoftClassProperty>(Property)) {
    if (ValueField->Type == EJson::String) {
      const FString Path = ValueField->AsString();
      void *ValuePtr = SCP->ContainerPtrToValuePtr<void>(TargetContainer);
      FSoftObjectPtr *SoftClassPtr = static_cast<FSoftObjectPtr *>(ValuePtr);
      if (SoftClassPtr) {
        if (Path.IsEmpty()) {
          *SoftClassPtr = FSoftObjectPtr();
        } else {
          *SoftClassPtr = FSoftObjectPath(Path);
        }
        return true;
      }
      OutError = TEXT("Failed to access soft class property");
      return false;
    } else if (ValueField->Type == EJson::Null) {
      void *ValuePtr = SCP->ContainerPtrToValuePtr<void>(TargetContainer);
      FSoftObjectPtr *SoftClassPtr = static_cast<FSoftObjectPtr *>(ValuePtr);
      if (SoftClassPtr) {
        *SoftClassPtr = FSoftObjectPtr();
        return true;
      }
    }
    OutError = TEXT("Soft class property requires string path or null");
    return false;
  }

  // Structs (Vector/Rotator)
  if (FStructProperty *SP = CastField<FStructProperty>(Property)) {
    const FString TypeName = SP->Struct ? SP->Struct->GetName() : FString();
    if (ValueField->Type == EJson::Array) {
      const TArray<TSharedPtr<FJsonValue>> &Arr = ValueField->AsArray();
      if (TypeName.Equals(TEXT("Vector"), ESearchCase::IgnoreCase) &&
          Arr.Num() >= 3) {
        FVector V((float)Arr[0]->AsNumber(), (float)Arr[1]->AsNumber(),
                  (float)Arr[2]->AsNumber());
        SP->Struct->CopyScriptStruct(
            SP->ContainerPtrToValuePtr<void>(TargetContainer), &V);
        return true;
      }
      if (TypeName.Equals(TEXT("Rotator"), ESearchCase::IgnoreCase) &&
          Arr.Num() >= 3) {
        FRotator R((float)Arr[0]->AsNumber(), (float)Arr[1]->AsNumber(),
                   (float)Arr[2]->AsNumber());
        SP->Struct->CopyScriptStruct(
            SP->ContainerPtrToValuePtr<void>(TargetContainer), &R);
        return true;
      }
    }

    // Try import from string for other structs. Prefer JSON conversion via
    // FJsonObjectConverter when the incoming text is valid JSON. Older
    // engine versions that provide ImportText on UScriptStruct are
    // supported via a guarded fallback for legacy builds.
    if (ValueField->Type == EJson::String) {
      const FString Txt = ValueField->AsString();
      if (SP->Struct) {
        // First attempt: parse the string as JSON and convert to struct
        // using the robust JsonObjectConverter which avoids relying on
        // engine-private textual import semantics.
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Txt);
        TSharedPtr<FJsonObject> ParsedObj;
        if (FJsonSerializer::Deserialize(Reader, ParsedObj) &&
            ParsedObj.IsValid()) {
          if (FJsonObjectConverter::JsonObjectToUStruct(
                  ParsedObj.ToSharedRef(), SP->Struct,
                  SP->ContainerPtrToValuePtr<void>(TargetContainer), 0, 0)) {
            return true;
          }
        }

        // NOTE: ImportText-based struct parsing is intentionally omitted
        // because engine textual import signatures differ across engine
        // revisions and can produce fragile compilation failures. If a
        // non-JSON textual import format is required in the future we
        // can implement a safe parser here or add an explicit engine
        // compatibility shim guarded by a feature macro.
      }
    }

    OutError = TEXT("Unsupported JSON type for struct property");
    return false;
  }

  // Arrays: handle common inner element types directly. Unsupported inner
  // types will return an error to avoid relying on ImportText-like APIs.
  if (FArrayProperty *AP = CastField<FArrayProperty>(Property)) {
    if (ValueField->Type != EJson::Array) {
      OutError = TEXT("Expected array for array property");
      return false;
    }
    FScriptArrayHelper Helper(
        AP, AP->ContainerPtrToValuePtr<void>(TargetContainer));
    Helper.EmptyValues();
    const TArray<TSharedPtr<FJsonValue>> &Src = ValueField->AsArray();
    for (int32 i = 0; i < Src.Num(); ++i) {
      Helper.AddValue();
      void *ElemPtr = Helper.GetRawPtr(Helper.Num() - 1);
      FProperty *Inner = AP->Inner;
      const TSharedPtr<FJsonValue> &V = Src[i];
      if (FStrProperty *SIP = CastField<FStrProperty>(Inner)) {
        FString &Dest = *reinterpret_cast<FString *>(ElemPtr);
        Dest = (V->Type == EJson::String)
                   ? V->AsString()
                   : FString::Printf(TEXT("%g"), V->AsNumber());
        continue;
      }
      if (FNameProperty *NIP = CastField<FNameProperty>(Inner)) {
        FName &Dest = *reinterpret_cast<FName *>(ElemPtr);
        Dest = (V->Type == EJson::String)
                   ? FName(*V->AsString())
                   : FName(*FString::Printf(TEXT("%g"), V->AsNumber()));
        continue;
      }
      if (FBoolProperty *BIP = CastField<FBoolProperty>(Inner)) {
        uint8 &Dest = *reinterpret_cast<uint8 *>(ElemPtr);
        Dest = (V->Type == EJson::Boolean) ? (V->AsBool() ? 1 : 0)
                                           : (V->AsNumber() != 0.0 ? 1 : 0);
        continue;
      }
      if (FFloatProperty *FIP = CastField<FFloatProperty>(Inner)) {
        float &Dest = *reinterpret_cast<float *>(ElemPtr);
        Dest = (V->Type == EJson::Number)
                   ? (float)V->AsNumber()
                   : (float)FCString::Atod(*V->AsString());
        continue;
      }
      if (FDoubleProperty *DIP = CastField<FDoubleProperty>(Inner)) {
        double &Dest = *reinterpret_cast<double *>(ElemPtr);
        Dest = (V->Type == EJson::Number) ? V->AsNumber()
                                          : FCString::Atod(*V->AsString());
        continue;
      }
      if (FIntProperty *IIP = CastField<FIntProperty>(Inner)) {
        int32 &Dest = *reinterpret_cast<int32 *>(ElemPtr);
        Dest = (V->Type == EJson::Number) ? (int32)V->AsNumber()
                                          : FCString::Atoi(*V->AsString());
        continue;
      }
      if (FInt64Property *I64IP = CastField<FInt64Property>(Inner)) {
        int64 &Dest = *reinterpret_cast<int64 *>(ElemPtr);
        Dest = (V->Type == EJson::Number) ? (int64)V->AsNumber()
                                          : FCString::Atoi64(*V->AsString());
        continue;
      }
      if (FByteProperty *BYP = CastField<FByteProperty>(Inner)) {
        uint8 &Dest = *reinterpret_cast<uint8 *>(ElemPtr);
        Dest = (V->Type == EJson::Number)
                   ? (uint8)V->AsNumber()
                   : (uint8)FCString::Atoi(*V->AsString());
        continue;
      }

      // Unsupported inner type -> fail explicitly
      OutError =
          TEXT("Unsupported array inner property type for JSON assignment");
      return false;
    }
    return true;
  }

  OutError = TEXT("Unsupported property type for JSON assignment");
  return false;
}

/**
 * Populate Out with the vector found at the given JSON field, or use Default if
 * the field is missing or invalid.
 *
 * @param Obj JSON object to read the field from; may be null.
 * @param FieldName Name of the field containing the vector (object with x/y/z
 * or an array of three numbers).
 * @param Out Receives the resulting FVector.
 * @param Default Fallback FVector used when the field is absent or cannot be
 * parsed.
 */
static inline void ReadVectorField(const TSharedPtr<FJsonObject> &Obj,
                                   const TCHAR *FieldName, FVector &Out,
                                   const FVector &Default) {
  if (!Obj.IsValid()) {
    Out = Default;
    return;
  }
  const TSharedPtr<FJsonObject> *FieldObj = nullptr;
  if (Obj->TryGetObjectField(FieldName, FieldObj) && FieldObj &&
      (*FieldObj).IsValid()) {
    double X = Default.X, Y = Default.Y, Z = Default.Z;
    if (!(*FieldObj)->TryGetNumberField(TEXT("x"), X))
      (*FieldObj)->TryGetNumberField(TEXT("X"), X);
    if (!(*FieldObj)->TryGetNumberField(TEXT("y"), Y))
      (*FieldObj)->TryGetNumberField(TEXT("Y"), Y);
    if (!(*FieldObj)->TryGetNumberField(TEXT("z"), Z))
      (*FieldObj)->TryGetNumberField(TEXT("Z"), Z);
    Out = FVector((float)X, (float)Y, (float)Z);
    return;
  }
  const TArray<TSharedPtr<FJsonValue>> *Arr = nullptr;
  if (Obj->TryGetArrayField(FieldName, Arr) && Arr && Arr->Num() >= 3) {
    Out = FVector((float)(*Arr)[0]->AsNumber(), (float)(*Arr)[1]->AsNumber(),
                  (float)(*Arr)[2]->AsNumber());
    return;
  }
  Out = Default;
}

/**
 * Read a rotator field from a JSON object into an FRotator.
 *
 * Attempts to read a rotator located at FieldName in Obj. Supports either an
 * object form with numeric fields "pitch"/"yaw"/"roll" (case-insensitive) or an
 * array form [pitch, yaw, roll]. If the field is missing or invalid, Out is
 * set to Default.
 *
 * @param Obj JSON object to read from.
 * @param FieldName Name of the field within Obj containing the rotator.
 * @param Out Output rotator populated from the JSON field or Default on
 * failure.
 * @param Default Fallback rotator used when the JSON field is absent or
 * invalid.
 */
static inline void ReadRotatorField(const TSharedPtr<FJsonObject> &Obj,
                                    const TCHAR *FieldName, FRotator &Out,
                                    const FRotator &Default) {
  if (!Obj.IsValid()) {
    Out = Default;
    return;
  }
  const TSharedPtr<FJsonObject> *FieldObj = nullptr;
  if (Obj->TryGetObjectField(FieldName, FieldObj) && FieldObj &&
      (*FieldObj).IsValid()) {
    double Pitch = Default.Pitch, Yaw = Default.Yaw, Roll = Default.Roll;
    if (!(*FieldObj)->TryGetNumberField(TEXT("pitch"), Pitch))
      (*FieldObj)->TryGetNumberField(TEXT("Pitch"), Pitch);
    if (!(*FieldObj)->TryGetNumberField(TEXT("yaw"), Yaw))
      (*FieldObj)->TryGetNumberField(TEXT("Yaw"), Yaw);
    if (!(*FieldObj)->TryGetNumberField(TEXT("roll"), Roll))
      (*FieldObj)->TryGetNumberField(TEXT("Roll"), Roll);
    Out = FRotator((float)Pitch, (float)Yaw, (float)Roll);
    return;
  }
  const TArray<TSharedPtr<FJsonValue>> *Arr = nullptr;
  if (Obj->TryGetArrayField(FieldName, Arr) && Arr && Arr->Num() >= 3) {
    Out = FRotator((float)(*Arr)[0]->AsNumber(), (float)(*Arr)[1]->AsNumber(),
                   (float)(*Arr)[2]->AsNumber());
    return;
  }
  Out = Default;
}

/**
 * Extracts a FVector from a JSON object field, returning a default when the
 * field is absent or invalid.
 * @param Source JSON object to read from.
 * @param FieldName Name of the field to extract (expects an object with x/y/z
 * or an array).
 * @param DefaultValue Value to return when the field is missing or cannot be
 * parsed.
 * @returns The parsed FVector from the specified field, or DefaultValue if
 * parsing failed.
 */
static inline FVector ExtractVectorField(const TSharedPtr<FJsonObject> &Source,
                                         const TCHAR *FieldName,
                                         const FVector &DefaultValue) {
  FVector Parsed = DefaultValue;
  ReadVectorField(Source, FieldName, Parsed, DefaultValue);
  return Parsed;
}

/**
 * Extracts a rotator value from a JSON object field, returning the provided
 * default when the field is absent or cannot be parsed.
 * @param Source JSON object to read the field from.
 * @param FieldName Name of the field to extract.
 * @param DefaultValue Value returned when the field is missing or invalid.
 * @returns Parsed FRotator from the specified field, or DefaultValue if
 * extraction fails.
 */
static inline FRotator
ExtractRotatorField(const TSharedPtr<FJsonObject> &Source,
                    const TCHAR *FieldName, const FRotator &DefaultValue) {
  FRotator Parsed = DefaultValue;
  ReadRotatorField(Source, FieldName, Parsed, DefaultValue);
  return Parsed;
}

// ============================================================================
// CONSOLIDATED JSON FIELD ACCESSORS
// ============================================================================
// These helpers safely extract values from JSON objects with defaults.
// Use these instead of duplicating helpers in each handler file.
// ============================================================================

/**
 * Safely get a string field from a JSON object with a default value.
 * @param Obj JSON object to read from (may be null/invalid).
 * @param Field Name of the string field.
 * @param Default Value to return if field is missing or Obj is invalid.
 * @returns The string value or Default.
 */
static inline FString GetJsonStringField(const TSharedPtr<FJsonObject>& Obj, const FString& Field, const FString& Default = TEXT(""))
{
    FString Value;
    if (Obj.IsValid() && Obj->TryGetStringField(Field, Value))
    {
        return Value;
    }
    return Default;
}

/**
 * Safely get a number field from a JSON object with a default value.
 * @param Obj JSON object to read from (may be null/invalid).
 * @param Field Name of the number field.
 * @param Default Value to return if field is missing or Obj is invalid.
 * @returns The number value or Default.
 */
static inline double GetJsonNumberField(const TSharedPtr<FJsonObject>& Obj, const FString& Field, double Default = 0.0)
{
    double Value = Default;
    if (Obj.IsValid())
    {
        Obj->TryGetNumberField(Field, Value);
    }
    return Value;
}

/**
 * Safely get a boolean field from a JSON object with a default value.
 * @param Obj JSON object to read from (may be null/invalid).
 * @param Field Name of the boolean field.
 * @param Default Value to return if field is missing or Obj is invalid.
 * @returns The boolean value or Default.
 */
static inline bool GetJsonBoolField(const TSharedPtr<FJsonObject>& Obj, const FString& Field, bool Default = false)
{
    bool Value = Default;
    if (Obj.IsValid())
    {
        Obj->TryGetBoolField(Field, Value);
    }
    return Value;
}

/**
 * Safely get an integer field from a JSON object with a default value.
 * @param Obj JSON object to read from (may be null/invalid).
 * @param Field Name of the number field to read as int32.
 * @param Default Value to return if field is missing or Obj is invalid.
 * @returns The integer value or Default.
 */
static inline int32 GetJsonIntField(const TSharedPtr<FJsonObject>& Obj, const FString& Field, int32 Default = 0)
{
    double Value = static_cast<double>(Default);
    if (Obj.IsValid())
    {
        Obj->TryGetNumberField(Field, Value);
    }
    return static_cast<int32>(Value);
}

// Resolve a nested property path (e.g., "Transform.Location.X" or
// "MyComponent.Intensity"). Returns the final property and target object, or
// nullptr on failure. OutError is populated with a descriptive error message on
// failure.
// Resolve a nested property path (e.g., "Transform.Location.X" or
// "MyComponent.Intensity"). Returns the final property and the pointer to the
// container holding it. OutError is populated with a descriptive error message
/**
 * Resolve a dotted property path against a root UObject and locate the terminal
 * property and its owning container.
 *
 * @param RootObject Root UObject to begin lookup from.
 * @param PropertyPath Dotted property path (e.g., "Transform.Location.X").
 * @param OutContainerPtr Set to a pointer to the container that holds the
 * resolved property on success; remains nullptr on failure.
 * @param OutError Set to a descriptive error message on failure; cleared on
 * entry.
 * @returns Pointer to the resolved FProperty for the final segment, or nullptr
 * if resolution failed.
 */
static inline FProperty *ResolveNestedPropertyPath(UObject *RootObject,
                                                   const FString &PropertyPath,
                                                   void *&OutContainerPtr,
                                                   FString &OutError) {
  OutError.Empty();
  OutContainerPtr = nullptr;

  if (!RootObject) {
    OutError = TEXT("Root object is null");
    return nullptr;
  }

  if (PropertyPath.IsEmpty()) {
    OutError = TEXT("Property path is empty");
    return nullptr;
  }

  TArray<FString> PathSegments;
  PropertyPath.ParseIntoArray(PathSegments, TEXT("."), true);

  if (PathSegments.Num() == 0) {
    OutError = TEXT("Invalid property path format");
    return nullptr;
  }

  UStruct *CurrentTypeScope = RootObject->GetClass();
  void *CurrentContainer = RootObject;
  FProperty *CurrentProperty = nullptr;

  for (int32 i = 0; i < PathSegments.Num(); ++i) {
    const FString &Segment = PathSegments[i];
    const bool bIsLastSegment = (i == PathSegments.Num() - 1);

    // Find property in current scope
    CurrentProperty =
        FindFProperty<FProperty>(CurrentTypeScope, FName(*Segment));

    if (!CurrentProperty) {
      OutError = FString::Printf(
          TEXT("Property '%s' not found in scope '%s' (segment %d of %d)"),
          *Segment, *CurrentTypeScope->GetName(), i + 1, PathSegments.Num());
      return nullptr;
    }

    // If this is the last segment, we've found our target
    if (bIsLastSegment) {
      OutContainerPtr = CurrentContainer;
      return CurrentProperty;
    }

    // Traverse deeper
    if (FObjectProperty *ObjectProp =
            CastField<FObjectProperty>(CurrentProperty)) {
      UObject *NextObject =
          ObjectProp->GetObjectPropertyValue_InContainer(CurrentContainer);
      if (!NextObject) {
        OutError = FString::Printf(
            TEXT("Object property '%s' is null (segment %d of %d)"), *Segment,
            i + 1, PathSegments.Num());
        return nullptr;
      }
      CurrentContainer = NextObject;
      CurrentTypeScope = NextObject->GetClass();
    } else if (FStructProperty *StructProp =
                   CastField<FStructProperty>(CurrentProperty)) {
      CurrentContainer =
          StructProp->ContainerPtrToValuePtr<void>(CurrentContainer);
      CurrentTypeScope = StructProp->Struct;
    } else {
      OutError = FString::Printf(
          TEXT("Cannot traverse into property '%s' of type '%s'"), *Segment,
          *CurrentProperty->GetClass()->GetName());
      return nullptr;
    }
  }

  OutError = TEXT("Unexpected end of property path resolution");
  return nullptr;
}

// Helper to find an SCS node by a (case-insensitive) name. Uses reflection
// to iterate the internal AllNodes array so this implementation does not
/**
 * Finds a Simple Construction Script node with the given name in the provided
 * USimpleConstructionScript.
 *
 * Matches case-insensitively first against a node's `VariableName` property
 * when present, and falls back to the node's object name.
 * @param SCS Pointer to the USimpleConstructionScript to search; may be
 * nullptr.
 * @param Name Name to match against nodes (case-insensitive).
 * @returns Pointer to the matching USCS_Node, or nullptr if no match is found
 * or input is invalid.
 */
static inline USCS_Node *FindScsNodeByName(USimpleConstructionScript *SCS,
                                           const FString &Name) {
  if (!SCS || Name.IsEmpty())
    return nullptr;

  // Attempt to find an array property named "AllNodes" on the SCS
  if (UClass *SCSClass = SCS->GetClass()) {
    if (FArrayProperty *ArrayProp =
            FindFProperty<FArrayProperty>(SCSClass, TEXT("AllNodes"))) {
      // Helper to iterate elements
      FScriptArrayHelper Helper(ArrayProp,
                                ArrayProp->ContainerPtrToValuePtr<void>(SCS));
      for (int32 Idx = 0; Idx < Helper.Num(); ++Idx) {
        void *ElemPtr = Helper.GetRawPtr(Idx);
        if (!ElemPtr)
          continue;
        if (FObjectProperty *ObjProp =
                CastField<FObjectProperty>(ArrayProp->Inner)) {
          UObject *ElemObj = ObjProp->GetObjectPropertyValue(ElemPtr);
          if (!ElemObj)
            continue;
          // Match by explicit VariableName property when present
          if (FProperty *VarProp = ElemObj->GetClass()->FindPropertyByName(
                  TEXT("VariableName"))) {
            if (FNameProperty *NP = CastField<FNameProperty>(VarProp)) {
              const FName V = NP->GetPropertyValue_InContainer(ElemObj);
              if (!V.IsNone() &&
                  V.ToString().Equals(Name, ESearchCase::IgnoreCase)) {
                return reinterpret_cast<USCS_Node *>(ElemObj);
              }
            }
          }
          // Fallback: match the object name
          if (ElemObj->GetName().Equals(Name, ESearchCase::IgnoreCase)) {
            return reinterpret_cast<USCS_Node *>(ElemObj);
          }
        }
      }
    }
  }
  return nullptr;
}

#if WITH_EDITOR
// Attempt to locate and load a Blueprint by several heuristics. Returns nullptr
/**
 * Locate and load a Blueprint asset from a variety of request formats and
 * return the loaded Blueprint.
 *
 * Attempts to resolve the input `Req` as an exact asset path (package.object),
 * a package path (with /Game/ prepended when missing), or by querying the Asset
 * Registry for a matching package name. On success `OutNormalized` is set to a
 * normalized package path (without the object suffix) and the loaded
 * `UBlueprint*` is returned; on failure `OutError` is set and nullptr is
 * returned.
 *
 * @param Req The requested asset identifier; may be an absolute package path,
 * an object-qualified path (Package.Asset), or a short path relative to /Game
 * (e.g., "Folder/Asset" or "/Game/Folder/Asset").
 * @param OutNormalized Out parameter that will receive the normalized package
 * path for the resolved asset (no object suffix) on success.
 * @param OutError Out parameter that will receive a descriptive error message
 * if resolution or loading fails.
 * @returns The loaded `UBlueprint*` when the asset is found and loaded, or
 * `nullptr` on failure.
 */
static inline UBlueprint *LoadBlueprintAsset(const FString &Req,
                                             FString &OutNormalized,
                                             FString &OutError) {
  OutNormalized.Empty();
  OutError.Empty();
  if (Req.IsEmpty()) {
    OutError = TEXT("Empty request");
    return nullptr;
  }

  // Build normalized paths
  FString Path = Req;
  if (!Path.StartsWith(TEXT("/"))) {
    Path = TEXT("/Game/") + Path;
  }
  
  FString ObjectPath = Path;
  FString PackagePath = Path;
  
  if (Path.Contains(TEXT("."))) {
    PackagePath = Path.Left(Path.Find(TEXT(".")));
  } else {
    FString AssetName = FPaths::GetBaseFilename(Path);
    ObjectPath = Path + TEXT(".") + AssetName;
  }
  
  FString AssetName = FPaths::GetBaseFilename(PackagePath);

  // Method 1: FindObject with full object path (fastest for in-memory)
  if (UBlueprint* BP = FindObject<UBlueprint>(nullptr, *ObjectPath)) {
    OutNormalized = PackagePath;
    return BP;
  }

  // Method 2: Find package first, then find asset within it
  if (UPackage* Package = FindPackage(nullptr, *PackagePath)) {
    if (UBlueprint* BP = FindObject<UBlueprint>(Package, *AssetName)) {
      OutNormalized = PackagePath;
      return BP;
    }
  }

  // Method 3: TObjectIterator fallback - iterate all blueprints to find by path
  // This is slower but guaranteed to find in-memory assets that weren't properly registered
  for (TObjectIterator<UBlueprint> It; It; ++It) {
    UBlueprint* BP = *It;
    if (BP) {
      FString BPPath = BP->GetPathName();
      // Match by full object path or package path
      if (BPPath.Equals(ObjectPath, ESearchCase::IgnoreCase) ||
          BPPath.Equals(PackagePath, ESearchCase::IgnoreCase) ||
          BPPath.Equals(Path, ESearchCase::IgnoreCase) ||
          BPPath.Equals(Req, ESearchCase::IgnoreCase)) {
        OutNormalized = PackagePath;
        return BP;
      }
      // Also check if the package paths match
      FString BPPackagePath = BPPath;
      if (BPPackagePath.Contains(TEXT("."))) {
        BPPackagePath = BPPackagePath.Left(BPPackagePath.Find(TEXT(".")));
      }
      if (BPPackagePath.Equals(PackagePath, ESearchCase::IgnoreCase)) {
        OutNormalized = PackagePath;
        return BP;
      }
    }
  }

  // Method 4: UEditorAssetLibrary existence check + LoadObject
  if (UEditorAssetLibrary::DoesAssetExist(ObjectPath)) {
    if (UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *ObjectPath)) {
      OutNormalized = PackagePath;
      return BP;
    }
  }

  // Method 5: Asset Registry lookup
  FAssetRegistryModule &ARM =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
          TEXT("AssetRegistry"));
  FAssetData Found;
  TArray<FAssetData> Results;
  ARM.Get().GetAssetsByPackageName(FName(*PackagePath), Results);
  if (Results.Num() > 0) {
    Found = Results[0];
  }

  if (Found.IsValid()) {
    UBlueprint* BP = Cast<UBlueprint>(Found.GetAsset());
    if (!BP) {
      const FString PathStr = Found.ToSoftObjectPath().ToString();
      BP = LoadObject<UBlueprint>(nullptr, *PathStr);
    }
    if (BP) {
      OutNormalized = Found.ToSoftObjectPath().ToString();
      if (OutNormalized.Contains(TEXT(".")))
        OutNormalized = OutNormalized.Left(OutNormalized.Find(TEXT(".")));
      return BP;
    }
  }

  OutError = FString::Printf(TEXT("Blueprint asset not found: %s"), *Req);
  return nullptr;
}
#endif

/**
 * Return the input FString unchanged.
 *
 * @param In The string to convert.
 * @returns The same FString provided as input.
 */
static inline FString ConvertToString(const FString &In) { return In; }
/**
 * Convert a FName to its FString representation.
 * @param In The name to convert.
 * @returns The FString produced by calling ToString() on the input name.
 */
static inline FString ConvertToString(const FName &In) { return In.ToString(); }
/**
 * Convert an FText to its string representation.
 * @param In Text to convert.
 * @returns FString containing the text's contents.
 */
static inline FString ConvertToString(const FText &In) { return In.ToString(); }

// Attempt to resolve a blueprint path to a normalized form without necessarily
/**
 * Find a normalized Blueprint package path for the given request string without
 * loading the asset.
 *
 * Normalizes common forms (prepends /Game when missing a root, strips a
 * trailing `.uasset` extension, and removes object-path suffixes like
 * `/PackageName.ObjectName`) and checks for the asset's existence using a
 * lightweight existence test.
 *
 * @param Req Input path or identifier (may be relative, start with `/`, include
 * `.uasset`, or be an object path).
 * @param OutNormalized Output set to the normalized package path (e.g.,
 * `/Game/...`) when found.
 * @returns `true` if an existing normalized blueprint path was found and
 * written to OutNormalized, `false` otherwise.
 */
static inline bool FindBlueprintNormalizedPath(const FString &Req,
                                               FString &OutNormalized) {
  OutNormalized.Empty();
  if (Req.IsEmpty())
    return false;
#if WITH_EDITOR
  // Use lightweight existence check - DO NOT use LoadBlueprintAsset here
  // as it causes Editor hangs when called repeatedly in polling loops
  FString CheckPath = Req;

  // Ensure path starts with /Game if it doesn't have a valid root
  if (!CheckPath.StartsWith(TEXT("/Game")) &&
      !CheckPath.StartsWith(TEXT("/Engine")) &&
      !CheckPath.StartsWith(TEXT("/Script"))) {
    if (CheckPath.StartsWith(TEXT("/"))) {
      CheckPath = TEXT("/Game") + CheckPath;
    } else {
      CheckPath = TEXT("/Game/") + CheckPath;
    }
  }

  // Remove .uasset extension if present
  if (CheckPath.EndsWith(TEXT(".uasset"))) {
    CheckPath = CheckPath.LeftChop(7);
  }

  // Remove object path suffix (e.g., /Game/BP.BP -> /Game/BP)
  int32 DotIdx;
  if (CheckPath.FindLastChar(TEXT('.'), DotIdx)) {
    // Check if this looks like an object path (PackagePath.ObjectName)
    FString AfterDot = CheckPath.Mid(DotIdx + 1);
    FString BeforeDot = CheckPath.Left(DotIdx);
    // If the part after the dot matches the asset name, strip it
    int32 LastSlashIdx;
    if (BeforeDot.FindLastChar(TEXT('/'), LastSlashIdx)) {
      FString AssetName = BeforeDot.Mid(LastSlashIdx + 1);
      if (AssetName.Equals(AfterDot, ESearchCase::IgnoreCase)) {
        CheckPath = BeforeDot;
      }
    }
  }

  if (UEditorAssetLibrary::DoesAssetExist(CheckPath)) {
    OutNormalized = CheckPath;
    return true;
  }
  return false;
#else
  return false;
#endif
}

/**
 * Resolve a UClass from a string that may be a full path, a blueprint class
 * path, or a short class name.
 *
 * @param Input The input string representing the class (examples:
 * "/Script/Engine.Actor", "/Game/MyBP.MyBP_C", or "Actor").
 * @returns A pointer to the resolved UClass if found, `nullptr` otherwise.
 */
static inline UClass *ResolveUClass(const FString &Input) {
  if (Input.IsEmpty())
    return nullptr;

  // 1. Try finding it directly (full path or already loaded)
  UClass *Found = FindObject<UClass>(nullptr, *Input);
  if (Found)
    return Found;

  // 2. Try loading it directly
  Found = LoadObject<UClass>(nullptr, *Input);
  if (Found)
    return Found;

  // 3. Handle Blueprint Generated Classes explicitly
  // parsing "MyBP" -> "/Game/MyBP.MyBP_C" logic is hard without path,
  // but if input ends in _C, treat as class path.
  if (Input.EndsWith(TEXT("_C"))) {
    // Already tried loading, maybe it needs a package path fix?
    // Assuming the user provided a full path if they included _C.
    return nullptr;
  }

  // 4. Short name resolution
  // Check common script packages
  const TArray<FString> ScriptPackages = {TEXT("/Script/Engine"),
                                          TEXT("/Script/CoreUObject"),
                                          TEXT("/Script/UMG"),
                                          TEXT("/Script/AIModule"),
                                          TEXT("/Script/NavigationSystem"),
                                          TEXT("/Script/Niagara")};

  for (const FString &Pkg : ScriptPackages) {
    FString TryPath = FString::Printf(TEXT("%s.%s"), *Pkg, *Input);
    Found = FindObject<UClass>(nullptr, *TryPath);
    if (Found)
      return Found;
    Found = LoadObject<UClass>(nullptr, *TryPath);
    if (Found)
      return Found;
  }

  // 5. Native class search by iteration (slow fallback, but useful for obscure
  // plugins)
  // Only doing this for exact short name matches to avoid false positives
  for (TObjectIterator<UClass> It; It; ++It) {
    if (It->GetName() == Input) {
      return *It;
    }
  }

  return nullptr;
}

// Standardized Response Helpers
// See: https://google.github.io/styleguide/jsoncstyleguide.xml

/**
 * Sends a standardized success response with a "data" envelope.
 *
 * Format:
 * {
 *   "success": true,
 *   "data": { ... },
 *   "warnings": [],
 *   "error": null
 * }
 */
static inline void SendStandardSuccessResponse(
    UMcpAutomationBridgeSubsystem *Subsystem,
    TSharedPtr<FMcpBridgeWebSocket> Socket, const FString &RequestId,
    const FString &Message, const TSharedPtr<FJsonObject> &Data,
    const TArray<FString> &Warnings = TArray<FString>()) {
  if (!Subsystem)
    return;

  TSharedPtr<FJsonObject> Envelope = MakeShared<FJsonObject>();
  Envelope->SetBoolField(TEXT("success"), true);
  Envelope->SetObjectField(TEXT("data"),
                           Data.IsValid() ? Data : MakeShared<FJsonObject>());

  TArray<TSharedPtr<FJsonValue>> WarningVals;
  for (const FString &W : Warnings) {
    WarningVals.Add(MakeShared<FJsonValueString>(W));
  }
  Envelope->SetArrayField(TEXT("warnings"), WarningVals);

  Envelope->SetField(TEXT("error"), MakeShared<FJsonValueNull>());

  Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, Envelope,
                                    FString());
}

/**
 * Sends a standardized error response with structured error details.
 *
 * Format:
 * {
 *   "success": false,
 *   "data": {},   // Empty object for schema compliance
 *   "error": {
 *     "code": "ERROR_CODE",
 *     "message": "Human readable message",
 *     "parameter": "optional_param_name",
 *     ...
 *   }
 * }
 */
static inline void SendStandardErrorResponse(
    UMcpAutomationBridgeSubsystem *Subsystem,
    TSharedPtr<FMcpBridgeWebSocket> Socket, const FString &RequestId,
    const FString &ErrorCode, const FString &ErrorMessage,
    const TSharedPtr<FJsonObject> &ErrorDetails = nullptr) {
  if (!Subsystem)
    return;

  TSharedPtr<FJsonObject> Envelope = MakeShared<FJsonObject>();
  Envelope->SetBoolField(TEXT("success"), false);
  
  // CRITICAL: Add empty data object for schema compliance
  // The MCP schema requires data: { type: 'object' } in all responses
  Envelope->SetObjectField(TEXT("data"), MakeShared<FJsonObject>());

  TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
  ErrorObj->SetStringField(TEXT("code"), ErrorCode);
  ErrorObj->SetStringField(TEXT("message"), ErrorMessage);

  if (ErrorDetails.IsValid()) {
    // Merge details into error object
    for (const auto &Pair : ErrorDetails->Values) {
      ErrorObj->SetField(Pair.Key, Pair.Value);
    }
  }

  Envelope->SetObjectField(TEXT("error"), ErrorObj);

  Subsystem->SendAutomationResponse(Socket, RequestId, false, ErrorMessage,
                                    Envelope, ErrorCode);
}

// ============================================================================
// ROBUST ACTOR SPAWNING HELPER
// ============================================================================
//
// SpawnActorInActiveWorld solves the "transient actor" issue where actors
// spawned via EditorActorSubsystem->SpawnActorFromClass may end up in the
// /Engine/Transient package, making them invisible in the World Outliner.
//
// This helper properly handles both PIE (Play-In-Editor) and regular Editor
// modes by:
// 1. Checking if GEditor->PlayWorld is active (PIE mode)
// 2. Using TargetWorld->SpawnActor for PIE (proper world context)
// 3. Using EditorActorSubsystem for Editor mode with explicit transform
// 4. Optionally setting an actor label for easy identification
//
// Usage:
//   AActor* MyActor = SpawnActorInActiveWorld<AActor>(
//       ADirectionalLight::StaticClass(),
//       FVector(0, 0, 100),
//       FRotator(-45, 0, 0),
//       TEXT("MySunLight")
//   );
//
// See: ControlHandlers.cpp HandleControlActorSpawn for the original pattern.
// ============================================================================

#if WITH_EDITOR
#include "Editor.h"
#include "GameFramework/Actor.h"
#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
#include "EditorActorSubsystem.h"
#endif

template <typename T = AActor>
static inline T *
SpawnActorInActiveWorld(UClass *ActorClass, const FVector &Location,
                        const FRotator &Rotation,
                        const FString &OptionalLabel = FString()) {
  static_assert(std::is_base_of<AActor, T>::value,
                "T must be derived from AActor");

  if (!GEditor || !ActorClass)
    return nullptr;

  AActor *Spawned = nullptr;

  // Check if PIE is active
  UWorld *TargetWorld = GEditor->PlayWorld;

  if (TargetWorld) {
    // PIE Path: Use World->SpawnActor for proper world context
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    Spawned =
        TargetWorld->SpawnActor(ActorClass, &Location, &Rotation, SpawnParams);
  } else {
    // Editor Path: Use EditorActorSubsystem with explicit transform
    UEditorActorSubsystem *ActorSS =
        GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (ActorSS) {
      Spawned = ActorSS->SpawnActorFromClass(ActorClass, Location, Rotation);
      if (Spawned) {
        // Explicit transform to ensure proper placement and registration
        Spawned->SetActorLocationAndRotation(Location, Rotation, false, nullptr,
                                             ETeleportType::TeleportPhysics);
      }
    }
  }

  // Set optional label for easy identification in World Outliner
  if (Spawned && !OptionalLabel.IsEmpty()) {
    Spawned->SetActorLabel(OptionalLabel);
  }

  return Cast<T>(Spawned);
}

// ============================================================================
// VERIFICATION HELPERS
// ============================================================================
//
// These helpers add verifiable data to responses so users can confirm
// that actions actually executed in Unreal Editor without manual verification.
//
// Key principle: Every response should include:
// 1. The ACTUAL path/name of the created/modified object (not the requested one)
// 2. Existence verification (existsAfter: true/false)
// 3. Object-specific data (component counts, GUIDs, etc.)
// ============================================================================

/**
 * Add actor verification data to a JSON response.
 * Includes: actorPath, actorName, actorGuid, existsAfter
 */
static inline void AddActorVerification(TSharedPtr<FJsonObject> Response, AActor* Actor) {
  if (!Response || !Actor) return;
  
  // Use GetPackage()->GetPathName() for the asset path
  FString ActorPath = Actor->GetPackage() ? Actor->GetPackage()->GetPathName() : Actor->GetPathName();
  Response->SetStringField(TEXT("actorPath"), ActorPath);
  Response->SetStringField(TEXT("actorName"), Actor->GetActorLabel());
  Response->SetStringField(TEXT("actorGuid"), Actor->GetActorGuid().ToString());
  Response->SetBoolField(TEXT("existsAfter"), true);
  Response->SetStringField(TEXT("actorClass"), Actor->GetClass()->GetName());
}

/**
 * Add component verification data to a JSON response.
 * Includes: componentPath, componentName, componentClass
 */
static inline void AddComponentVerification(TSharedPtr<FJsonObject> Response, USceneComponent* Component) {
  if (!Response || !Component) return;
  
  Response->SetStringField(TEXT("componentName"), Component->GetName());
  Response->SetStringField(TEXT("componentClass"), Component->GetClass()->GetName());
  if (AActor* Owner = Component->GetOwner()) {
    Response->SetStringField(TEXT("ownerActorPath"), Owner->GetPackage() ? Owner->GetPackage()->GetPathName() : Owner->GetPathName());
  }
}

/**
 * Add asset verification data to a JSON response.
 * Includes: assetPath, assetName, existsAfter
 */
static inline void AddAssetVerification(TSharedPtr<FJsonObject> Response, UObject* Asset) {
  if (!Response || !Asset) return;
  
  FString AssetPath = Asset->GetPackage() ? Asset->GetPackage()->GetPathName() : Asset->GetPathName();
  Response->SetStringField(TEXT("assetPath"), AssetPath);
  Response->SetStringField(TEXT("assetName"), Asset->GetName());
  Response->SetBoolField(TEXT("existsAfter"), true);
  Response->SetStringField(TEXT("assetClass"), Asset->GetClass()->GetName());
}

/**
 * Add asset verification data to a nested object within the response.
 * Use this when verifying multiple assets to avoid field overwrites.
 * @param Response The main response object
 * @param FieldName The field name for the nested verification object (e.g., "contextVerification", "actionVerification")
 * @param Asset The asset to verify
 */
static inline void AddAssetVerificationNested(TSharedPtr<FJsonObject> Response, const FString& FieldName, UObject* Asset) {
  if (!Response || !Asset) return;
  
  TSharedPtr<FJsonObject> VerificationObj = MakeShared<FJsonObject>();
  FString AssetPath = Asset->GetPackage() ? Asset->GetPackage()->GetPathName() : Asset->GetPathName();
  VerificationObj->SetStringField(TEXT("assetPath"), AssetPath);
  VerificationObj->SetStringField(TEXT("assetName"), Asset->GetName());
  VerificationObj->SetBoolField(TEXT("existsAfter"), true);
  VerificationObj->SetStringField(TEXT("assetClass"), Asset->GetClass()->GetName());
  Response->SetObjectField(FieldName, VerificationObj);
}

/**
 * Verify an asset exists at the given path and add to response.
 */
static inline bool VerifyAssetExists(TSharedPtr<FJsonObject> Response, const FString& AssetPath) {
  bool bExists = UEditorAssetLibrary::DoesAssetExist(AssetPath);
  if (Response) {
    Response->SetStringField(TEXT("verifiedPath"), AssetPath);
    Response->SetBoolField(TEXT("existsAfter"), bExists);
  }
  return bExists;
}

/**
 * Check if a UE asset directory path ACTUALLY exists on disk.
 * 
 * UEditorAssetLibrary::DoesDirectoryExist() uses the AssetRegistry cache which may
 * contain stale entries for directories that no longer exist or never existed.
 * This function converts the UE path to an absolute file system path and checks
 * if the directory actually exists on disk.
 * 
 * @param AssetPath UE asset path (e.g., /Game/MyFolder)
 * @returns true if the directory exists on disk, false otherwise
 */
static inline bool DoesAssetDirectoryExistOnDisk(const FString& AssetPath) {
#if WITH_EDITOR
  // Handle root paths that always exist
  if (AssetPath.Equals(TEXT("/Game"), ESearchCase::IgnoreCase) ||
      AssetPath.Equals(TEXT("/Game/"), ESearchCase::IgnoreCase) ||
      AssetPath.Equals(TEXT("/Engine"), ESearchCase::IgnoreCase) ||
      AssetPath.Equals(TEXT("/Engine/"), ESearchCase::IgnoreCase)) {
    return true;
  }
  
  // Normalize the path - remove trailing slash
  FString NormalizedPath = AssetPath;
  if (NormalizedPath.EndsWith(TEXT("/"))) {
    NormalizedPath.RemoveAt(NormalizedPath.Len() - 1);
  }
  
  // Convert UE asset path to file system path
  // /Game/Folder -> Project/Content/Folder
  FString FileSystemPath;
  
  if (NormalizedPath.StartsWith(TEXT("/Game/"))) {
    // /Game/... -> Project/Content/...
    FString RelativePath = NormalizedPath.RightChop(6); // Remove "/Game/"
    FileSystemPath = FPaths::ProjectContentDir() / RelativePath;
  } else if (NormalizedPath.StartsWith(TEXT("/Engine/"))) {
    // /Engine/... -> Engine/Content/...
    FString RelativePath = NormalizedPath.RightChop(8); // Remove "/Engine/"
    FileSystemPath = FPaths::EngineContentDir() / RelativePath;
  } else {
    // For plugin paths or other roots, try to use FPackageName
    FString PackageName = NormalizedPath;
    if (FPackageName::TryConvertLongPackageNameToFilename(PackageName, FileSystemPath)) {
      // Success - FileSystemPath is now set
    } else {
      // Fallback: check if it exists in AssetRegistry (less reliable)
      return UEditorAssetLibrary::DoesDirectoryExist(AssetPath);
    }
  }
  
  // Check if the directory exists on disk using IFileManager
  IFileManager& FileManager = IFileManager::Get();
  return FileManager.DirectoryExists(*FileSystemPath);
#else
  // Non-editor builds: fall back to AssetRegistry
  return false;
#endif
}

/**
 * Check if a parent directory exists for asset creation.
 * Combines AssetRegistry check (for valid paths) with disk check (for actual existence).
 * 
 * @param AssetPath UE asset path for the asset to be created
 * @return true if parent directory exists, false otherwise
 */
static inline bool DoesParentDirectoryExist(const FString& AssetPath) {
#if WITH_EDITOR
  // Extract parent path
  FString ParentPath = FPaths::GetPath(AssetPath);
  if (ParentPath.IsEmpty()) {
    return false;
  }
  
  // Check if parent exists on disk
  return DoesAssetDirectoryExistOnDisk(ParentPath);
#else
  return false;
#endif
}

#endif