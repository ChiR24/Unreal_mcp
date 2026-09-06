// McpResourceRevision.h
// Task 31 primitive C2 (native mirror): version-aware read-only resource
// revisions. This header is the native counterpart of the TypeScript module
// `src/server/mcp-primitives/resource-revision.ts`. It carries NO transport
// wiring and NO editor reads; it defines the numeric monotonic revision type,
// the revisioned payload envelope, the allowlisted subscribable URIs, and the
// allowlist guard. Task 37 owns wiring these into `resources/*` protocol
// methods; this header is metadata only and is compiled only where included.
#pragma once

#include "CoreMinimal.h"

// A numeric, monotonically non-decreasing resource revision. Mirrors the
// TypeScript branded `ResourceRevision`. 0 is reserved as "never observed";
// observed revisions start at McpInitialResourceRevision.
using FMcpResourceRevision = int64;

inline constexpr FMcpResourceRevision McpInitialResourceRevision = 1;

// A resource payload tagged with the URI it was read from and the revision that
// produced it. Mirrors the TypeScript `RevisionedResource<T>`. `Data` is the
// bounded, redacted body; it never carries raw editor internals or host paths.
template <typename T>
struct TMcpRevisionedResource
{
	FString Uri;
	FMcpResourceRevision Revision = McpInitialResourceRevision;
	T Data;
};

namespace McpResourceRevisionInternal
{
	// The closed allowlist of URIs whose revisions can be tracked and (in
	// Task 34) subscribed to. Mirrors the TypeScript `SUBSCRIBABLE_URIS`.
	inline const TArray<FString>& SubscribableUris()
	{
		static const TArray<FString> Uris = {
			TEXT("ue://capability/catalog"),
			TEXT("ue://project"),
			TEXT("ue://level"),
			TEXT("ue://selection"),
			TEXT("ue://asset-registry"),
			TEXT("ue://pie"),
			TEXT("ue://build"),
			TEXT("ue://render"),
			TEXT("ue://logs"),
		};
		return Uris;
	}
}

// Narrow an arbitrary URI to the closed subscribable allowlist. Mirrors the
// TypeScript `isSubscribableUri` guard.
inline bool McpIsSubscribableUri(const FString& Uri)
{
	return McpResourceRevisionInternal::SubscribableUris().Contains(Uri);
}
