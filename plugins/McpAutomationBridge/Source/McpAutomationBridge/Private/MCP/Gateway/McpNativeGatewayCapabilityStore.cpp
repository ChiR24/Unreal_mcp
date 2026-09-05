// McpNativeGatewayCapabilityStore.cpp — see header for the fail-closed contract.

#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"
#include "MCP/Generated/McpGeneratedCapabilityShards.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
FString ConcatenateShard(const McpGeneratedCapabilityShards::FMcpCapabilityShard& Shard)
{
	FString Payload;
	for (int32 Index = 0; Index < Shard.ChunkCount; ++Index)
	{
		Payload.Append(Shard.Chunks[Index]);
	}
	return Payload;
}

TArray<FString> ReadStringArray(const TSharedPtr<FJsonObject>& Owner, const TCHAR* Field)
{
	TArray<FString> Values;
	const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
	if (Owner.IsValid() && Owner->TryGetArrayField(Field, Items) && Items)
	{
		for (const TSharedPtr<FJsonValue>& Item : *Items)
		{
			FString Value;
			if (Item.IsValid() && Item->TryGetString(Value)) Values.Add(Value);
		}
	}
	return Values;
}

TSharedPtr<FJsonObject> ReadObject(const TSharedPtr<FJsonObject>& Owner, const TCHAR* Field)
{
	const TSharedPtr<FJsonObject>* Nested = nullptr;
	if (Owner.IsValid() && Owner->TryGetObjectField(Field, Nested) && Nested) return *Nested;
	return nullptr;
}

bool ParseRecord(const TSharedPtr<FJsonObject>& Entry, FMcpCapabilityRecord& Out, FString& OutError)
{
	const TSharedPtr<FJsonObject> Record = ReadObject(Entry, TEXT("record"));
	if (!Record.IsValid())
	{
		OutError = TEXT("entry is missing its 'record' object");
		return false;
	}
	const TSharedPtr<FJsonObject> Discovery = ReadObject(Record, TEXT("discovery"));
	const TSharedPtr<FJsonObject> Routing = ReadObject(Record, TEXT("routing"));
	const TSharedPtr<FJsonObject> Schemas = ReadObject(Record, TEXT("schemas"));
	const TSharedPtr<FJsonObject> Hashes = ReadObject(Record, TEXT("hashes"));
	if (!Record->TryGetStringField(TEXT("id"), Out.Id) || Out.Id.IsEmpty())
	{
		OutError = TEXT("record is missing a non-empty 'id'");
		return false;
	}
	if (!Discovery.IsValid() || !Routing.IsValid() || !Schemas.IsValid() || !Hashes.IsValid())
	{
		OutError = FString::Printf(
			TEXT("record '%s' is missing discovery/routing/schemas/hashes"), *Out.Id);
		return false;
	}
	if (!Routing->TryGetStringField(TEXT("parentTool"), Out.Parent) ||
		!Routing->TryGetStringField(TEXT("dispatchAction"), Out.DispatchAction) ||
		Out.Parent.IsEmpty() || Out.DispatchAction.IsEmpty())
	{
		OutError = FString::Printf(TEXT("record '%s' has incomplete routing"), *Out.Id);
		return false;
	}
	Out.InputSchema = ReadObject(Schemas, TEXT("input"));
	Out.OutputSchema = ReadObject(Schemas, TEXT("output"));
	if (!Out.InputSchema.IsValid() || !Out.OutputSchema.IsValid())
	{
		OutError = FString::Printf(TEXT("record '%s' is missing an exact input/output schema"), *Out.Id);
		return false;
	}

	Discovery->TryGetStringField(TEXT("domain"), Out.Domain);
	Discovery->TryGetStringField(TEXT("family"), Out.Family);
	Discovery->TryGetStringField(TEXT("summary"), Out.Summary);
	Out.Topics = ReadStringArray(Discovery, TEXT("topics"));
	Out.Aliases = ReadStringArray(Record, TEXT("aliases"));
	Out.WhenToUse = ReadStringArray(Discovery, TEXT("whenToUse"));
	Out.WhenNotToUse = ReadStringArray(Discovery, TEXT("whenNotToUse"));

	Out.Availability = ReadObject(Record, TEXT("availability"));
	Out.Behavior = ReadObject(Record, TEXT("behavior"));
	Out.Policy = ReadObject(Record, TEXT("policy"));
	Out.Cost = ReadObject(Record, TEXT("cost"));
	Out.Deprecation = ReadObject(Record, TEXT("deprecation"));
	Out.Hashes = Hashes;
	if (Out.Behavior.IsValid()) Out.Behavior->TryGetStringField(TEXT("effect"), Out.Effect);
	if (Out.Deprecation.IsValid()) Out.Deprecation->TryGetStringField(TEXT("status"), Out.DeprecationStatus);

	const TArray<TSharedPtr<FJsonValue>>* Examples = nullptr;
	if (Record->TryGetArrayField(TEXT("examples"), Examples) && Examples)
	{
		Out.Examples = *Examples;
		Out.ExampleCount = Out.Examples.Num();
	}
	else
	{
		Out.ExampleCount = 0;
	}
	return true;
}

