// McpResourceCatalog.h
// The resources and templates native /mcp lists. Mirrors the stdio surface in
// src/server/resource-registry.ts and src/resources/resource-catalog.ts, minus
// the live editor-state URIs the socket thread may not read (IsNativeUnservedUri).
#pragma once

#include "CoreMinimal.h"

struct FMcpResourceDefinition
{
	FString Uri;
	FString Name;
	FString Description;
	FString MimeType;
};

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

	inline const FString& HealthUri()
	{
		static const FString Uri = TEXT("ue://health");
		return Uri;
	}

	// Listed == servable: the read classifier uses this same list.
	inline const TArray<FMcpResourceDefinition>& AllListedResources()
	{
		static const TArray<FMcpResourceDefinition> Defs = {
			{ HealthUri(), TEXT("Health Status"), TEXT("Server health and performance metrics"), JsonMimeType() },
			{ TEXT("ue://automation-bridge"), TEXT("Automation Bridge"),
				TEXT("Automation bridge diagnostics and recent activity"), JsonMimeType() },
			{ TEXT("ue://capability/catalog"), TEXT("Capability Catalog"),
				TEXT("Bounded catalog of gateway capabilities with a monotonic revision"), JsonMimeType() },
			{ TEXT("ue://project"), TEXT("Project"),
				TEXT("Redacted project name, engine version, and content root"), JsonMimeType() },
			{ LiveStateRevisionUri(), TEXT("Live State Revisions"),
				TEXT("Current selection, level, asset-registry, and package revision counters"), JsonMimeType() },
		};
		return Defs;
	}

	inline const TArray<FMcpResourceTemplateDefinition>& Templates()
	{
		static const TArray<FMcpResourceTemplateDefinition> Defs = {
			{ TEXT("ue://capability/{capabilityId}"), TEXT("Capability Record"),
				TEXT("Bounded record for one capability (identifier, category, action count; no full schema)"), JsonMimeType() },
			{ TEXT("ue://knowledge/{topic}"), TEXT("Engine Knowledge"),
				TEXT("Stable Unreal knowledge keyed by topic"), JsonMimeType() },
			{ TEXT("ue://asset/{assetPath}"), TEXT("Asset Reference"),
				TEXT("Normalized handle for an asset at a UE content path"), JsonMimeType() },
		};
		return Defs;
	}

	// Live editor state only the game thread may read. The stdio transport serves
	// these by round-tripping through the automation bridge; native answers
	// resources/read on the socket thread, which must never block on editor work
	// and must not serve a stale cache, so it neither lists nor serves them.
	inline bool IsNativeUnservedUri(const FString& Uri)
	{
		static const TArray<FString> Uris = {
			TEXT("ue://assets"), TEXT("ue://actors"), TEXT("ue://editor"), TEXT("ue://selection"),
		};
		return Uris.Contains(Uri);
	}

	inline bool IsListedResourceUri(const FString& Uri)
	{
		return AllListedResources().ContainsByPredicate([&Uri](const FMcpResourceDefinition& Def) { return Def.Uri == Uri; });
	}

	// A concrete instance of a known template: a KNOWN-but-unservable read
	// (RESOURCE_UNAVAILABLE) rather than an unknown uri (RESOURCE_NOT_FOUND).
	inline bool MatchesKnownTemplate(const FString& Uri)
	{
		for (const TCHAR* Prefix : { TEXT("ue://capability/"), TEXT("ue://knowledge/"), TEXT("ue://asset/") })
		{
			if (Uri.StartsWith(Prefix, ESearchCase::CaseSensitive) && Uri.Len() > FCString::Strlen(Prefix))
			{
				return true;
			}
		}
		return false;
	}
}
