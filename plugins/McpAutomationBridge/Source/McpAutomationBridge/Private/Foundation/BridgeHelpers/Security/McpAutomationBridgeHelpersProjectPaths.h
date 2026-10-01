#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformFile.h"
#include "HAL/PlatformFileManager.h"
#include "McpAutomationBridgeLog.h"
#include "Misc/EngineVersionComparison.h"

#if PLATFORM_UNIX || PLATFORM_MAC
#include <errno.h>
#include <sys/stat.h>
#endif

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif
#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersAssetPathCanonical.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

/** Why SanitizeProjectRelativePath refused a path. */
enum class EMcpPathRejection : uint8 {
  None,
  Empty,
  WindowsAbsolutePath,
  Traversal,
  NotAMountedRoot,
  // Under a mounted root, but not a valid package path: a character a package name cannot carry.
  InvalidName,
};

/**
 * True when CleanPath is at or under a registered content mount, accepted the way /Game paths are: the bare
 * root ("/MoverExamples"), a folder with a trailing slash, a package path, an object path
 * ("/Plugin/Dir/Name.Name"). FPackageName::IsValidLongPackageName validates a package NAME, so on its own it
 * refused a trailing slash, an object path and a root shorter than four characters, which /Game, /Engine and
 * /Script never were. On a refusal OutReason and OutDetail say whether the root is not mounted or the rest is
 * not a valid package path.
 */
static inline bool McpIsMountedContentPath(const FString &CleanPath,
                                           EMcpPathRejection &OutReason,
                                           FText &OutDetail) {
  FString Probe = FPackageName::ObjectPathToPackageName(CleanPath);
  while (Probe.Len() > 1 && Probe.EndsWith(TEXT("/"))) {
    Probe.LeftChopInline(1);
  }
  int32 Slash = INDEX_NONE;
  const bool bBareRoot = !Probe.RightChop(1).FindChar(TEXT('/'), Slash);
  const FString Root = bBareRoot ? Probe : Probe.Left(Slash + 1);
  const bool bRootMounted = FPackageName::MountPointExists(Root + TEXT("/"));
  if (bRootMounted && (bBareRoot || FPackageName::IsValidLongPackageName(Probe, true, &OutDetail))) {
    return true;
  }
  OutReason = bRootMounted ? EMcpPathRejection::InvalidName : EMcpPathRejection::NotAMountedRoot;
  if (!bRootMounted) {
    OutDetail = NSLOCTEXT("Mcp", "PathRootNotMounted", "No content root with that name is mounted.");
  }
  return false;
}

/**
 * Normalize a project-relative asset path, or return an empty string if it must be refused.
 *
 * OutReason and OutDetail say WHY, without them the refusal paths are indistinguishable to the caller, so an
 * unmounted content root (a plugin that is not enabled, a typo'd root) got reported to the user as a
 * path-traversal violation. OutNormalized gets the path after normalization even when it is refused, so a
 * message can name its root. A refusal is logged only when bLogRefusal: a caller that asks again, to word a
 * message (McpPathRefusalMessage), must not log it twice.
 */
