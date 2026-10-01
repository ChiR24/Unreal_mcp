#pragma once

// The words for a path SanitizeProjectRelativePath refused. Continues McpAutomationBridgeHelpersProjectPaths.h
// (it is included from the end of that header), because every call site that reports a refusal needs the same
// thing: the reason the helper gave, not a guess at one.

#include "CoreMinimal.h"
#include "Misc/PackageName.h"

/** Every registered content mount, /Game first, without trailing slashes, cut after thirty. For a message. */
static inline FString McpListMountedRoots() {
  TArray<FString> Roots;
  FPackageName::QueryRootContentPaths(Roots, /*bIncludeReadOnlyRoots=*/true,
                                      /*bWithoutLeadingSlashes=*/false,
                                      /*bWithoutTrailingSlashes=*/true);
  Roots.Sort();
  Roots.RemoveSingle(FString(TEXT("/Game")));
  Roots.Insert(FString(TEXT("/Game")), 0);
  constexpr int32 MaxListed = 30;
  const int32 More = Roots.Num() - MaxListed;
  if (More > 0) {
    Roots.SetNum(MaxListed);
  }
  const FString List = FString::Join(Roots, TEXT(", "));
  return More > 0 ? FString::Printf(TEXT("%s and %d more"), *List, More) : List;
}

/**
 * The message for a refusal: Label names the field ("assetPath"), RawPath is what the caller sent, and the
 * rest is the reason SanitizeProjectRelativePath gave. A root that is not mounted says so, names the root, and
 * lists the roots that are: that is the one refusal a caller can act on, and it used to read as traversal.
 * Normalized is the helper's OutNormalized, used only to name the root.
 */
static inline FString McpDescribePathRejection(const TCHAR *Label, const FString &RawPath,
                                               EMcpPathRejection Reason, const FText &Detail,
                                               const FString &Normalized = FString()) {
  switch (Reason) {
  case EMcpPathRejection::Empty:
    return FString::Printf(TEXT("Invalid %s: the path is empty."), Label);
  case EMcpPathRejection::WindowsAbsolutePath:
    return FString::Printf(
        TEXT("Invalid %s '%s': an absolute filesystem path is not accepted; use a content path such as /Game/Folder/Name."),
        Label, *RawPath);
  case EMcpPathRejection::Traversal:
    return FString::Printf(TEXT("Invalid %s '%s': the path contains a '..' traversal segment."), Label, *RawPath);
  case EMcpPathRejection::NotAMountedRoot: {
    // The root is named without its slash, so a reply that hides an unregistered path still shows it.
    const FString Rest = Normalized.RightChop(1);
    int32 Slash = INDEX_NONE;
    const FString Root = Rest.FindChar(TEXT('/'), Slash) ? Rest.Left(Slash) : Rest;
    const FString Named = Root.IsEmpty() ? FString(TEXT("the root of that path")) : FString::Printf(TEXT("'%s'"), *Root);
    return FString::Printf(
        TEXT("Invalid %s '%s': %s is not a mounted content root (its plugin is not enabled, or the name is mistyped). Mounted roots: %s."),
        Label, *RawPath, *Named, *McpListMountedRoots());
  }
  case EMcpPathRejection::InvalidName:
    return FString::Printf(TEXT("Invalid %s '%s': %s"), Label, *RawPath, *Detail.ToString());
  default:
    return FString::Printf(TEXT("Invalid %s '%s'."), Label, *RawPath);
  }
}

/** The message for a path SanitizeProjectRelativePath just refused, from the raw value alone; asks again, silently. */
static inline FString McpPathRefusalMessage(const TCHAR *Label, const FString &RawPath) {
  EMcpPathRejection Reason = EMcpPathRejection::None;
  FText Detail;
  FString Normalized;
  McpClassifyProjectPath(RawPath, &Reason, &Detail, &Normalized, /*bLogRefusal=*/false);
  return McpDescribePathRejection(Label, RawPath, Reason, Detail, Normalized);
}
