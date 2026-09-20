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
/**
 * A first path segment that names a host filesystem root, never a UE mount.
 * Mirrors the TypeScript HOST_PATH_PATTERN so both surfaces refuse the same
 * shapes. `/Temp` is a real UE root and does not collide with `tmp`.
 */
inline bool IsHostFilesystemRootSegment(const FString& Segment)
{
	static const TCHAR* const HostRoots[] = {
		TEXT("home"), TEXT("users"), TEXT("etc"), TEXT("proc"), TEXT("sys"),
		TEXT("var"), TEXT("root"), TEXT("tmp"), TEXT("bin"), TEXT("opt"), TEXT("usr")
	};
	for (const TCHAR* const HostRoot : HostRoots)
	{
		if (Segment.Equals(HostRoot, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	return false;
}

/**
 * Mount roots a canonical UE object path can start at.
 *
 * The five engine roots are not the whole list. Every enabled plugin mounts its
 * own content root -- /MoverExamples, /MetaHumanCharacter, /Paper2D, and any
 * Fab, marketplace or game-feature plugin -- and the post-queue validator
 * SanitizeProjectRelativePath already accepts those by asking FPackageName
 * which roots actually exist. Hardcoding five here meant this socket-thread
 * stage emptied every plugin path BEFORE that validator ever ran, so all
 * plugin-shipped content (meshes, animations, physics assets) was unreachable
 * through any capability that canonicalizes a path, and the caller got back
 * "Could not load asset: " with the path erased from the message.
 *
 * This stage stays free of engine mount state on purpose -- it runs on a socket
 * thread for every string in a hostile payload -- so it accepts a root SHAPED
 * like a mount and leaves "is it really mounted" to the post-queue validator,
 * which consults FPackageName and logs. A root the engine has not mounted
 * simply fails to resolve; traversal and colons are still rejected by the
 * caller before this is reached, and a host filesystem root is refused here.
 */
inline bool IsUnrealRoot(const FString& Path)
{
	if (Path.Len() < 2 || Path[0] != TEXT('/'))
	{
		return false;
	}

	int32 SegmentEnd = 1;
	while (SegmentEnd < Path.Len() && Path[SegmentEnd] != TEXT('/'))
	{
		const TCHAR Char = Path[SegmentEnd];
		const bool bMountNameChar = (Char >= TEXT('A') && Char <= TEXT('Z')) ||
			(Char >= TEXT('a') && Char <= TEXT('z')) ||
			(Char >= TEXT('0') && Char <= TEXT('9')) || Char == TEXT('_');
		if (!bMountNameChar)
		{
			return false;
		}
		++SegmentEnd;
	}

	if (SegmentEnd <= 1)
	{
		return false;
	}
	return !IsHostFilesystemRootSegment(Path.Mid(1, SegmentEnd - 1));
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
