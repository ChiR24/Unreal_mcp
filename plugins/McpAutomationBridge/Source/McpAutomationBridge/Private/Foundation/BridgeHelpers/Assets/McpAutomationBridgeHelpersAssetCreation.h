#pragma once

// Declares SanitizeProjectRelativePath, used below. Included directly rather
// than relied upon transitively so this header resolves in any unity-build blob.
#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersProjectPaths.h"
#include "ObjectTools.h"

static inline bool IsValidAssetPath(const FString &Path) {
  return !Path.IsEmpty() &&
         Path.StartsWith(TEXT("/")) &&
         !Path.Contains(TEXT("..")) &&
         !Path.Contains(TEXT("//")) &&
         !Path.Contains(TEXT(":"));  // Reject Windows absolute paths
}

// Replaces the engine's invalid object- and package-name characters (and '+')
// with '_', collapses and trims underscores, prefixes a non-letter start with
// "Asset_", and caps the result at 64 characters. Never returns empty.
static inline FString SanitizeAssetName(const FString &InName) {
  FString Sanitized = ObjectTools::SanitizeInvalidChars(
      InName.TrimStartAndEnd(),
      FString(INVALID_OBJECTNAME_CHARACTERS) + INVALID_LONGPACKAGE_CHARACTERS + TEXT("+"));
  while (Sanitized.ReplaceInline(TEXT("__"), TEXT("_")) > 0) {
  }
  while (Sanitized.RemoveFromStart(TEXT("_"))) {
  }
  while (Sanitized.RemoveFromEnd(TEXT("_"))) {
  }
  if (Sanitized.IsEmpty())
    return TEXT("Asset");
  if (!FChar::IsAlpha(Sanitized[0]))
    Sanitized = TEXT("Asset_") + Sanitized;
  return Sanitized.Left(64);
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
