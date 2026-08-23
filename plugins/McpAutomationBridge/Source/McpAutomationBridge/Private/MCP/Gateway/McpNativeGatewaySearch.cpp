// McpNativeGatewaySearch.cpp — see header for the cross-surface contract.

#include "MCP/Gateway/McpNativeGatewaySearch.h"
#include "MCP/Gateway/McpNativeGatewayCanonicalJson.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"
#include "MCP/Gateway/McpNativeGatewayGuidance.h"

namespace
{
// Ordered highest-signal first. A record matches when at least one rule fires;
// the score is the sum, so a query hitting the id outranks one hitting prose.
struct FMatchRule { const TCHAR* Reason; int32 Weight; };
const FMatchRule MatchRules[] = {
	{ TEXT("id-exact"), 100 },
	{ TEXT("id"), 50 },
	{ TEXT("family"), 20 },
	{ TEXT("domain"), 15 },
	{ TEXT("topic"), 12 },
	{ TEXT("summary"), 8 },
	{ TEXT("parent"), 5 },
};
constexpr int32 MatchRuleCount = UE_ARRAY_COUNT(MatchRules);

bool AnyTopicContains(const TArray<FString>& Topics, const FString& Query)
{
	for (const FString& Topic : Topics)
	{
		if (Topic.ToLower().Contains(Query, ESearchCase::CaseSensitive)) return true;
	}
	return false;
}

void EvaluateRules(const FMcpCapabilityRecord& Record, const FString& Query, bool (&OutHits)[MatchRuleCount])
{
	const FString LowerId = Record.Id.ToLower();
	OutHits[0] = LowerId.Equals(Query, ESearchCase::CaseSensitive);
	OutHits[1] = LowerId.Contains(Query, ESearchCase::CaseSensitive);
	OutHits[2] = Record.Family.ToLower().Contains(Query, ESearchCase::CaseSensitive);
	OutHits[3] = Record.Domain.ToLower().Contains(Query, ESearchCase::CaseSensitive);
	OutHits[4] = AnyTopicContains(Record.Topics, Query);
	OutHits[5] = Record.Summary.ToLower().Contains(Query, ESearchCase::CaseSensitive);
	OutHits[6] = Record.Parent.ToLower().Contains(Query, ESearchCase::CaseSensitive);
}

struct FScoredRecord
{
	const FMcpCapabilityRecord* Record = nullptr;
	int32 Score = 0;
	TArray<FString> Reasons;
};

TSharedPtr<FJsonObject> GuidedFilterError(
	const FString& ErrorCode, const FString& Message,
	const TArray<FString>& Candidates, const FString& Target, const FString& CatalogRevision)
{
	TSharedPtr<FJsonObject> Error = GatewayError(TEXT("search"), ErrorCode, Message);
	Error->SetStringField(TEXT("catalogRevision"), CatalogRevision);
	Error->SetArrayField(TEXT("suggestions"), GatewayStringArray(GatewayClosestMatches(Target, Candidates, 3)));
	Error->SetObjectField(TEXT("nextCall"), GatewayBuildNextCall(TEXT("search"), FString(), FString(), FString()));
	return Error;
}
}

TSharedPtr<FJsonObject> McpGatewayCatalogUnavailable(
	const FString& Operation, const FMcpCapabilityStore& Store)
{
	TSharedPtr<FJsonObject> Error = GatewayError(Operation, TEXT("CAPABILITY_CATALOG_UNAVAILABLE"),
		FString::Printf(
			TEXT("The generated capability catalog failed to load (%s): %s. Discovery is unavailable until the plugin is rebuilt from a valid canonical registry."),
			McpCapabilityStoreStatusToken(Store.GetStatus()), *Store.GetStatusDetail()));
	Error->SetStringField(TEXT("status"), McpCapabilityStoreStatusToken(Store.GetStatus()));
	return Error;
}

