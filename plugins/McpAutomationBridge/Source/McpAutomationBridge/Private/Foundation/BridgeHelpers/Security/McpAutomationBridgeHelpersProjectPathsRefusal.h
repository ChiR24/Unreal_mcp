#pragma once

// The words for a path SanitizeProjectRelativePath refused. Continues McpAutomationBridgeHelpersProjectPaths.h
// (it is included from the end of that header), because every call site that reports a refusal needs the same
// thing: the reason the helper gave, not a guess at one.

#include "CoreMinimal.h"
#include "Misc/PackageName.h"

/**
 * The registered content mounts for a message, without trailing slashes: /Game first, then the ones that start like
 * Near (a mistyped or half-remembered root), then the rest. An editor mounts dozens, and a reply is cut at 512
 * characters, so the list stops at MaxChars and says how many it left out.
 */
static inline FString McpListMountedRoots(const FString &Near = FString(), int32 MaxChars = 240) {
  TArray<FString> Roots;
  FPackageName::QueryRootContentPaths(Roots, /*bIncludeReadOnlyRoots=*/true,
                                      /*bWithoutLeadingSlashes=*/false,
                                      /*bWithoutTrailingSlashes=*/true);
  const auto Rank = [&Near](const FString &Root) {
    if (Root.Equals(TEXT("/Game"), ESearchCase::IgnoreCase)) {
      return 0;
    }
    return Near.Len() >= 3 && Root.RightChop(1).StartsWith(Near.Left(3), ESearchCase::IgnoreCase) ? 1 : 2;
  };
  Roots.Sort([&Rank](const FString &A, const FString &B) {
    const int32 RankA = Rank(A);
    const int32 RankB = Rank(B);
    return RankA != RankB ? RankA < RankB : A < B;
  });
  FString List;
  int32 Shown = 0;
  for (const FString &Root : Roots) {
    if (Shown > 0 && List.Len() + Root.Len() + 2 > MaxChars) {
      break;
    }
    List += Shown > 0 ? FString(TEXT(", ")) + Root : Root;
    ++Shown;
  }
  const int32 More = Roots.Num() - Shown;
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
    // The root is named without its slash, so a reply that hides an unregistered path still shows it. The list
    // ends the message with no full stop: the reply redactor reads a root followed by '.' as a path and hides it.
    const FString Rest = Normalized.RightChop(1);
    int32 Slash = INDEX_NONE;
    const FString Root = Rest.FindChar(TEXT('/'), Slash) ? Rest.Left(Slash) : Rest;
    const FString Named = Root.IsEmpty() ? FString(TEXT("the root of that path")) : FString::Printf(TEXT("'%s'"), *Root);
    return FString::Printf(
        TEXT("Invalid %s '%s': %s is not a mounted content root (its plugin is not enabled, or the name is mistyped). Mounted roots: %s"),
        Label, *RawPath, *Named, *McpListMountedRoots(Root));
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
