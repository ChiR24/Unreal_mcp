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
			{ TEXT("ue://health"), TEXT("Health Status"), TEXT("Server health and performance metrics"), JsonMimeType() },
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

	// The complete resources/list surface: the six legacy resources followed by
	// the four Task 31 additions, matching the TypeScript order
	// [...RESOURCE_DEFINITIONS, ...NEW_RESOURCE_DEFINITIONS].
	inline const TArray<FMcpResourceDefinition>& AllListedResources()
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

	// True when Uri is one of the listed static resources (any of the ten).
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
