// McpNativeGatewayCanonicalRecords.cpp — see header for the resolution contract.

#include "MCP/Execute/McpNativeGatewayCanonicalRecords.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"
#include "MCP/Generated/McpGeneratedCapabilityShards.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FString McpLegacyCapabilityKey(const FString& Tool, const FString& Action)
{
	return Tool + TEXT("\t") + Action;
}

namespace
{
FString ConcatenateShardChunks(const McpGeneratedCapabilityShards::FMcpCapabilityShard& Shard)
{
	FString Payload;
	for (int32 Index = 0; Index < Shard.ChunkCount; ++Index)
	{
		Payload.Append(Shard.Chunks[Index]);
	}
	return Payload;
}

const TSharedPtr<FJsonObject>* ReadObjectField(const TSharedPtr<FJsonObject>& Owner, const TCHAR* Field)
{
	const TSharedPtr<FJsonObject>* Nested = nullptr;
	if (Owner.IsValid() && Owner->TryGetObjectField(Field, Nested) && Nested)
	{
		return Nested;
	}
	return nullptr;
}
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
	}

	// The store keeps schemas/routing but not aliases/legacyIds, so this pass
	// projects exactly those two fields out of the same generated shards.
	for (int32 ShardIndex = 0; ShardIndex < McpGeneratedCapabilityShards::Num(); ++ShardIndex)
	{
		const McpGeneratedCapabilityShards::FMcpCapabilityShard& Shard =
			McpGeneratedCapabilityShards::At(ShardIndex);
		const FString Payload = ConcatenateShardChunks(Shard);

		TArray<TSharedPtr<FJsonValue>> Entries;
		TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(Payload);
		if (!FJsonSerializer::Deserialize(Reader, Entries))
		{
			Index.RecordsById.Reset();
			Index.LegacyToCapabilityId.Reset();
			Index.CapabilityIdToLegacyAction.Reset();
			Index.AliasToCapabilityIds.Reset();
			Index.LoadError = FString::Printf(
				TEXT("shard '%s' could not be parsed for alias/legacy resolution"), Shard.ParentTool);
			return Index;
		}

		for (const TSharedPtr<FJsonValue>& Entry : Entries)
		{
			const TSharedPtr<FJsonObject>* EntryObject = nullptr;
			if (!Entry.IsValid() || !Entry->TryGetObject(EntryObject) || !EntryObject)
			{
				continue;
			}
			const TSharedPtr<FJsonObject>* RecordObject = ReadObjectField(*EntryObject, TEXT("record"));
			if (!RecordObject)
			{
				continue;
			}

			FString CapabilityId;
			if (!(*RecordObject)->TryGetStringField(TEXT("id"), CapabilityId) || CapabilityId.IsEmpty())
			{
				continue;
			}

			const TArray<TSharedPtr<FJsonValue>>* LegacyIds = nullptr;
			if ((*RecordObject)->TryGetArrayField(TEXT("legacyIds"), LegacyIds) && LegacyIds)
			{
				for (const TSharedPtr<FJsonValue>& LegacyValue : *LegacyIds)
				{
					const TSharedPtr<FJsonObject>* LegacyObject = nullptr;
					if (!LegacyValue.IsValid() || !LegacyValue->TryGetObject(LegacyObject) || !LegacyObject)
					{
						continue;
					}
					FString LegacyTool;
					FString LegacyAction;
					if ((*LegacyObject)->TryGetStringField(TEXT("tool"), LegacyTool) &&
						(*LegacyObject)->TryGetStringField(TEXT("action"), LegacyAction) &&
						!LegacyTool.IsEmpty() && !LegacyAction.IsEmpty())
					{
						Index.LegacyToCapabilityId.Add(
							McpLegacyCapabilityKey(LegacyTool, LegacyAction), CapabilityId);
						Index.CapabilityIdToLegacyAction.FindOrAdd(CapabilityId) = LegacyAction;
					}
				}
			}

			const TArray<TSharedPtr<FJsonValue>>* Aliases = nullptr;
			if ((*RecordObject)->TryGetArrayField(TEXT("aliases"), Aliases) && Aliases)
			{
				for (const TSharedPtr<FJsonValue>& AliasValue : *Aliases)
				{
					FString Alias;
					if (AliasValue.IsValid() && AliasValue->TryGetString(Alias) && !Alias.IsEmpty())
					{
						Index.AliasToCapabilityIds.FindOrAdd(Alias).AddUnique(CapabilityId);
					}
				}
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

bool McpCanonicalRecordsAvailable()
{
	return FMcpCanonicalRecordIndex::Get().IsLoaded();
}
