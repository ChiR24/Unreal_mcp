// McpResourceCatalog.h
// Task 31 (native mirror): the normalized catalog of NEW version-aware
// read-only resources and templates. This is the native half of the TS/native
// normalized fixture; the TypeScript half is `src/resources/resource-catalog.ts`
// and the plugin source-contract test `tests/unit/plugin/mcp_resources_contracts.test.ts`
// asserts the two agree. The six pre-existing resources stay registered in the
// TypeScript resource-registry; these entries are strictly additive. Task 37
// owns wiring these into `resources/list` and `resources/templates/list`; this
// header is metadata only and is compiled only where included.
#pragma once

#include "CoreMinimal.h"

// A single read-only resource definition. Mirrors the TypeScript
// `ResourceDefinition`.
struct FMcpResourceDefinition
{
	FString Uri;
	FString Name;
	FString Description;
	FString MimeType;
};

// A single read-only resource template definition. Mirrors the TypeScript
// `ResourceTemplateDefinition`.
struct FMcpResourceTemplateDefinition
{
	FString UriTemplate;
	FString Name;
	FString Description;
	FString MimeType;
};

namespace McpResourceCatalog
{
	inline const FString& JsonMimeType()
	{
		static const FString Mime = TEXT("application/json");
		return Mime;
	}

	inline const FString& LiveStateRevisionUri()
	{
		static const FString Uri = TEXT("ue://state/revisions");
		return Uri;
	}

	// Task 47 serves readiness, anonymous aggregates and the rendered telemetry
	// exposition here. Named so the read classifier and the health body builder
	// share ONE spelling with the listed catalog entry below.
	inline const FString& HealthUri()
	{
		static const FString Uri = TEXT("ue://health");
		return Uri;
	}

	// NEW static resources added by Task 31 (beyond the pre-existing six).
	// Mirrors the TypeScript `NEW_RESOURCE_DEFINITIONS`.
	inline const TArray<FMcpResourceDefinition>& NewStaticResources()
	{
		static const TArray<FMcpResourceDefinition> Defs = {
			{ TEXT("ue://capability/catalog"), TEXT("Capability Catalog"),
				TEXT("Bounded catalog of gateway capabilities with a monotonic revision"), JsonMimeType() },
			{ TEXT("ue://project"), TEXT("Project"),
				TEXT("Redacted project name, engine version, and content root"), JsonMimeType() },
			{ TEXT("ue://editor"), TEXT("Editor State"),
				TEXT("Bounded editor state: PIE status and current level"), JsonMimeType() },
			{ TEXT("ue://selection"), TEXT("Selection"),
				TEXT("Bounded list of selected actor handles"), JsonMimeType() },
			{ LiveStateRevisionUri(), TEXT("Live State Revisions"),
				TEXT("Current selection, level, asset-registry, and package revision counters"), JsonMimeType() },
		};
		return Defs;
	}

	// The six pre-existing resources the TypeScript resource-registry serves
	// directly (src/server/resource-registry.ts RESOURCE_DEFINITIONS). Native
	// resources/list must advertise the SAME public set as the TS transport, so
	// they are mirrored here verbatim (name/description/mimeType byte-equal).
	inline const TArray<FMcpResourceDefinition>& LegacyStaticResources()
	{
		static const TArray<FMcpResourceDefinition> Defs = {
			{ TEXT("ue://assets"), TEXT("Assets"), TEXT("Project assets"), JsonMimeType() },
			{ TEXT("ue://actors"), TEXT("Actors"), TEXT("Actors in the current level"), JsonMimeType() },
			{ TEXT("ue://level"), TEXT("Current Level"), TEXT("Current level name and path"), JsonMimeType() },
			{ HealthUri(), TEXT("Health Status"), TEXT("Server health and performance metrics"), JsonMimeType() },
			{ TEXT("ue://automation-bridge"), TEXT("Automation Bridge"),
				TEXT("Automation bridge diagnostics and recent activity"), JsonMimeType() },
			{ TEXT("ue://version"), TEXT("Engine Version"), TEXT("Unreal Engine version and compatibility info"), JsonMimeType() },
		};
		return Defs;
	}

