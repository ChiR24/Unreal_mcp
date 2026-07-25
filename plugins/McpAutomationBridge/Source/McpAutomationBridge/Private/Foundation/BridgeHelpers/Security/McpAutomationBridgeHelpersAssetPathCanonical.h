#pragma once

#include "CoreMinimal.h"

// THE single place the `/Content` content-root alias is mapped onto `/Game`, and
// the single canonical form of a client-supplied UE content path.
//
// Why this exists: the pre-queue security gate and the post-queue handlers MUST
// agree on what string a payload value denotes. Before this header the handlers
// canonicalized (`/Content` -> `/Game`, backslash -> slash, bare-relative ->
// `/Game/...`) while the gate compared the RAW value against a literal prefix
// list, so `/Content/TeamB` was invisible to path confinement and then resolved
// to `/Game/TeamB` by the handler. Both sides now call this function, so an
// alias can no longer mean one thing to the guard and another to the executor.
//
// Ordering is normalize -> reject -> classify, matching
// `McpAutomationBridgeHelpersProjectPaths.h`. That header stays the post-queue
// root/mount validation (it consults `FPackageName` and logs); this one is
// allocation-light, silent and free of engine mount state, so it is also safe to
// run on a socket thread for every string in a hostile payload.

namespace McpAssetPathCanonical
{
/** Mount roots a canonical UE object path can start at. */
inline bool IsUnrealRoot(const FString& Path)
{
	static const TCHAR* const Roots[] = {
		TEXT("/Game"), TEXT("/Engine"), TEXT("/Script"), TEXT("/Temp"), TEXT("/Niagara")
	};
	for (const TCHAR* const Root : Roots)
	{
		const FString RootText(Root);
		if (Path.Equals(RootText, ESearchCase::IgnoreCase) ||
			Path.StartsWith(RootText + TEXT("/"), ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	return false;
}

/** Replace a leading `/Content` root with `/Game`, on a segment boundary only. */
inline void MapContentRootInline(FString& Path)
{
	const FString Alias(TEXT("/Content"));
	if (!Path.StartsWith(Alias, ESearchCase::IgnoreCase))
	{
		return;
	}
	// Boundary-aware: `/Content` and `/Content/...` are the content root;
	// `/ContentOther` is a different folder and must not be rewritten.
	if (Path.Len() == Alias.Len() || Path[Alias.Len()] == TEXT('/'))
	{
		Path = TEXT("/Game") + Path.RightChop(Alias.Len());
	}
}
} // namespace McpAssetPathCanonical

/**
 * Canonical `/Game`-rooted form of a client-supplied content path, or an empty
 * string when the value is not a content path (or is one the engine must never
 * accept: traversal, or a drive-letter/object-suffix colon).
 *
 * bAssumeGameRoot mirrors the handlers that prepend `/Game` to a bare relative
 * path; pass it wherever the executor would do the same.
 */
inline FString McpCanonicalizeContentPath(const FString& InPath, bool bAssumeGameRoot = false)
{
	FString Path = InPath.TrimStartAndEnd();
	if (Path.IsEmpty())
	{
		return FString();
	}

	Path.ReplaceInline(TEXT("\\"), TEXT("/"));
	while (Path.Contains(TEXT("//")))
	{
		Path = Path.Replace(TEXT("//"), TEXT("/"));
	}

	McpAssetPathCanonical::MapContentRootInline(Path);

	if (bAssumeGameRoot && !Path.StartsWith(TEXT("/")))
	{
		Path = TEXT("/Game/") + Path;
	}

	// Rejected rather than canonicalized: a traversal segment or a colon can
	// change what the path resolves to after this function returns, so no
	// canonical form of it is trustworthy.
	if (Path.Contains(TEXT("..")) || Path.Contains(TEXT(":")))
	{
		return FString();
	}

	while (Path.EndsWith(TEXT("/")) && Path.Len() > 1)
	{
		Path.LeftChopInline(1);
	}

	return McpAssetPathCanonical::IsUnrealRoot(Path) ? Path : FString();
}

/**
 * True when a value would root at a UE mount after separator and `/Content`
 * normalization — EVEN IF it also carries a traversal or colon that made its
 * canonical form untrustworthy. The gate fails closed on exactly these values
 * instead of ignoring them, while a genuinely non-UE string (an OS import path,
 * a message, a class name) stays outside path confinement.
 */
inline bool McpIsUnrealRootedCandidate(const FString& InPath)
{
	FString Path = InPath.TrimStartAndEnd();
	Path.ReplaceInline(TEXT("\\"), TEXT("/"));
	while (Path.Contains(TEXT("//")))
	{
		Path = Path.Replace(TEXT("//"), TEXT("/"));
	}
	McpAssetPathCanonical::MapContentRootInline(Path);
	return McpAssetPathCanonical::IsUnrealRoot(Path);
}
