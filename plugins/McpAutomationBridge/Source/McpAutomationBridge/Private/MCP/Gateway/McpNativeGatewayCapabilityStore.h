// McpNativeGatewayCapabilityStore.h — generated capability shard loader
//
// Owns the ONLY native source of discovery metadata: the generated shards under
// MCP/Generated. There is deliberately no handwritten alternate catalog. If a
// shard is missing, unparsable, structurally invalid, or the record total does
// not match the generated index, the store reports a typed failure and exposes
// ZERO records, so discovery answers with an honest startup error instead of
// advertising data that is not in the canonical registry.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

enum class EMcpCapabilityStoreStatus : uint8
{
	Ready,
	ShardParseFailed,
	ShardRecordInvalid,
	RecordCountMismatch,
};

struct FMcpCapabilityRecord
{
	FString Id;
	FString Parent;
	FString DispatchAction;
	FString Domain;
	FString Family;
	FString Summary;
	FString Effect;
	FString DeprecationStatus;
	TArray<FString> Topics;
	TArray<FString> Aliases;
	TArray<FString> WhenToUse;
	TArray<FString> WhenNotToUse;
	TArray<TSharedPtr<FJsonValue>> Examples;
	TSharedPtr<FJsonObject> InputSchema;
	TSharedPtr<FJsonObject> OutputSchema;
	TSharedPtr<FJsonObject> Availability;
	TSharedPtr<FJsonObject> Behavior;
	TSharedPtr<FJsonObject> Policy;
	TSharedPtr<FJsonObject> Cost;
	TSharedPtr<FJsonObject> Deprecation;
	TSharedPtr<FJsonObject> Hashes;
	int32 ExampleCount = 0;
};

/**
 * The action name this capability is addressed by on its parent tool — the
 * capability id's last dotted segment (e.g. "manage_audio.create_metasound" ->
 * "create_metasound").
 *
 * This is the name `execute` accepts and `search` advertises. `DispatchAction`
 * is INTERNAL routing and is not unique per capability (every manage_audio
 * capability dispatches through "manage_audio"), so it cannot identify one.
 */
inline FString McpCapabilityPublicAction(const FMcpCapabilityRecord& Record)
{
	int32 LastDot = INDEX_NONE;
	if (Record.Id.FindLastChar(TEXT('.'), LastDot) && LastDot != INDEX_NONE)
	{
		return Record.Id.RightChop(LastDot + 1);
	}
	return Record.DispatchAction;
}

/**
 * Distinct, byte-order-sorted projection over records, for projections that
 * COMPUTE a string. The reference-returning variant in the describe unit cannot
 * be used for those: binding a temporary to `const FString&` would dangle.
 */
inline TArray<FString> McpDistinctSortedComputed(
	const TArray<const FMcpCapabilityRecord*>& Records,
	TFunctionRef<FString(const FMcpCapabilityRecord&)> Project)
{
	TArray<FString> Values;
	for (const FMcpCapabilityRecord* Record : Records)
	{
		const FString Value = Project(*Record);
		if (!Values.ContainsByPredicate(
				[&Value](const FString& E) { return E.Equals(Value, ESearchCase::CaseSensitive); }))
		{
			Values.Add(Value);
		}
	}
	Values.Sort([](const FString& L, const FString& R) { return L.Compare(R, ESearchCase::CaseSensitive) < 0; });
	return Values;
}

class FMcpCapabilityStore
{
public:
	/** Process-wide store, loaded once on first use. */
	static const FMcpCapabilityStore& Get();

	/** Build a store from explicit shard payloads. Used by Get() and by tests. */
	static FMcpCapabilityStore FromShards();

	bool IsReady() const { return Status == EMcpCapabilityStoreStatus::Ready; }
	EMcpCapabilityStoreStatus GetStatus() const { return Status; }
	const FString& GetStatusDetail() const { return StatusDetail; }

	/** Records ordered by canonical id. Empty unless IsReady(). */
	const TArray<FMcpCapabilityRecord>& GetRecords() const { return Records; }
	const FString& GetCatalogRevision() const { return CatalogRevision; }

	/** Distinct sorted projections used by discovery filters and guidance. */
	TArray<FString> GetParents() const;
	TArray<FString> GetDomains() const;
	TArray<FString> GetFamilies() const;

	/** Records for one parent tool, ordered by canonical id. */
	TArray<const FMcpCapabilityRecord*> GetRecordsForParent(const FString& Parent) const;
	const FMcpCapabilityRecord* FindByParentAction(const FString& Parent, const FString& Action) const;

private:
	EMcpCapabilityStoreStatus Status = EMcpCapabilityStoreStatus::ShardParseFailed;
	FString StatusDetail;
	FString CatalogRevision;
	TArray<FMcpCapabilityRecord> Records;
};

/** Stable machine-readable token for a store failure, echoed in discovery errors. */
const TCHAR* McpCapabilityStoreStatusToken(EMcpCapabilityStoreStatus Status);
