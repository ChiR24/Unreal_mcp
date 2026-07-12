#include "Domains/Sequence/McpAutomationBridge_SequencePathSecurity.h"

#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

#if PLATFORM_UNIX
#include <sys/stat.h>
#endif

namespace McpSequencePathSecurity {
namespace {

bool HasTraversalSegment(const FString &Path) {
  TArray<FString> Segments;
  FString Normalized = Path;
  Normalized.ReplaceInline(TEXT("\\"), TEXT("/"));
  Normalized.ParseIntoArray(Segments, TEXT("/"), false);
  for (const FString &Segment : Segments) {
    if (Segment == TEXT("..")) {
      return true;
    }
  }
  return false;
}

FString CanonicalDirectory(FString Path) {
  Path.ReplaceInline(TEXT("\\"), TEXT("/"));
  FPaths::NormalizeDirectoryName(Path);
  return Path;
}

FString CanonicalFilePath(FString Path) {
  Path.ReplaceInline(TEXT("\\"), TEXT("/"));
  FPaths::NormalizeFilename(Path);
  return Path;
}

FString ResolveLocalPath(const FString &InputPath) {
  FString Path = InputPath.TrimStartAndEnd();
  Path.ReplaceInline(TEXT("\\"), TEXT("/"));
  if (Path.Equals(TEXT("/Saved"), ESearchCase::IgnoreCase) ||
      Path.StartsWith(TEXT("/Saved/"), ESearchCase::IgnoreCase)) {
    Path = FPaths::Combine(
        FPaths::ProjectSavedDir(),
        Path.Equals(TEXT("/Saved"), ESearchCase::IgnoreCase)
            ? FString()
            : Path.Mid(7));
  } else if (FPaths::IsRelative(Path)) {
    if (Path.StartsWith(TEXT("Content/"))) {
      Path = FPaths::Combine(FPaths::ProjectDir(), Path);
    } else if (Path.StartsWith(TEXT("Saved/"))) {
      Path = FPaths::Combine(FPaths::ProjectDir(), Path);
    } else {
      Path = FPaths::Combine(FPaths::ProjectContentDir(), Path);
    }
  }
  Path = FPaths::ConvertRelativePathToFull(Path);
  FPaths::CollapseRelativeDirectories(Path);
  return CanonicalFilePath(Path);
}

bool IsUnderRoot(const FString &Path, const FString &Root) {
  const FString Candidate = CanonicalFilePath(Path);
  const FString CanonicalRoot = CanonicalDirectory(
      FPaths::ConvertRelativePathToFull(Root));
  const FString RootWithSlash =
      CanonicalRoot.EndsWith(TEXT("/")) ? CanonicalRoot
                                        : CanonicalRoot + TEXT("/");
#if PLATFORM_WINDOWS
  constexpr ESearchCase::Type PathCase = ESearchCase::IgnoreCase;
#else
  constexpr ESearchCase::Type PathCase = ESearchCase::CaseSensitive;
#endif
  return Candidate.Equals(CanonicalRoot, PathCase) ||
         Candidate.StartsWith(RootWithSlash, PathCase);
}

bool IsSymlink(const FString &Path) {
#if PLATFORM_UNIX
  struct stat FileInfo;
  return lstat(TCHAR_TO_UTF8(*Path), &FileInfo) == 0 &&
         S_ISLNK(FileInfo.st_mode);
#else
  return FPlatformFileManager::Get().GetPlatformFile().IsSymlink(*Path) ==
         ESymlinkResult::Symlink;
#endif
}

bool HasSymlinkComponent(const FString &Path) {
  FString Current = Path;
  while (!Current.IsEmpty()) {
    if (IsSymlink(Current)) {
      return true;
    }
    const FString Parent = FPaths::GetPath(Current);
    if (Parent.IsEmpty() || Parent == Current) {
      break;
    }
    Current = Parent;
  }
  return false;
}

void AddAllowedRoots(ELocalPathUse PathUse, TArray<FString> &OutRoots) {
  OutRoots.Add(FPaths::ProjectSavedDir());
  if (PathUse == ELocalPathUse::MediaInput) {
    OutRoots.Add(FPaths::ProjectContentDir());
  }
}

}

bool ValidateLocalPath(const FString &InputPath, ELocalPathUse PathUse,
                       FString &OutResolvedPath, FString &OutError) {
  OutResolvedPath.Reset();
  OutError.Reset();
  if (InputPath.TrimStartAndEnd().IsEmpty()) {
    OutError = TEXT("A non-empty local filesystem path is required.");
    return false;
  }
  if (InputPath.StartsWith(TEXT("file://"), ESearchCase::IgnoreCase)) {
    OutError = TEXT("file:// URLs are not allowed; use a local file path.");
    return false;
  }
  if (HasTraversalSegment(InputPath)) {
    OutError = TEXT("Path traversal is not allowed.");
    return false;
  }

  OutResolvedPath = ResolveLocalPath(InputPath);
  if (HasSymlinkComponent(OutResolvedPath)) {
    OutError = TEXT("Symbolic links are not allowed in local media or render paths.");
    return false;
  }
  TArray<FString> AllowedRoots;
  AddAllowedRoots(PathUse, AllowedRoots);
  for (const FString &Root : AllowedRoots) {
    if (IsUnderRoot(OutResolvedPath, Root)) {
      return true;
    }
  }

  OutError = PathUse == ELocalPathUse::RenderOutput
                 ? TEXT("Render output directories must be under Project Saved.")
                 : TEXT("Media file paths must be under Project Content or Project Saved.");
  return false;
}

bool RevalidateResolvedLocalPath(const FString &ResolvedPath,
                                 ELocalPathUse PathUse, FString &OutError) {
  // Contract: input MUST be an already-resolved absolute filesystem path.
  // Short forms like "/Saved/foo.mp4" or relative paths are NOT supported.
  if (FPaths::IsRelative(ResolvedPath))
  {
    OutError = TEXT("The local path must be an absolute filesystem path.");
    return false;
  }
  FString CurrentResolvedPath;
  if (!ValidateLocalPath(ResolvedPath, PathUse, CurrentResolvedPath, OutError))
    return false;
#if PLATFORM_WINDOWS
  constexpr ESearchCase::Type PathCase = ESearchCase::IgnoreCase;
#else
  constexpr ESearchCase::Type PathCase = ESearchCase::CaseSensitive;
#endif
  if (!CurrentResolvedPath.Equals(CanonicalFilePath(ResolvedPath), PathCase)) {
    OutError = TEXT("The local path changed after validation.");
    return false;
  }
  return true;
}

bool ValidateWritableAssetPath(const FString &InputPath,
                               FString &OutResolvedPath, FString &OutError) {
  OutResolvedPath = InputPath.TrimStartAndEnd();
  OutResolvedPath.ReplaceInline(TEXT("\\"), TEXT("/"));
  OutError.Reset();
  if (OutResolvedPath.IsEmpty() || HasTraversalSegment(OutResolvedPath) ||
      !OutResolvedPath.StartsWith(TEXT("/Game/"),
                                  ESearchCase::CaseSensitive)) {
    OutError = TEXT("Sequence mutations are restricted to /Game assets.");
    return false;
  }
  const FString PackageName =
      FPackageName::ObjectPathToPackageName(OutResolvedPath);
  if (!FPackageName::IsValidLongPackageName(PackageName)) {
    OutError = TEXT("sequencePath must be a valid /Game asset path.");
    return false;
  }
  return true;
}

}