TArray<FString> DistinctSorted(const TArray<FMcpCapabilityRecord>& Records,
	TFunctionRef<const FString&(const FMcpCapabilityRecord&)> Project)
{
	TArray<FString> Values;
	for (const FMcpCapabilityRecord& Record : Records)
	{
		const FString& Value = Project(Record);
		const bool bSeen = Values.ContainsByPredicate(
			[&Value](const FString& Existing) { return Existing.Equals(Value, ESearchCase::CaseSensitive); });
		if (!bSeen) Values.Add(Value);
	}
	Values.Sort([](const FString& L, const FString& R) { return L.Compare(R, ESearchCase::CaseSensitive) < 0; });
	return Values;
}
}

const TCHAR* McpCapabilityStoreStatusToken(EMcpCapabilityStoreStatus Status)
{
	switch (Status)
	{
	case EMcpCapabilityStoreStatus::Ready: return TEXT("READY");
	case EMcpCapabilityStoreStatus::ShardParseFailed: return TEXT("SHARD_PARSE_FAILED");
	case EMcpCapabilityStoreStatus::ShardRecordInvalid: return TEXT("SHARD_RECORD_INVALID");
	case EMcpCapabilityStoreStatus::RecordCountMismatch: return TEXT("RECORD_COUNT_MISMATCH");
	}
	return TEXT("SHARD_PARSE_FAILED");
}

FMcpCapabilityStore FMcpCapabilityStore::FromShards()
{
	FMcpCapabilityStore Store;
	TArray<FMcpCapabilityRecord> Loaded;

	for (int32 ShardIndex = 0; ShardIndex < McpGeneratedCapabilityShards::Num(); ++ShardIndex)
	{
		const McpGeneratedCapabilityShards::FMcpCapabilityShard& Shard =
			McpGeneratedCapabilityShards::At(ShardIndex);
		const FString Payload = ConcatenateShard(Shard);

		TArray<TSharedPtr<FJsonValue>> Entries;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Payload);
		if (!FJsonSerializer::Deserialize(Reader, Entries))
		{
			Store.Status = EMcpCapabilityStoreStatus::ShardParseFailed;
			Store.StatusDetail = FString::Printf(
				TEXT("shard '%s' is not parsable JSON"), Shard.ParentTool);
			return Store;
		}
		if (Entries.Num() != Shard.RecordCount)
		{
			Store.Status = EMcpCapabilityStoreStatus::RecordCountMismatch;
			Store.StatusDetail = FString::Printf(
				TEXT("shard '%s' declares %d records but carries %d"),
				Shard.ParentTool, Shard.RecordCount, Entries.Num());
			return Store;
		}
		for (const TSharedPtr<FJsonValue>& Entry : Entries)
		{
			FMcpCapabilityRecord Record;
			FString Error;
			if (!Entry.IsValid() || Entry->Type != EJson::Object ||
				!ParseRecord(Entry->AsObject(), Record, Error))
			{
				Store.Status = EMcpCapabilityStoreStatus::ShardRecordInvalid;
				Store.StatusDetail = FString::Printf(
					TEXT("shard '%s': %s"), Shard.ParentTool, *Error);
				return Store;
			}
			Loaded.Add(MoveTemp(Record));
		}
	}

	if (Loaded.Num() != McpGeneratedCapabilityShards::TotalRecordCount())
	{
		Store.Status = EMcpCapabilityStoreStatus::RecordCountMismatch;
		Store.StatusDetail = FString::Printf(
			TEXT("loaded %d records but the generated index declares %d"),
			Loaded.Num(), McpGeneratedCapabilityShards::TotalRecordCount());
		return Store;
	}

	Loaded.Sort([](const FMcpCapabilityRecord& L, const FMcpCapabilityRecord& R)
		{ return L.Id.Compare(R.Id, ESearchCase::CaseSensitive) < 0; });
	Store.Records = MoveTemp(Loaded);
	Store.CatalogRevision = McpGeneratedCapabilityShards::CatalogRevision();
	Store.Status = EMcpCapabilityStoreStatus::Ready;
	return Store;
}