static inline FString McpClassifyProjectPath(
    const FString &InPath, EMcpPathRejection *OutReason, FText *OutDetail,
    FString *OutNormalized, bool bLogRefusal) {
  const auto Refuse = [OutReason, OutDetail](EMcpPathRejection Reason,
                                             const FText &Detail) -> FString {
    if (OutReason)
      *OutReason = Reason;
    if (OutDetail)
      *OutDetail = Detail;
    return FString();
  };
  if (OutReason)
    *OutReason = EMcpPathRejection::None;

  if (InPath.IsEmpty())
    return Refuse(EMcpPathRejection::Empty,
                  NSLOCTEXT("Mcp", "PathEmpty", "The path is empty."));

  FString CleanPath = InPath;

  // Reject Windows absolute paths early (contain drive letter colon)
  if (CleanPath.Len() >= 2 && CleanPath[1] == TEXT(':')) {
    if (bLogRefusal) {
      UE_LOG(
          LogMcpAutomationBridgeSubsystem, Warning,
          TEXT("SanitizeProjectRelativePath: Rejected Windows absolute path: %s"),
          *InPath);
    }
    return Refuse(
        EMcpPathRejection::WindowsAbsolutePath,
        NSLOCTEXT("Mcp", "PathWindowsAbsolute",
                  "The path is an absolute filesystem path; an asset path such "
                  "as /Game/... is required."));
  }

  FPaths::NormalizeFilename(CleanPath);
  // Double slashes crash the engine (/Game//Test).
  McpNormalizeSlashes(CleanPath);

  // Reject paths containing traversal
  if (CleanPath.Contains(TEXT(".."))) {
    if (bLogRefusal) {
      UE_LOG(
          LogMcpAutomationBridgeSubsystem, Warning,
          TEXT("SanitizeProjectRelativePath: Rejected path containing '..': %s"),
          *InPath);
    }
    return Refuse(EMcpPathRejection::Traversal,
                  NSLOCTEXT("Mcp", "PathTraversal",
                            "The path contains a '..' traversal segment."));
  }

  // Ensure path starts with a slash
  if (!CleanPath.StartsWith(TEXT("/"))) {
    CleanPath = TEXT("/") + CleanPath;
  }
  if (OutNormalized)
    *OutNormalized = CleanPath;

  // Whitelist valid roots - MUST start with one of these
  const bool bValidRoot = CleanPath.StartsWith(TEXT("/Game/")) ||
                          CleanPath.StartsWith(TEXT("/Engine/")) ||
                          CleanPath.StartsWith(TEXT("/Script/"));

  // Reject paths that start with / but don't have a valid root
  // This catches paths like /etc/passwd or /invalid/path
  if (!bValidRoot) {
    // Validate against engine's registered mount points (covers all plugin
    // content mounts like /MyGameFeature/, /ShooterCore/, /ALS/, etc.)
    EMcpPathRejection MountReason = EMcpPathRejection::NotAMountedRoot;
    FText MountDetail;
    if (!McpIsMountedContentPath(CleanPath, MountReason, MountDetail)) {
      if (bLogRefusal) {
        UE_LOG(
            LogMcpAutomationBridgeSubsystem, Warning,
            TEXT("SanitizeProjectRelativePath: Rejected path '%s': %s"),
            *InPath, *MountDetail.ToString());
      }
      return Refuse(MountReason, MountDetail);
    }
  }

  return CleanPath;
}

static inline FString SanitizeProjectRelativePath(
    const FString &InPath, EMcpPathRejection *OutReason = nullptr,
    FText *OutDetail = nullptr) {
  return McpClassifyProjectPath(InPath, OutReason, OutDetail, nullptr, true);
}

/** The project directory as a full path ending in '/'. */
static inline FString McpProjectRootDir() {
  FString Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
  FPaths::NormalizeDirectoryName(Root);
  return Root + TEXT("/");
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

  // An absolute path that already points INSIDE the project is the natural way to
  // name a file the caller just looked at on disk, and every caller resolves the
  // result against ProjectDir() anyway. Rebase it to project-relative here so the
  // colon rejection below keeps catching only paths that escape the project.
  if (!FPaths::IsRelative(CleanPath)) {
    FString FullPath = FPaths::ConvertRelativePathToFull(CleanPath);
    FPaths::NormalizeFilename(FullPath);
    const FString ProjectRoot = McpProjectRootDir();
    if (FullPath.StartsWith(ProjectRoot, ESearchCase::IgnoreCase)) {
      CleanPath = FullPath.RightChop(ProjectRoot.Len());
    }
  }

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
  McpNormalizeSlashes(CleanPath);

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

  const FString NormalizedProjectDir = McpProjectRootDir();

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
#elif PLATFORM_WINDOWS
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
#if !(PLATFORM_UNIX || PLATFORM_MAC || PLATFORM_WINDOWS)
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
#if !(PLATFORM_UNIX || PLATFORM_MAC || PLATFORM_WINDOWS)
    OutError = TEXT("SECURITY_VIOLATION: Snapshot path symlink validation is unavailable on this engine version");
    return false;
#endif
#endif
  }

  return true;
}

#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersProjectPathsResolve.h"
#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersProjectPathsRefusal.h"
