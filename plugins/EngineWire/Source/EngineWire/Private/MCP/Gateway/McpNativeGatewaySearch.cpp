// McpNativeGatewaySearch.cpp — see header for the cross-surface contract.

#include "MCP/Gateway/McpNativeGatewaySearch.h"
#include "Domains/Scripts/McpRecipes.h"
#include "MCP/Gateway/McpNativeGatewayCanonicalJson.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"
#include "MCP/Gateway/McpNativeGatewayCatalog.h"
#include "MCP/Gateway/McpNativeGatewayGuidance.h"
#include "MCP/Gateway/McpNativeGatewaySearchMatch.h"
#include "MCP/Execute/McpNativeGatewayCanonicalRecords.h"
#include "MCP/Execute/Request/McpFoldVariantRules.h"

namespace
{
struct FScoredRecord
{
	const FMcpCapabilityRecord* Record = nullptr;
	int32 Score = 0;
};

bool HasExactly(const TArray<FString>& Values, const FString& Value)
{
	return Values.ContainsByPredicate([&Value](const FString& Entry) { return Entry.Equals(Value, ESearchCase::CaseSensitive); });
}

// A rejected filter hands back the same search with that filter replaced by its closest declared value
// (guidedFilterError in gateway-search-filters.ts).
TSharedPtr<FJsonObject> GuidedFilterError(const FString& ErrorCode, const FString& Message, const TCHAR* Dimension,
	const FString& Target, const TArray<FString>& Candidates, const FString& Domain)
{
	TSharedPtr<FJsonObject> Error = GatewayError(TEXT("search"), ErrorCode, Message);
	const TArray<FString> Suggestions = GatewayClosestMatches(Target, Candidates, 3);
	Error->SetArrayField(TEXT("suggestions"), GatewayStringArray(Suggestions));
	TSharedPtr<FJsonObject> Next = GatewayBuildNextCall(TEXT("search"), FString(), FString(), FString());
	if (!Domain.IsEmpty()) Next->SetStringField(TEXT("domain"), Domain);
	if (Suggestions.Num() > 0) Next->SetStringField(Dimension, Suggestions[0]);
	Error->SetObjectField(TEXT("nextCall"), Next);
	return Error;
}

// A row is a reply, not a fingerprint: a confident hit's signature example (playRate 1.25) failed the whole search.
bool RenderedLength(const TSharedPtr<FJsonObject>& View, int32& OutLength)
{
	FString Rendered;
	if (!McpCanonicalJsonObject(View, Rendered, /*bAllowFractions=*/true)) return false;
	OutLength = Rendered.Len();
	return true;
}

// The offset a nextCursor names, "c:<offset>" (decodeCursor in gateway-search-filters.ts). At most 9 digits, so the
// offset stays an int32: the catalog holds a few hundred records.
bool DecodeCursor(const FString& Cursor, int32& OutOffset)
{
	const FString Digits = Cursor.StartsWith(TEXT("c:"), ESearchCase::CaseSensitive) ? Cursor.RightChop(2) : FString();
	if (Digits.IsEmpty() || Digits.Len() > 9) return false;
	for (const TCHAR Ch : Digits)
	{
		if (Ch < TEXT('0') || Ch > TEXT('9')) return false;
	}
	OutOffset = FCString::Atoi(*Digits);
	return true;
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
	int32 Offset = FMath::Max(0, Input.Offset);
	const FString& Revision = Store.GetCatalogRevision();

	// Every filter is checked before ranking, in the TS order and case-sensitively (validateFilters).
	if (Input.bHasDomain && !HasExactly(Store.GetDomains(), Input.Domain))
	{
		return GuidedFilterError(TEXT("UNKNOWN_DOMAIN"),
			FString::Printf(TEXT("Unknown domain '%s'. Call describe with no selector to list domains."), *Input.Domain),
			TEXT("domain"), Input.Domain, Store.GetDomains(), FString());
	}
	if (Input.bHasFamily)
	{
		// The families of the domain filter when there is one.
		TArray<const FMcpCapabilityRecord*> InScope;
		for (const FMcpCapabilityRecord& Record : Store.GetRecords())
		{
			if (!Input.bHasDomain || Record.Domain.Equals(Input.Domain, ESearchCase::CaseSensitive)) InScope.Add(&Record);
		}
		const TArray<FString> Families = McpDistinctSorted(InScope, [](const FMcpCapabilityRecord& R) -> FString { return R.Family; });
		if (!HasExactly(Families, Input.Family))
		{
			return GuidedFilterError(TEXT("UNKNOWN_FAMILY"),
				FString::Printf(TEXT("Unknown family '%s'. Call describe on the domain to list its families."), *Input.Family),
				TEXT("family"), Input.Family, Families, Input.bHasDomain ? Input.Domain : FString());
		}
	}
	if (Input.bHasTool && !HasExactly(Store.GetParents(), Input.Tool))
	{
		return GuidedFilterError(TEXT("UNKNOWN_TOOL"),
			FString::Printf(TEXT("Unknown tool '%s'. Call search without a tool filter to browse capabilities."), *Input.Tool),
			TEXT("tool"), Input.Tool, Store.GetParents(), FString());
	}
	if (Input.bHasEffect)
	{
		TArray<const FMcpCapabilityRecord*> All;
		for (const FMcpCapabilityRecord& Record : Store.GetRecords()) All.Add(&Record);
		const TArray<FString> Effects = McpDistinctSorted(All, [](const FMcpCapabilityRecord& R) -> FString { return R.Effect; });
		if (!HasExactly(Effects, Input.Effect))
		{
			return GuidedFilterError(TEXT("UNKNOWN_EFFECT"),
				FString::Printf(TEXT("Unknown effect '%s'. Declared effects: %s."), *Input.Effect, *FString::Join(Effects, TEXT(", "))),
				TEXT("effect"), Input.Effect, Effects, FString());
		}
	}

	if (Input.bHasCursor && !DecodeCursor(Input.Cursor, Offset))
	{
		TSharedPtr<FJsonObject> Error = GatewayError(TEXT("search"), TEXT("INVALID_CURSOR"),
			TEXT("cursor is not a gateway search cursor. Re-run search without a cursor."));
		Error->SetObjectField(TEXT("nextCall"), GatewayBuildNextCall(TEXT("search"), FString(), FString(), FString()));
		return Error;
	}

	// An exact capability (or a unique alias) or an exact tool + action pair is that one record; an action alone matches
	// either name a caller can mean, the bridge verb or a declared pair (selectCandidates in gateway-search-filters.ts).
	const FMcpCanonicalRecordIndex& Canonical = FMcpCanonicalRecordIndex::Get();
	const bool bExact = Input.bHasCapability || (Input.bHasTool && Input.bHasAction);
	FString ExactId;
	if (Input.bHasCapability)
	{
		ExactId = Input.Capability;
		FString AliasTarget;
		if (!Canonical.FindById(ExactId) && Canonical.ResolveAlias(Input.Capability, AliasTarget) == EMcpAliasResolution::Unique) ExactId = AliasTarget;
	}
	else if (bExact)
	{
		const FMcpCapabilityRecord* Legacy = Canonical.FindByLegacy(Input.Tool, Input.Action);
		ExactId = Legacy ? Legacy->Id : FString();
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
		if (bExact)
		{
			if (!Record.Id.Equals(ExactId, ESearchCase::CaseSensitive)) continue;
		}
		else if ((Input.bHasDomain && !Record.Domain.Equals(Input.Domain, ESearchCase::CaseSensitive))
			|| (Input.bHasFamily && !Record.Family.Equals(Input.Family, ESearchCase::CaseSensitive))
			|| (Input.bHasTool && !Record.Parent.Equals(Input.Tool, ESearchCase::CaseSensitive))
			|| (Input.bHasEffect && !Record.Effect.Equals(Input.Effect, ESearchCase::CaseSensitive))
			|| (Input.bHasAction && !Record.DispatchAction.Equals(Input.Action, ESearchCase::CaseSensitive)
				&& !Record.LegacyPairs.ContainsByPredicate([&Input](const FMcpLegacyPair& Pair) { return Pair.Action.Equals(Input.Action, ESearchCase::CaseSensitive); })))
		{
			continue;
		}

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
		Scored.Add(MoveTemp(Entry));
	}

	// Strict total order: unique ids make the result independent of TArray::Sort
	// being an unstable introsort.
	Scored.Sort([](const FScoredRecord& L, const FScoredRecord& R)
	{
		if (L.Score != R.Score) return L.Score > R.Score;
		return L.Record->Id.Compare(R.Record->Id, ESearchCase::CaseSensitive) < 0;
	});

	// A clear first hit that needs no consent carries its signature and the execute call, so a caller can skip
	// describe (withSignature in gateway-search.ts); consent still needs describe's grant.
	FString TopConsent;
	const bool bConfident = !Query.IsEmpty() && Offset == 0 && Scored.Num() > 0 && Scored[0].Score > 0 &&
		(Scored.Num() == 1 || Scored[1].Score * 100 <= Scored[0].Score * McpSearchConfidentRatioPercent) &&
		Scored[0].Record->Policy.IsValid() && Scored[0].Record->Policy->TryGetStringField(TEXT("consent"), TopConsent) &&
		TopConsent.Equals(TEXT("none"), ESearchCase::CaseSensitive) && IsToolEnabled(Scored[0].Record->Parent);

	const int32 Total = Scored.Num();
	TArray<TSharedPtr<FJsonValue>> Results;
	const int32 Budget = Input.MaxBytes > 0 ? FMath::Clamp(Input.MaxBytes, 512, 262144) : McpMaxResultBytes;
	int32 Bytes = 0;
	bool bTruncated = false;
	bool bBudgetExceeded = false;
	bool bSigned = false;
	for (int32 Index = Offset; Index < Total && Results.Num() < Limit; ++Index)
	{
		const FMcpCapabilityRecord& Record = *Scored[Index].Record;
		const TSharedPtr<FJsonObject> View = McpSearchRow(Record, IsToolEnabled(Record.Parent));
		if (Index == 0 && bConfident)
		{
			View->SetObjectField(TEXT("signature"), McpSearchSignature(Record));
			// The execute call describe hands over (CapabilityContract in McpNativeGatewayDescribe.cpp).
			const TSharedPtr<FJsonObject> Execute = GatewayBuildNextCall(
				TEXT("execute"), Record.Parent, McpCapabilityPublicAction(Record), FString());
			Execute->SetStringField(TEXT("capability"), Record.Id);
			Execute->SetObjectField(TEXT("params"), MakeShared<FJsonObject>());
			View->SetObjectField(TEXT("executeCall"), Execute);
		}
		int32 Size = 0;
		bool bRendered = RenderedLength(View, Size);
		// The first row's signature goes before the row does; a page no row fits says so (withinByteBudget in TS).
		if (bRendered && Results.Num() == 0 && Size > Budget && View->HasField(TEXT("signature")))
		{
			View->RemoveField(TEXT("signature"));
			View->RemoveField(TEXT("executeCall"));
			bRendered = RenderedLength(View, Size);
		}
		if (!bRendered)
		{
			return GatewayError(TEXT("search"), TEXT("CAPABILITY_RENDER_FAILED"),
				FString::Printf(TEXT("Capability '%s' could not be rendered deterministically."), *Record.Id));
		}
		if (Bytes + Size > Budget)
		{
			bTruncated = true;
			bBudgetExceeded = Results.Num() == 0;
			break;
		}
		Bytes += Size;
		bSigned = bSigned || View->HasField(TEXT("signature"));
		Results.Add(MakeShared<FJsonValueObject>(View));
	}

	const bool bHasMore = Offset + Results.Num() < Total;
	auto Out = MakeShared<FJsonObject>();
	Out->SetBoolField(TEXT("success"), true);
	Out->SetStringField(TEXT("operation"), TEXT("search"));
	Out->SetStringField(TEXT("catalogRevision"), Revision);
	Out->SetStringField(TEXT("query"), Input.Query);
	Out->SetObjectField(TEXT("filters"), McpSearchFilters(Input));
	Out->SetArrayField(TEXT("results"), Results);
	Out->SetNumberField(TEXT("total"), Total);
	Out->SetNumberField(TEXT("offset"), Offset);
	Out->SetNumberField(TEXT("limit"), Limit);
	Out->SetNumberField(TEXT("maxBytes"), Budget);
	Out->SetBoolField(TEXT("hasMore"), bHasMore);
	Out->SetStringField(TEXT("truncationReason"),
		bTruncated ? TEXT("byte-budget") : (bHasMore ? TEXT("limit") : TEXT("none")));
	Out->SetStringField(TEXT("message"), bSigned
		? TEXT("The first result carries signature and executeCall: fill params from the signature and send executeCall, or describe it first for the full contract.")
		: TEXT("Results are compact. Call describe with an exact capability before execute."));
	if (bHasMore && !bBudgetExceeded) Out->SetStringField(TEXT("nextCursor"), FString::Printf(TEXT("c:%d"), Offset + Results.Num()));
	// Not even one row fits: a cursor would name the page just read, so the reply says the one thing that helps.
	if (bBudgetExceeded)
	{
		Out->SetBoolField(TEXT("budgetExceeded"), true);
		Out->SetStringField(TEXT("message"), FString::Printf(
			TEXT("No result fits maxBytes=%d. Raise maxBytes and retry; this page cannot advance at the current budget."), Budget));
	}
	// An empty page is where a caller starts inventing names: say what to change
	// and hand over the one call that always works.
	if (Results.Num() == 0 && Total == 0)
	{
		Out->SetStringField(TEXT("message"), Input.Query.IsEmpty()
			? FString(TEXT("No capability matches these filters. Remove a filter, or call describe with no selector to browse domains."))
			: FString::Printf(TEXT("No capability matched '%s'. Use 2-4 plain words naming the verb and the object (e.g. 'spawn actor'), drop any filter, or call describe with no selector to browse domains. Game logic no capability makes in one call is built as a Blueprint graph: search 'blueprint logic'."), *Input.Query));
		Out->SetObjectField(TEXT("nextCall"), GatewayBuildNextCall(TEXT("describe"), FString(), FString(), FString()));
	}
	// Saved recipes (system_control save_recipe) the request names, on an unfiltered first page (McpRecipesSearch.cpp).
	// They are project data, so only for a caller list_recipes would answer (Read scope or Admin).
	const bool bFiltered = bExact || Input.bHasDomain || Input.bHasFamily || Input.bHasTool || Input.bHasAction || Input.bHasEffect;
	const TArray<TSharedPtr<FJsonValue>> Recipes = !Input.bMayReadProject || bFiltered || Offset > 0 ? TArray<TSharedPtr<FJsonValue>>() : McpRecipes::SearchRows(ContentWords, 3);
	if (Recipes.Num() > 0) Out->SetArrayField(TEXT("recipes"), Recipes);
	if (Input.Coercions.Num() > 0) Out->SetArrayField(TEXT("coercions"), Input.Coercions);
	return Out;
}