const FMcpCapabilityStore& FMcpCapabilityStore::Get()
{
	static const FMcpCapabilityStore Store = FMcpCapabilityStore::FromShards();
	return Store;
}

TArray<FString> FMcpCapabilityStore::GetParents() const
{
	return DistinctSorted(Records, [](const FMcpCapabilityRecord& R) -> const FString& { return R.Parent; });
}

TArray<FString> FMcpCapabilityStore::GetDomains() const
{
	return DistinctSorted(Records, [](const FMcpCapabilityRecord& R) -> const FString& { return R.Domain; });
}

TArray<FString> FMcpCapabilityStore::GetFamilies() const
{
	return DistinctSorted(Records, [](const FMcpCapabilityRecord& R) -> const FString& { return R.Family; });
}

TArray<const FMcpCapabilityRecord*> FMcpCapabilityStore::GetRecordsForParent(const FString& Parent) const
{
	TArray<const FMcpCapabilityRecord*> Matches;
	for (const FMcpCapabilityRecord& Record : Records)
	{
		if (Record.Parent.Equals(Parent, ESearchCase::CaseSensitive)) Matches.Add(&Record);
	}
	return Matches;
}

const FMcpCapabilityRecord* FMcpCapabilityStore::FindByParentAction(
	const FString& Parent, const FString& Action) const
{
	// Resolve the PUBLIC action name (the capability id's leaf) first — that is
	// the name execute accepts and the name search advertises. Matching only
	// DispatchAction made describe and execute disagree in BOTH directions:
	//   * manage_audio — all 50 capabilities share DispatchAction "manage_audio",
	//     so describe{action:"create_metasound"} was UNKNOWN_ACTION even though
	//     execute ran it, and describe{action:"manage_audio"} answered with one
	//     arbitrary sibling's contract (add_cue_node) for all 50.
	//   * build_environment — describe{action:"add_foliage_type"} resolved via
	//     DispatchAction while execute refused it, the error telling the caller
	//     to "call describe before execute", which is what they had just done.
	// DispatchAction is kept as a fallback so internal-routing spellings that
	// already worked keep working.
	for (const FMcpCapabilityRecord& Record : Records)
	{
		if (Record.Parent.Equals(Parent, ESearchCase::CaseSensitive) &&
			McpCapabilityPublicAction(Record).Equals(Action, ESearchCase::CaseSensitive)) return &Record;
	}
	for (const FMcpCapabilityRecord& Record : Records)
	{
		if (Record.Parent.Equals(Parent, ESearchCase::CaseSensitive) &&
			Record.DispatchAction.Equals(Action, ESearchCase::CaseSensitive)) return &Record;
	}
	return nullptr;
}
