#pragma once

// Project-relative file resolution: reserved device segments and the
// project-directory containment check. Split from
// McpAutomationBridgeHelpersProjectPaths.h, which includes this file at its
// end so every consumer of that header keeps the full surface. It relies on
// the sanitizers and includes declared there: include the parent header, not
// this one.

/**
 * Windows reserves device names in EVERY path segment, not just the leaf: ".../NUL/logo.png" is a device path
 * even though its filename is innocent. Mirrors IsWindowsReservedFilename in the snapshot module, which lives
 * in a .cpp and is therefore not linkable from this header.
 */
static inline bool McpIsReservedDeviceSegment(const FString &InSegment) {
  FString Leaf = InSegment;
  int32 DotAt = INDEX_NONE;
  if (Leaf.FindChar(TEXT('.'), DotAt)) {
    Leaf.LeftInline(DotAt);
  }
  Leaf.TrimStartAndEndInline();
  static const TCHAR *ReservedNames[] = {TEXT("CON"), TEXT("PRN"), TEXT("AUX"), TEXT("NUL")};
  for (const TCHAR *Reserved : ReservedNames) {
    if (Leaf.Equals(Reserved, ESearchCase::IgnoreCase)) {
      return true;
    }
  }
  if (Leaf.Len() == 4 && FChar::IsDigit(Leaf[3]) && Leaf[3] != TEXT('0')) {
    return Leaf.StartsWith(TEXT("COM"), ESearchCase::IgnoreCase) ||
           Leaf.StartsWith(TEXT("LPT"), ESearchCase::IgnoreCase);
  }
  return false;
}

static inline bool McpResolveProjectFilePath(const FString &ProjectRelativePath,
                                             FString &OutAbsolutePath,
                                             FString &OutError) {
  if (ProjectRelativePath.IsEmpty()) {
    OutError = TEXT("SECURITY_VIOLATION: File path must be project-relative "
                    "(received an empty path)");
    return false;
  }

  FString CandidatePath;
  const bool bWasAbsolute = !FPaths::IsRelative(ProjectRelativePath);

  if (bWasAbsolute) {
    // Absolute input is accepted only when it resolves *inside* the project
    // directory. Containment (McpValidateProjectSnapshotFilePath, below) is
    // the real security boundary here, so an absolute path that lands under
    // ProjectDir is exactly as safe as the project-relative spelling of the
    // same file. Callers routinely have absolute paths on hand (OS file
    // pickers, drag-and-drop, other tools' output); rejecting them outright
    // forced the caller to hand-convert a path we can normalize ourselves.
    CandidatePath = ProjectRelativePath;
    CandidatePath.ReplaceInline(TEXT("\\"), TEXT("/"));
    CandidatePath = FPaths::ConvertRelativePathToFull(CandidatePath);

    // Collapse here (absolute input only); the shared segment walk below then checks what survived.
    if (!FPaths::CollapseRelativeDirectories(CandidatePath)) {
      OutError = TEXT("SECURITY_VIOLATION: File path contains unresolved "
                      "parent-directory traversal");
      return false;
    }

  } else {
    FString SafeRelativePath = SanitizeProjectFilePath(ProjectRelativePath);
    if (SafeRelativePath.IsEmpty()) {
      OutError = TEXT("SECURITY_VIOLATION: Invalid project-relative file path");
      return false;
    }
    SafeRelativePath.RemoveFromStart(TEXT("/"));

    CandidatePath = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(FPaths::ProjectDir(), SafeRelativePath));
  }

  FPaths::NormalizeFilename(CandidatePath);

  // Applied to EVERY segment and to BOTH branches, so "Content/NUL" and its absolute-path equivalent can never get
  // opposite answers for the same file -- they did while this lived inside the absolute branch only.
  {
    TArray<FString> Segments;
    CandidatePath.ParseIntoArray(Segments, TEXT("/"), true);
    for (const FString &Segment : Segments) {
      if (Segment == TEXT("..")) {
        OutError = TEXT("SECURITY_VIOLATION: File path contains unresolved "
                        "parent-directory traversal");
        return false;
      }
      if (McpIsReservedDeviceSegment(Segment)) {
        OutError = TEXT("SECURITY_VIOLATION: File path uses a reserved device name");
        return false;
      }
    }
  }

  if (!McpValidateProjectSnapshotFilePath(CandidatePath, OutError)) {
    if (bWasAbsolute) {
      // The generic containment message ("Snapshot path escapes project
      // directory") tells the caller nothing about how to spell a path we
      // would accept. Say where the project is and show a usable example.
      FString ProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
      FPaths::NormalizeDirectoryName(ProjectDir);
      OutError = FString::Printf(
          TEXT("SECURITY_VIOLATION: File path must resolve inside the project "
               "directory ('%s'), but '%s' resolves outside it. Pass a "
               "project-relative path (e.g. 'Content/Icons/Logo.png') or an "
               "absolute path under the project directory."),
          *ProjectDir, *ProjectRelativePath);
    }
    return false;
  }

  OutAbsolutePath = MoveTemp(CandidatePath);
  return true;
}