TSharedPtr<FJsonObject> McpGatewaySearchCapabilities(
	const FMcpDiscoveryQuery& Input, const FMcpCapabilityStore& Store,
	FMcpToolEnabledPredicate IsToolEnabled)
{
	if (!Store.IsReady()) return McpGatewayCatalogUnavailable(TEXT("search"), Store);

	const FString Query = Input.Query.TrimStartAndEnd().ToLower();
	const int32 Limit = FMath::Clamp(Input.Limit, 1, McpSearchMaxLimit);
	const int32 Offset = FMath::Max(0, Input.Offset);
	const FString& Revision = Store.GetCatalogRevision();

	if (Input.bHasDomain)
	{
		const TArray<FString> Domains = Store.GetDomains();
		if (!Domains.ContainsByPredicate([&](const FString& D) { return D.Equals(Input.Domain, ESearchCase::CaseSensitive); }))
		{
			return GuidedFilterError(TEXT("UNKNOWN_DOMAIN"),
				FString::Printf(TEXT("Unknown domain '%s'."), *Input.Domain), Domains, Input.Domain, Revision);
		}
	}
	if (Input.bHasFamily)
	{
		const TArray<FString> Families = Store.GetFamilies();
		if (!Families.ContainsByPredicate([&](const FString& F) { return F.Equals(Input.Family, ESearchCase::CaseSensitive); }))
		{
			return GuidedFilterError(TEXT("UNKNOWN_FAMILY"),
				FString::Printf(TEXT("Unknown family '%s'."), *Input.Family), Families, Input.Family, Revision);
		}
	}

	// Split on whitespace once; single-word queries yield one word and behave
	// exactly as before apart from the coverage bonus.
	TArray<FString> QueryWords;
	if (!Query.IsEmpty())
	{
		Query.ParseIntoArrayWS(QueryWords);
		if (QueryWords.Num() == 1) QueryWords.Reset();
	}

	TArray<FScoredRecord> Scored;
	for (const FMcpCapabilityRecord& Record : Store.GetRecords())
	{
		if (Input.bHasDomain && !Record.Domain.Equals(Input.Domain, ESearchCase::CaseSensitive)) continue;
		if (Input.bHasFamily && !Record.Family.Equals(Input.Family, ESearchCase::CaseSensitive)) continue;

		FScoredRecord Entry;
		Entry.Record = &Record;
		if (Query.IsEmpty())
		{
			Scored.Add(MoveTemp(Entry));
			continue;
		}
		// The whole query is scored first, so an exact phrase still ranks highest.
		bool Hits[MatchRuleCount] = {};
		EvaluateRules(Record, Query, Hits);
		for (int32 Rule = 0; Rule < MatchRuleCount; ++Rule)
		{
			if (!Hits[Rule]) continue;
			Entry.Reasons.Add(MatchRules[Rule].Reason);
			Entry.Score += MatchRules[Rule].Weight;
		}
		// Then each word separately. Matching the whole query as ONE literal
		// substring meant every ordinary phrase returned zero results — "create
		// new level map" found nothing while "create_level" found three — even
		// though the tool's own instruction is to search first. Per-word scoring
		// keeps a record that matches ANY word and ranks by how many it matched,
		// so adding a word can now refine a result set instead of emptying it.
		int32 MatchedWords = 0;
		for (const FString& Word : QueryWords)
		{
			bool WordHits[MatchRuleCount] = {};
			EvaluateRules(Record, Word, WordHits);
			bool bWordMatched = false;
			for (int32 Rule = 0; Rule < MatchRuleCount; ++Rule)
			{
				if (!WordHits[Rule]) continue;
				bWordMatched = true;
				Entry.Reasons.AddUnique(MatchRules[Rule].Reason);
				Entry.Score += MatchRules[Rule].Weight;
			}
			if (bWordMatched) ++MatchedWords;
		}
		// Favour records covering more of the query than records covering fewer.
		Entry.Score += MatchedWords * McpSearchWordCoverageBonus;
		if (Entry.Reasons.Num() == 0) continue;
		Scored.Add(MoveTemp(Entry));
	}

	// Strict total order: unique ids make the result independent of TArray::Sort
	// being an unstable introsort.
	Scored.Sort([](const FScoredRecord& L, const FScoredRecord& R)
	{
		if (L.Score != R.Score) return L.Score > R.Score;
		return L.Record->Id.Compare(R.Record->Id, ESearchCase::CaseSensitive) < 0;
	});

	const int32 Total = Scored.Num();
	TArray<TSharedPtr<FJsonValue>> Results;
	int32 Bytes = 0;
	bool bTruncated = false;
	for (int32 Index = Offset; Index < Total && Results.Num() < Limit; ++Index)
	{
		const FScoredRecord& Entry = Scored[Index];
		auto View = MakeShared<FJsonObject>();
		View->SetBoolField(TEXT("available"),
			!Entry.Record->DeprecationStatus.Equals(TEXT("removed"), ESearchCase::CaseSensitive) &&
			IsToolEnabled(Entry.Record->Parent));
		View->SetStringField(TEXT("capability"), Entry.Record->Id);
		View->SetStringField(TEXT("domain"), Entry.Record->Domain);
		View->SetStringField(TEXT("effect"), Entry.Record->Effect);
		View->SetStringField(TEXT("family"), Entry.Record->Family);
		View->SetArrayField(TEXT("matchReasons"), GatewayStringArray(Entry.Reasons));
		View->SetObjectField(TEXT("nextCall"), GatewayBuildNextCall(
			TEXT("describe"), Entry.Record->Parent, McpCapabilityPublicAction(*Entry.Record), FString()));
		View->SetStringField(TEXT("parent"), Entry.Record->Parent);
		View->SetNumberField(TEXT("score"), Entry.Score);
		View->SetStringField(TEXT("summary"), Entry.Record->Summary);

		FString Rendered;
		if (!McpCanonicalJsonObject(View, Rendered))
		{
			return GatewayError(TEXT("search"), TEXT("CAPABILITY_RENDER_FAILED"),
				FString::Printf(TEXT("Capability '%s' could not be rendered deterministically."), *Entry.Record->Id));
		}
		const int32 Size = McpCanonicalByteLength(Rendered);
		// The first result is always emitted, so an oversized single entry is
		// reported rather than silently producing an empty page.
		if (Bytes + Size > McpMaxResultBytes && Results.Num() > 0)
		{
			bTruncated = true;
			break;
		}
		Bytes += Size;
		Results.Add(MakeShared<FJsonValueObject>(View));
	}

	const bool bHasMore = Offset + Results.Num() < Total;
	const bool bByteBudgetTruncated = bTruncated;
	bTruncated = bByteBudgetTruncated || bHasMore;
	auto Out = MakeShared<FJsonObject>();
	Out->SetStringField(TEXT("catalogRevision"), Revision);
	Out->SetBoolField(TEXT("hasMore"), bHasMore);
	Out->SetNumberField(TEXT("limit"), Limit);
	Out->SetNumberField(TEXT("effectiveLimit"), Limit);
	Out->SetStringField(TEXT("message"),
		TEXT("Results are capability-level and bounded. Call describe with the exact capability before execute."));
	Out->SetNumberField(TEXT("offset"), Offset);
	Out->SetStringField(TEXT("operation"), TEXT("search"));
	Out->SetStringField(TEXT("query"), Input.Query);
	Out->SetArrayField(TEXT("results"), Results);
	Out->SetBoolField(TEXT("success"), true);
	Out->SetNumberField(TEXT("total"), Total);
	Out->SetBoolField(TEXT("truncated"), bTruncated);
	Out->SetStringField(TEXT("truncationReason"),
		bByteBudgetTruncated ? TEXT("byte-budget") : (bHasMore ? TEXT("limit") : TEXT("none")));
	if (Input.bHasDomain) Out->SetStringField(TEXT("domain"), Input.Domain);
	if (Input.bHasFamily) Out->SetStringField(TEXT("family"), Input.Family);
	if (bHasMore) Out->SetStringField(TEXT("nextCursor"), FString::FromInt(Offset + Results.Num()));
	return Out;
}
