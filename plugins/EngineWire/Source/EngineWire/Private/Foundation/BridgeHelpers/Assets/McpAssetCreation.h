#pragma once

// Declares SanitizeProjectRelativePath, used below. Included directly rather
// than relied upon transitively so this header resolves in any unity-build blob.
#include "Foundation/BridgeHelpers/Security/McpProjectPaths.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"
#include "Foundation/McpCapabilityPrincipal.h"
#include "Foundation/BridgeHelpers/Responses/McpResponses.h"
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
    OutError = McpPathRefusalMessage(TEXT("folder path"), FolderPath);
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
  // The folder may be a handler's default (/Game/Enums, /Game/DataTables), chosen after the gate looked.
  if (!McpCapabilityPrincipal::ActiveRequestPermitsPath(OutFullPath)) {
    OutError = FString::Printf(TEXT("%s is outside the caller's allowed paths; name a folder inside them."), *OutFullPath);
    return false;
  }

  return true;
}

// The package path of a new asset named AssetName in the folder RequestedPath (which may already end in the name), or
// false with OutMessage and OutErrorCode: SECURITY_VIOLATION for a folder outside the project's content, INVALID_ARGUMENT
// for a name with spaces at either end, a separator or "..", one that is not an object name, or a path that is not a
// package name. AssetLabel names the asset in the refusals ("color curve"). ValidateAssetCreationPath rewrites a bad
// name instead; this refuses it.
inline bool McpBuildValidatedAssetPath(const FString &RequestedPath, const FString &AssetName, const TCHAR *AssetLabel,
                                       FString &OutPackagePath, FString &OutMessage, FString &OutErrorCode)
{
  const FString SafePath = SanitizeProjectRelativePath(RequestedPath);
  if (SafePath.IsEmpty())
  {
    return McpRefuse(OutMessage, OutErrorCode,
        McpPathRefusalMessage(AssetLabel, RequestedPath), TEXT("SECURITY_VIOLATION"));
  }

  const FString TrimmedName = AssetName.TrimStartAndEnd();
  if (TrimmedName.IsEmpty() || TrimmedName != AssetName)
  {
    return McpRefuse(OutMessage, OutErrorCode,
        FString::Printf(TEXT("Invalid %s name '%s': leading or trailing whitespace is not allowed"), AssetLabel, *AssetName), TEXT("INVALID_ARGUMENT"));
  }
  if (AssetName.Contains(TEXT("..")) || AssetName.Contains(TEXT("/")) || AssetName.Contains(TEXT("\\")))
  {
    return McpRefuse(OutMessage, OutErrorCode,
        FString::Printf(TEXT("Invalid %s name '%s': path separators and traversal sequences are not allowed"), AssetLabel, *AssetName), TEXT("INVALID_ARGUMENT"));
  }

  FText ObjectNameReason;
  if (!FName(*AssetName).IsValidObjectName(ObjectNameReason))
  {
    return McpRefuse(OutMessage, OutErrorCode,
        FString::Printf(TEXT("Invalid %s name '%s': %s"), AssetLabel, *AssetName, *ObjectNameReason.ToString()), TEXT("INVALID_ARGUMENT"));
  }

  OutPackagePath = SafePath;
  if (!FPaths::GetBaseFilename(OutPackagePath).Equals(AssetName, ESearchCase::IgnoreCase))
  {
    OutPackagePath = SafePath / AssetName;
  }
  FText PackageValidationReason;
  if (!FPackageName::IsValidLongPackageName(OutPackagePath, true, &PackageValidationReason))
  {
    return McpRefuse(OutMessage, OutErrorCode,
        FString::Printf(TEXT("Invalid %s package path '%s': %s"), AssetLabel, *OutPackagePath, *PackageValidationReason.ToString()), TEXT("INVALID_ARGUMENT"));
  }
  return true;
}

// The tests IAssetTools::CreateAsset makes before it creates (CanCreateAsset), which answer with a modal dialog at the
// user for a name that is no object name (a space, a dot, a bracket) or a folder nothing may be created under: false
// with the engine's own reason, so a caller refuses before the dialog opens.
inline bool McpAssetToolsCanCreate(const FString &Folder, const FString &Name, FString &OutReason)
{
  FText Reason;
  if (FName(*Name).IsValidObjectName(Reason) && FPackageName::IsValidLongPackageName(Folder / Name, false, &Reason))
  {
    return true;
  }
  OutReason = Reason.ToString();
  return false;
}

// CreatePackage, except that a path the engine would stop the editor on answers null: CreatePackage is a Fatal log for
// a name holding "//" (a folder ending in a slash joined to a name, or a name like "a//b"), and so is the FName under
// it for one past the 1023 characters a name holds. Handlers make their packages through this, never the engine call.
static inline UPackage *McpCreatePackageOrNull(const FString &PackagePath)
{
  return PackagePath.Contains(TEXT("//")) || PackagePath.Len() >= NAME_SIZE ? nullptr : CreatePackage(*PackagePath);
}

// The package for a NEW asset at PackagePath (/Game/Folder/Name), or null with OutError and OutCode when that is not
// possible: ASSET_ALREADY_EXISTS when an asset already lives there, PACKAGE_ERROR when the package cannot be made.
// CreatePackage hands back an existing package, and the NewObject or factory call after it rebuilt that asset in
// place (everything in it lost once saved) or, for another class, brought the editor down.
static inline UPackage *McpCreatePackageForNewAsset(const FString &PackagePath, FString &OutError, FString &OutCode)
{
  // A default folder (/Game/Enums, /Game/AI) or a name joined to it is chosen here, after the gate checked the paths
  // the request named: a path-restricted caller creates only inside its prefixes.
  if (!McpCapabilityPrincipal::ActiveRequestPermitsPath(PackagePath))
  {
    OutError = FString::Printf(TEXT("%s is outside the caller's allowed paths; name a folder inside them. Nothing was created."), *PackagePath);
    OutCode = TEXT("PATH_NOT_PERMITTED");
    return nullptr;
  }
  // CreatePackage is a Fatal log for a name holding "//" (a caller's name joined straight onto a folder: name "a//b")
  // and the FName under it for one past the 1023 characters a name holds, so a name the engine does not take as a
  // package name (the test the editor's own asset creation makes) is refused here, for every caller.
  FText InvalidWhy;
  const bool bTooLong = PackagePath.Len() >= NAME_SIZE;
  if (bTooLong || !FPackageName::IsValidLongPackageName(PackagePath, true, &InvalidWhy))
  {
    OutError = FString::Printf(TEXT("%s is not a package name: %s Nothing was created."), *PackagePath.Left(256),
                               bTooLong ? TEXT("it is longer than the 1023 characters a name holds.") : *InvalidWhy.ToString());
    OutCode = TEXT("INVALID_PATH");
    return nullptr;
  }
  const UPackage *Loaded = FindPackage(nullptr, *PackagePath);
  if (McpAssetExists(PackagePath) || (Loaded && Loaded->FindAssetInPackage()))
  {
    OutError = FString::Printf(TEXT("%s already exists; pick another name or folder, or change that asset instead. Nothing was created."), *PackagePath);
    OutCode = TEXT("ASSET_ALREADY_EXISTS");
    return nullptr;
  }
  UPackage *Package = McpCreatePackageOrNull(PackagePath);
  if (!Package)
  {
    OutError = FString::Printf(TEXT("Could not create the package %s."), *PackagePath);
    OutCode = TEXT("PACKAGE_ERROR");
  }
  return Package;
}
