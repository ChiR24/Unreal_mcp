#pragma once

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
