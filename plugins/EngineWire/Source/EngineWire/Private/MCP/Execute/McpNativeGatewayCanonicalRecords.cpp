#include "MCP/Execute/McpNativeGatewayCanonicalRecords.h"
// McpNativeGatewayCanonicalRecords.cpp — see header for the resolution contract.

#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"
#include "MCP/Generated/McpGeneratedCapabilityShards.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

FString McpLegacyCapabilityKey(const FString& Tool, const FString& Action)
{
	return Tool + TEXT("\t") + Action;
}

FMcpCanonicalRecordIndex FMcpCanonicalRecordIndex::Build()
{
	FMcpCanonicalRecordIndex Index;
	Index.CatalogRevision = McpGeneratedCapabilityShards::CatalogRevision();

	const FMcpCapabilityStore& Store = FMcpCapabilityStore::Get();
	if (!Store.IsReady())
	{
		Index.LoadError = FString::Printf(
			TEXT("capability store unavailable (%s): %s"),
			McpCapabilityStoreStatusToken(Store.GetStatus()), *Store.GetStatusDetail());
		return Index;
	}

	for (const FMcpCapabilityRecord& Record : Store.GetRecords())
	{
		Index.RecordsById.Add(Record.Id, &Record);
		for (const FMcpLegacyPair& Pair : Record.LegacyPairs)
		{
			Index.LegacyToCapabilityId.Add(McpLegacyCapabilityKey(Pair.Tool, Pair.Action), Record.Id);
		}
		// The first pair is the advertised primary; a folded family's former
		// names follow it and must not displace it.
		if (Record.LegacyPairs.Num() > 0)
		{
			Index.CapabilityIdToLegacyAction.Add(Record.Id, Record.LegacyPairs[0].Action);
		}
		for (const FString& Alias : Record.Aliases)
		{
			if (!Alias.IsEmpty())
			{
				Index.AliasToCapabilityIds.FindOrAdd(Alias).AddUnique(Record.Id);
			}
		}
	}

	if (Index.RecordsById.Num() != McpGeneratedCapabilityShards::TotalRecordCount())
	{
		Index.LoadError = FString::Printf(
			TEXT("resolved %d records but the generated index declares %d"),
			Index.RecordsById.Num(), McpGeneratedCapabilityShards::TotalRecordCount());
		Index.RecordsById.Reset();
		Index.LegacyToCapabilityId.Reset();
		Index.CapabilityIdToLegacyAction.Reset();
		Index.AliasToCapabilityIds.Reset();
		return Index;
	}

	Index.bLoaded = true;
	return Index;
}

const FMcpCanonicalRecordIndex& FMcpCanonicalRecordIndex::Get()
{
	static const FMcpCanonicalRecordIndex Index = FMcpCanonicalRecordIndex::Build();
	return Index;
}

const FMcpCapabilityRecord* FMcpCanonicalRecordIndex::FindById(const FString& CapabilityId) const
{
	const FMcpCapabilityRecord* const* Found = RecordsById.Find(CapabilityId);
	return Found ? *Found : nullptr;
}

const FMcpCapabilityRecord* FMcpCanonicalRecordIndex::FindByLegacy(
	const FString& Tool, const FString& Action) const
{
	const FString* CapabilityId = LegacyToCapabilityId.Find(McpLegacyCapabilityKey(Tool, Action));
	return CapabilityId ? FindById(*CapabilityId) : nullptr;
}

EMcpAliasResolution FMcpCanonicalRecordIndex::ResolveAlias(
	const FString& Alias, FString& OutCapabilityId) const
{
	OutCapabilityId.Reset();
	const TArray<FString>* Owners = AliasToCapabilityIds.Find(Alias);
	if (!Owners || Owners->Num() == 0)
	{
		return EMcpAliasResolution::Unknown;
	}
	if (Owners->Num() > 1)
	{
		return EMcpAliasResolution::Ambiguous;
	}
	OutCapabilityId = (*Owners)[0];
	return EMcpAliasResolution::Unique;
}

TArray<FString> FMcpCanonicalRecordIndex::GetLegacyActionsForTool(const FString& Tool) const
{
	const FString Prefix = Tool + TEXT("\t");
	TArray<FString> Actions;
	for (const TPair<FString, FString>& Entry : LegacyToCapabilityId)
	{
		if (Entry.Key.StartsWith(Prefix, ESearchCase::CaseSensitive))
		{
			Actions.Add(Entry.Key.RightChop(Prefix.Len()));
		}
	}
	Actions.Sort();
	return Actions;
}

FString FMcpCanonicalRecordIndex::GetLegacyActionForCapability(const FString& CapabilityId) const
{
	const FString* Action = CapabilityIdToLegacyAction.Find(CapabilityId);
	return Action ? *Action : FString();
}

TArray<FString> FMcpCanonicalRecordIndex::GetCapabilityIds() const
{
	TArray<FString> Ids;
	RecordsById.GetKeys(Ids);
	Ids.Sort();
	return Ids;
}
