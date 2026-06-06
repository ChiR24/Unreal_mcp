#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformFile.h"
#include "HAL/PlatformFileManager.h"
#include "McpAutomationBridgeLog.h"
#include "Misc/EngineVersionComparison.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

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
  const bool bValidRoot = CleanPath.StartsWith(TEXT("/Game/")) ||
                          CleanPath.StartsWith(TEXT("/Engine/")) ||
                          CleanPath.StartsWith(TEXT("/Script/"));

  // Reject paths that start with / but don't have a valid root
  // This catches paths like /etc/passwd or /invalid/path
  if (!bValidRoot) {
    // Validate against engine's registered mount points (covers all plugin
    // content mounts like /MyGameFeature/, /ShooterCore/, /ALS/, etc.)
    FText MountReason;
    if (!FPackageName::IsValidLongPackageName(CleanPath, true, &MountReason)) {
      UE_LOG(
          LogMcpAutomationBridgeSubsystem, Warning,
          TEXT("SanitizeProjectRelativePath: Rejected path '%s': %s"),
          *InPath, *MountReason.ToString());
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

/** Validate native snapshot file paths before file read/write operations. */
static inline bool McpValidateProjectSnapshotFilePath(const FString &AbsolutePath,
                                                      FString &OutError) {
  IPlatformFile &PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

  FString NormalizedAbsolute = FPaths::ConvertRelativePathToFull(AbsolutePath);
  FPaths::NormalizeFilename(NormalizedAbsolute);

  FString NormalizedProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
  FPaths::NormalizeDirectoryName(NormalizedProjectDir);
  if (!NormalizedProjectDir.EndsWith(TEXT("/"))) {
    NormalizedProjectDir += TEXT("/");
  }

  if (!NormalizedAbsolute.StartsWith(NormalizedProjectDir, ESearchCase::IgnoreCase)) {
    OutError = TEXT("SECURITY_VIOLATION: Snapshot path escapes project directory");
    return false;
  }

  FString RelativePath = NormalizedAbsolute.RightChop(NormalizedProjectDir.Len());
  TArray<FString> Segments;
  RelativePath.ParseIntoArray(Segments, TEXT("/"), true);

  FString CurrentPath = NormalizedProjectDir;
  if (CurrentPath.EndsWith(TEXT("/"))) {
    CurrentPath.LeftChopInline(1);
  }

  for (const FString &Segment : Segments) {
    CurrentPath = FPaths::Combine(CurrentPath, Segment);
    FPaths::NormalizeFilename(CurrentPath);

#if PLATFORM_UNIX || PLATFORM_MAC
    struct stat FileInfo;
    if (lstat(TCHAR_TO_UTF8(*CurrentPath), &FileInfo) == 0) {
      if (S_ISLNK(FileInfo.st_mode)) {
        OutError = TEXT("SECURITY_VIOLATION: Snapshot path cannot contain symbolic link components");
        return false;
      }
    } else if (errno != ENOENT) {
      OutError = TEXT("SECURITY_VIOLATION: Snapshot path symlink validation failed");
      return false;
    }
#elif PLATFORM_WINDOWS || MCP_PLATFORM_HOLOLENS
    const uint32 FileAttributes = GetFileAttributesW(*CurrentPath);
    if (FileAttributes != 0xFFFFFFFF && (FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
      OutError = TEXT("SECURITY_VIOLATION: Snapshot path cannot contain symbolic link components");
      return false;
    }
#endif

    if (!PlatformFile.FileExists(*CurrentPath) &&
        !PlatformFile.DirectoryExists(*CurrentPath)) {
      break;
    }

#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1)
#if !(PLATFORM_UNIX || PLATFORM_MAC || PLATFORM_WINDOWS || MCP_PLATFORM_HOLOLENS)
    const ESymlinkResult SymlinkResult = PlatformFile.IsSymlink(*CurrentPath);
    if (SymlinkResult == ESymlinkResult::Symlink) {
      OutError = TEXT("SECURITY_VIOLATION: Snapshot path cannot contain symbolic link components");
      return false;
    }
    if (SymlinkResult == ESymlinkResult::Unimplemented) {
      OutError = TEXT("SECURITY_VIOLATION: Snapshot path symlink validation is unavailable on this platform");
      return false;
    }
#endif
#else
    // UE 5.0 predates IPlatformFile::IsSymlink(). Keep snapshot support usable
    // after the project-directory containment check, while preserving symlink
    // rejection on supported platforms above.
#if !(PLATFORM_UNIX || PLATFORM_MAC || PLATFORM_WINDOWS || MCP_PLATFORM_HOLOLENS)
    OutError = TEXT("SECURITY_VIOLATION: Snapshot path symlink validation is unavailable on this engine version");
    return false;
#endif
#endif
  }

  return true;
}

static inline bool McpResolveProjectFilePath(const FString &ProjectRelativePath,
                                             FString &OutAbsolutePath,
                                             FString &OutError) {
  if (ProjectRelativePath.IsEmpty() || !FPaths::IsRelative(ProjectRelativePath)) {
    OutError = TEXT("SECURITY_VIOLATION: File path must be project-relative");
    return false;
  }

  FString SafeRelativePath = SanitizeProjectFilePath(ProjectRelativePath);
  if (SafeRelativePath.IsEmpty()) {
    OutError = TEXT("SECURITY_VIOLATION: Invalid project-relative file path");
    return false;
  }
  SafeRelativePath.RemoveFromStart(TEXT("/"));

  FString CandidatePath = FPaths::ConvertRelativePathToFull(
      FPaths::Combine(FPaths::ProjectDir(), SafeRelativePath));
  FPaths::NormalizeFilename(CandidatePath);

  if (!McpValidateProjectSnapshotFilePath(CandidatePath, OutError)) {
    return false;
  }

  OutAbsolutePath = MoveTemp(CandidatePath);
  return true;
}
