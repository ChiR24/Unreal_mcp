// McpNativeGatewayCanonicalRecords.h — capability resolution for gateway execute
//
// Execute has to answer three questions discovery never asks: which capability
// does a canonical id name, which capability does a generated legacy
// {tool, action} pair name, and is an alias unambiguous. 207 of the 1,335
// records carry a legacy action that differs from routing.dispatchAction (for
// example manage_asset.find_by_tag dispatches asset_query), so a legacy caller
// cannot be resolved by dispatch action alone.
//
// Schemas, routing and policy come from FMcpCapabilityStore — this index adds
// only the alias/legacy projection the store does not retain, and never becomes
// a second source of capability metadata.

#pragma once

#include "CoreMinimal.h"

struct FMcpCapabilityRecord;

enum class EMcpAliasResolution : uint8
{
	Unknown,
	Unique,
	Ambiguous,
};

class FMcpCanonicalRecordIndex
{
public:
	/** Process-wide index, built once on first use. */
	static const FMcpCanonicalRecordIndex& Get();

	/** Build from the generated shards. Used by Get() and by tests. */
	static FMcpCanonicalRecordIndex Build();

	bool IsLoaded() const { return bLoaded; }
	const FString& GetLoadError() const { return LoadError; }
	const FString& GetCatalogRevision() const { return CatalogRevision; }
	int32 Num() const { return RecordsById.Num(); }

	const FMcpCapabilityRecord* FindById(const FString& CapabilityId) const;

	/** Resolve a generated legacy {tool, action} pair to its canonical record. */
	const FMcpCapabilityRecord* FindByLegacy(const FString& Tool, const FString& Action) const;

	/** Ambiguous aliases are reported, never silently resolved to a first match. */
	EMcpAliasResolution ResolveAlias(const FString& Alias, FString& OutCapabilityId) const;

	/** Legacy action names accepted for a parent tool, sorted. */
	TArray<FString> GetLegacyActionsForTool(const FString& Tool) const;

	/**
	 * Client-facing legacy action for a capability. This is the value handlers
	 * read from the payload; every record carries exactly one legacy id, and it
	 * is NOT routing.dispatchAction (that names the internal TypeScript route).
	 */
	FString GetLegacyActionForCapability(const FString& CapabilityId) const;

	TArray<FString> GetCapabilityIds() const;

private:
	bool bLoaded = false;
	FString LoadError;
	FString CatalogRevision;
	TMap<FString, const FMcpCapabilityRecord*> RecordsById;
	TMap<FString, FString> LegacyToCapabilityId;
	TMap<FString, FString> CapabilityIdToLegacyAction;
	TMap<FString, TArray<FString>> AliasToCapabilityIds;
};

/** True when per-action canonical validation may replace the legacy per-tool gate. */
bool McpCanonicalRecordsAvailable();

/** Stable key for a generated legacy {tool, action} pair. */
FString McpLegacyCapabilityKey(const FString& Tool, const FString& Action);