	// Read-only resource templates added by Task 31. Mirrors the TypeScript
	// `RESOURCE_TEMPLATES`.
	inline const TArray<FMcpResourceTemplateDefinition>& Templates()
	{
		static const TArray<FMcpResourceTemplateDefinition> Defs = {
			{ TEXT("ue://capability/{capabilityId}"), TEXT("Capability Record"),
				TEXT("Bounded record for one capability (identifier, category, action count; no full schema)"), JsonMimeType() },
			{ TEXT("ue://knowledge/{engineVersion}/{topic}"), TEXT("Engine Knowledge"),
				TEXT("Stable Unreal knowledge keyed by engine version and topic"), JsonMimeType() },
			{ TEXT("ue://object/{objectPath}"), TEXT("Object Reference"),
				TEXT("Normalized handle for an object at a UE content path"), JsonMimeType() },
			{ TEXT("ue://asset/{assetPath}"), TEXT("Asset Reference"),
				TEXT("Normalized handle for an asset at a UE content path"), JsonMimeType() },
		};
		return Defs;
	}

	// The complete TypeScript resources/list surface: the six legacy resources
	// followed by the five additive resources, matching the TypeScript order
	// [...RESOURCE_DEFINITIONS, ...NEW_RESOURCE_DEFINITIONS]. This is the
	// stdio transport's set, mirrored here so native can tell a resource it
	// deliberately does not serve from one it has never heard of.
	inline const TArray<FMcpResourceDefinition>& TypeScriptListedResources()
	{
		static const TArray<FMcpResourceDefinition> All = []()
		{
			TArray<FMcpResourceDefinition> Out;
			Out.Append(LegacyStaticResources());
			Out.Append(NewStaticResources());
			return Out;
		}();
		return All;
	}

	// The resources the stdio transport serves and native /mcp deliberately does
	// NOT advertise.
	//
	// Each one's content is live editor state that is only valid on the GAME
	// thread: the asset registry (ue://assets), a world actor iteration
	// (ue://actors), the open map (ue://level), PIE status (ue://editor) and the
	// editor selection (ue://selection). The stdio transport can serve them
	// because it is a separate process that round-trips every read through the
	// automation bridge onto the game thread. The native transport answers
	// resources/read ON THE SOCKET THREAD, where it may do neither of the only
	// two things that would produce this data:
	//
	//   * block the socket thread on editor work - forbidden by this plugin's
	//     own transport contract ("Do not block socket threads on Unreal work");
	//   * publish a transport-thread cache and serve that - rejected in writing
	//     by Foundation/McpLiveStateRevisions.h, because a cached view of live
	//     editor state is stale by the time it is read.
	//
	// So native does not advertise them. Listing a resource in resources/list
	// and then refusing every resources/read for it is a broken contract; a
	// smaller honest catalogue is not. A client that needs these reads them over
	// the stdio transport, which can.
	inline const TArray<FString>& NativeUnservedUris()
	{
		static const TArray<FString> Uris = {
			TEXT("ue://assets"), TEXT("ue://actors"), TEXT("ue://level"),
			TEXT("ue://editor"), TEXT("ue://selection"),
		};
		return Uris;
	}

	inline bool IsNativeUnservedUri(const FString& Uri)
	{
		return NativeUnservedUris().Contains(Uri);
	}

	// The native resources/list surface: every TypeScript resource this
	// transport can actually READ. Advertised set == servable set, by
	// construction - the two cannot drift apart because the same filter builds
	// this list and gates the read classifier.
	inline const TArray<FMcpResourceDefinition>& AllListedResources()
	{
		static const TArray<FMcpResourceDefinition> Served = []()
		{
			TArray<FMcpResourceDefinition> Out;
			for (const FMcpResourceDefinition& Def : TypeScriptListedResources())
			{
				if (!IsNativeUnservedUri(Def.Uri))
				{
					Out.Add(Def);
				}
			}
			return Out;
		}();
		return Served;
	}

	inline bool IsListedResourceUri(const FString& Uri)
	{
		for (const FMcpResourceDefinition& Def : AllListedResources())
		{
			if (Def.Uri == Uri)
			{
				return true;
			}
		}
		return false;
	}

	// True when Uri is a concrete instance of a known resource template (e.g.
	// `ue://capability/manage_asset`, `ue://object/...`). Used to distinguish a
	// KNOWN-but-editor-state read (RESOURCE_UNAVAILABLE) from a genuinely unknown
	// uri (RESOURCE_NOT_FOUND).
	inline bool MatchesKnownTemplate(const FString& Uri)
	{
		static const TArray<FString> Prefixes = {
			TEXT("ue://capability/"), TEXT("ue://knowledge/"), TEXT("ue://object/"), TEXT("ue://asset/"),
		};
		for (const FString& Prefix : Prefixes)
		{
			if (Uri.StartsWith(Prefix, ESearchCase::CaseSensitive) && Uri.Len() > Prefix.Len())
			{
				return true;
			}
		}
		return false;
	}
}
