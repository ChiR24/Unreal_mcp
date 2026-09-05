// McpNativeGatewaySearch.cpp — see header for the cross-surface contract.

#include "MCP/Gateway/McpNativeGatewaySearch.h"
#include "MCP/Gateway/McpNativeGatewayCanonicalJson.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"
#include "MCP/Gateway/McpNativeGatewayGuidance.h"
#include "MCP/Gateway/McpNativeGatewaySearchMatch.h"

namespace
{
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

	// Query words are ASCII alphanumeric runs (McpSearchWords), the same split the
	// TypeScript reference applies; matching is word-level on both surfaces.
	TArray<FString> AllWords;
	McpSearchWords(Query, AllWords);
	TArray<FString> ContentWords;
	McpSearchContentWords(AllWords, ContentWords);

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
		FMcpSearchMatch Match;
		if (!McpSearchScoreRecord(Record, Query, AllWords, ContentWords, Match)) continue;
		Entry.Score = Match.Score;
		Entry.Reasons = Match.Reasons;
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
	const int32 Budget = Input.MaxBytes > 0 ? FMath::Clamp(Input.MaxBytes, 512, 262144) : McpMaxResultBytes;
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
		if (Bytes + Size > Budget && Results.Num() > 0)
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
	// An empty page is where a caller starts inventing names: say what to change
	// and hand over the one call that always works.
	if (Results.Num() == 0 && Total == 0)
	{
		Out->SetStringField(TEXT("message"), Query.IsEmpty()
			? FString(TEXT("No capability matches these filters. Remove a filter, or call describe with no selector to browse."))
			: FString::Printf(TEXT("No capability matched '%s'. Use 2-4 plain words naming the verb and the object (e.g. 'spawn actor'), drop any filter, or call describe with no selector to browse."), *Query));
		Out->SetObjectField(TEXT("nextCall"), GatewayBuildNextCall(TEXT("describe"), FString(), FString(), FString()));
	}
	if (Input.bHasDomain) Out->SetStringField(TEXT("domain"), Input.Domain);
	if (Input.bHasFamily) Out->SetStringField(TEXT("family"), Input.Family);
	if (bHasMore) Out->SetStringField(TEXT("nextCursor"), FString::FromInt(Offset + Results.Num()));
	return Out;
}
