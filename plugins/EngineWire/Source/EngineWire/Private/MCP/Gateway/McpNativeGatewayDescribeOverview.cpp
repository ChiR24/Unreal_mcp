// McpNativeGatewayDescribeOverview.cpp — describe's listings and schema hygiene.
//
// The browse levels above one capability (catalog -> domain -> family) and the tool summary, each in the shape
// gateway-describe-browse.ts and gateway-describe.ts answer on the TS door. describe {} used to
// answer UNKNOWN_TOOL although nothing else enumerated the canonical parents (dogfood #1), then listed the 23 parents
// while the TS door listed domains; and every capability inputSchema leaked the envelope key `action` as a required
// parameter (dogfood #78/#110).
#include "MCP/Gateway/McpNativeGatewayDescribe.h"
#include "MCP/Gateway/McpNativeGatewayGuidance.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"
#include "MCP/Registry/McpToolDefinition.h"
#include "MCP/Registry/McpToolRegistry.h"

namespace
{
// A functor, not a function: TFunctionRef keeps a pointer to the callable it is handed.
const auto RecordFamily = [](const FMcpCapabilityRecord& Record) -> FString { return Record.Family; };

// Rows [Offset, Offset + Limit) of a listing, and whether more follow (paged in gateway-describe-browse.ts).
template <typename T>
TArray<T> ListingPage(const TArray<T>& Entries, int32 Offset, int32 Limit, bool& bOutHasMore)
{
	TArray<T> Rows;
	for (int32 Index = Offset; Index < Entries.Num() && Rows.Num() < Limit; ++Index) Rows.Add(Entries[Index]);
	bOutHasMore = Offset + Rows.Num() < Entries.Num();
	return Rows;
}

// The rows and the paging fields every listing carries: <rows>, <prefix>Count, Offset, Limit and HasMore.
void SetListing(const TSharedPtr<FJsonObject>& Out, const TCHAR* Rows, const TCHAR* Prefix,
	const TArray<TSharedPtr<FJsonValue>>& Page, int32 Count, int32 Offset, int32 Limit, bool bHasMore)
{
	Out->SetArrayField(Rows, Page);
	Out->SetNumberField(FString(Prefix) + TEXT("Count"), Count);
	Out->SetNumberField(FString(Prefix) + TEXT("Offset"), Offset);
	Out->SetNumberField(FString(Prefix) + TEXT("Limit"), Limit);
	Out->SetBoolField(FString(Prefix) + TEXT("HasMore"), bHasMore);
}

// One domain's records, or one family's in it, in canonical id order.
TArray<const FMcpCapabilityRecord*> RecordsIn(const FMcpCapabilityStore& Store, const FString& Domain, const FString* Family)
{
	TArray<const FMcpCapabilityRecord*> Out;
	for (const FMcpCapabilityRecord& Record : Store.GetRecords())
	{
		if (Record.Domain.Equals(Domain, ESearchCase::CaseSensitive) &&
			(!Family || Record.Family.Equals(*Family, ESearchCase::CaseSensitive))) Out.Add(&Record);
	}
	return Out;
}

TSharedPtr<FJsonObject> BrowseCall(const FString& Domain, const FString& Family)
{
	TSharedPtr<FJsonObject> Next = GatewayBuildNextCall(TEXT("describe"), FString(), FString(), FString());
	if (!Domain.IsEmpty()) Next->SetStringField(TEXT("domain"), Domain);
	if (!Family.IsEmpty()) Next->SetStringField(TEXT("family"), Family);
	return Next;
}

TSharedPtr<FJsonObject> BrowseEnvelope(const TCHAR* Scope, const FMcpCapabilityStore& Store)
{
	auto Out = MakeShared<FJsonObject>();
	Out->SetBoolField(TEXT("success"), true);
	Out->SetStringField(TEXT("operation"), TEXT("describe"));
	Out->SetStringField(TEXT("catalogRevision"), Store.GetCatalogRevision());
	Out->SetStringField(TEXT("scope"), Scope);
	return Out;
}

TSharedPtr<FJsonObject> UnknownDomain(const FMcpCapabilityStore& Store, const FString& Domain)
{
	TSharedPtr<FJsonObject> Error = GatewayError(TEXT("describe"), TEXT("UNKNOWN_DOMAIN"),
		FString::Printf(TEXT("Unknown domain '%s'."), *Domain));
	Error->SetArrayField(TEXT("suggestions"), GatewayStringArray(GatewayClosestMatches(Domain, Store.GetDomains(), 3)));
	Error->SetObjectField(TEXT("nextCall"), BrowseCall(FString(), FString()));
	return Error;
}

TSharedPtr<FJsonObject> UnknownFamily(const FString& Domain, const FString& Family, const TArray<FString>& Families)
{
	TSharedPtr<FJsonObject> Error = GatewayError(TEXT("describe"), TEXT("UNKNOWN_FAMILY"),
		FString::Printf(TEXT("Unknown family '%s' in domain '%s'."), *Family, *Domain));
	Error->SetStringField(TEXT("domain"), Domain);
	Error->SetArrayField(TEXT("availableFamilies"), GatewayStringArray(Families));
	Error->SetArrayField(TEXT("suggestions"), GatewayStringArray(GatewayClosestMatches(Family, Families, 3)));
	Error->SetObjectField(TEXT("nextCall"), BrowseCall(Domain, FString()));
	return Error;
}

TSharedPtr<FJsonObject> DescribeCatalog(const FMcpCapabilityStore& Store, int32 Offset, int32 Limit)
{
	const TArray<FString> Domains = Store.GetDomains();
	bool bHasMore = false;
	TArray<TSharedPtr<FJsonValue>> Rows;
	for (const FString& Domain : ListingPage(Domains, Offset, Limit, bHasMore))
	{
		const TArray<const FMcpCapabilityRecord*> Records = RecordsIn(Store, Domain, nullptr);
		auto Row = MakeShared<FJsonObject>();
		Row->SetStringField(TEXT("domain"), Domain);
		Row->SetNumberField(TEXT("capabilityCount"), Records.Num());
		Row->SetNumberField(TEXT("familyCount"), McpDistinctSorted(Records, RecordFamily).Num());
		Row->SetObjectField(TEXT("nextCall"), BrowseCall(Domain, FString()));
		Rows.Add(MakeShared<FJsonValueObject>(Row));
	}
	TSharedPtr<FJsonObject> Out = BrowseEnvelope(TEXT("catalog"), Store);
	SetListing(Out, TEXT("domains"), TEXT("domain"), Rows, Domains.Num(), Offset, Limit, bHasMore);
	Out->SetNumberField(TEXT("capabilityCount"), Store.GetRecords().Num());
	Out->SetStringField(TEXT("message"), TEXT("Catalog domains. Drill into a domain to list its families, then a family to list capabilities."));
	return Out;
}

TSharedPtr<FJsonObject> DescribeDomain(const FMcpCapabilityStore& Store, const FString& Domain, int32 Offset, int32 Limit)
{
	const TArray<const FMcpCapabilityRecord*> Records = RecordsIn(Store, Domain, nullptr);
	if (Records.Num() == 0) return UnknownDomain(Store, Domain);
	const TArray<FString> Families = McpDistinctSorted(Records, RecordFamily);
	bool bHasMore = false;
	TArray<TSharedPtr<FJsonValue>> Rows;
	for (const FString& Family : ListingPage(Families, Offset, Limit, bHasMore))
	{
		auto Row = MakeShared<FJsonObject>();
		Row->SetStringField(TEXT("family"), Family);
		Row->SetNumberField(TEXT("capabilityCount"), RecordsIn(Store, Domain, &Family).Num());
		Row->SetObjectField(TEXT("nextCall"), BrowseCall(Domain, Family));
		Rows.Add(MakeShared<FJsonValueObject>(Row));
	}
	TSharedPtr<FJsonObject> Out = BrowseEnvelope(TEXT("domain"), Store);
	Out->SetStringField(TEXT("domain"), Domain);
	SetListing(Out, TEXT("families"), TEXT("family"), Rows, Families.Num(), Offset, Limit, bHasMore);
	Out->SetStringField(TEXT("message"), TEXT("Domain families. Drill into a family to list its capabilities."));
	return Out;
}

TSharedPtr<FJsonObject> DescribeFamily(const FMcpCapabilityStore& Store, const FString& Domain, const FString& Family,
	int32 Offset, int32 Limit, FMcpToolEnabledPredicate IsToolEnabled)
{
	const TArray<const FMcpCapabilityRecord*> InDomain = RecordsIn(Store, Domain, nullptr);
	if (InDomain.Num() == 0) return UnknownDomain(Store, Domain);
	const TArray<const FMcpCapabilityRecord*> Records = RecordsIn(Store, Domain, &Family);
	if (Records.Num() == 0) return UnknownFamily(Domain, Family, McpDistinctSorted(InDomain, RecordFamily));
	bool bHasMore = false;
	TArray<TSharedPtr<FJsonValue>> Rows;
	for (const FMcpCapabilityRecord* Record : ListingPage(Records, Offset, Limit, bHasMore))
	{
		auto Row = MakeShared<FJsonObject>();
		Row->SetStringField(TEXT("capability"), Record->Id);
		Row->SetStringField(TEXT("parentTool"), Record->Parent);
		Row->SetStringField(TEXT("action"), McpCapabilityPublicAction(*Record));
		Row->SetStringField(TEXT("summary"), Record->Summary);
		Row->SetStringField(TEXT("effect"), Record->Effect);
		Row->SetStringField(TEXT("availability"), IsToolEnabled(Record->Parent) ? TEXT("available") : TEXT("disabled"));
		const TSharedPtr<FJsonObject> Next = GatewayBuildNextCall(TEXT("describe"), FString(), FString(), FString());
		Next->SetStringField(TEXT("capability"), Record->Id);
		Row->SetObjectField(TEXT("nextCall"), Next);
		Rows.Add(MakeShared<FJsonValueObject>(Row));
	}
	TSharedPtr<FJsonObject> Out = BrowseEnvelope(TEXT("family"), Store);
	Out->SetStringField(TEXT("domain"), Domain);
	Out->SetStringField(TEXT("family"), Family);
	SetListing(Out, TEXT("capabilities"), TEXT("capability"), Rows, Records.Num(), Offset, Limit, bHasMore);
	Out->SetStringField(TEXT("message"), TEXT("Family capabilities. Drill into a capability for its exact contract."));
	return Out;
}
}

TSharedPtr<FJsonObject> McpGatewayDescribeBrowse(
	const FMcpDiscoveryQuery& Input, const FMcpCapabilityStore& Store, FMcpToolEnabledPredicate IsToolEnabled)
{
	const int32 Limit = Input.bHasLimit ? FMath::Clamp(Input.Limit, 1, McpDescribeMaxLimit) : McpDescribeMaxLimit;
	const int32 Offset = FMath::Max(0, Input.Offset);
	if (Input.bHasFamily)
	{
		// A family without a domain belongs to the first domain, in name order, that has it (resolveDomainForFamily).
		FString Domain = Input.bHasDomain ? Input.Domain : FString();
		for (const FString& Candidate : Store.GetDomains())
		{
			if (Domain.IsEmpty() && RecordsIn(Store, Candidate, &Input.Family).Num() > 0) Domain = Candidate;
		}
		return Domain.IsEmpty() ? UnknownFamily(Domain, Input.Family, TArray<FString>())
			: DescribeFamily(Store, Domain, Input.Family, Offset, Limit, IsToolEnabled);
	}
	return Input.bHasDomain ? DescribeDomain(Store, Input.Domain, Offset, Limit) : DescribeCatalog(Store, Offset, Limit);
}

TSharedPtr<FJsonObject> McpGatewayToolSummary(
	const FString& Tool, const TArray<FString>& Actions, const FMcpDiscoveryQuery& Input, bool bEnabled)
{
	const FString Query = Input.Query.TrimStartAndEnd().ToLower();
	TArray<FString> Filtered;
	for (const FString& Action : Actions)
	{
		if (Query.IsEmpty() || Action.ToLower().Contains(Query, ESearchCase::CaseSensitive)) Filtered.Add(Action);
	}
	// Action names are short, so the whole list unless a page is asked for: limit stops at 50 and a parent can carry
	// more actions than one page (describeToolSummary in gateway-describe.ts).
	const int32 Limit = Input.bHasLimit ? FMath::Clamp(Input.Limit, 1, McpDescribeMaxLimit) : Filtered.Num();
	const int32 Offset = FMath::Max(0, Input.Offset);
	bool bHasMore = false;
	const TArray<FString> Paged = ListingPage(Filtered, Offset, Limit, bHasMore);
	const FMcpToolDefinition* Definition = FMcpToolRegistry::Get().FindTool(Tool);
	auto Out = MakeShared<FJsonObject>();
	Out->SetBoolField(TEXT("success"), true);
	Out->SetStringField(TEXT("operation"), TEXT("describe"));
	Out->SetStringField(TEXT("tool"), Tool);
	Out->SetStringField(TEXT("category"), Definition ? Definition->GetCategory() : FString());
	Out->SetStringField(TEXT("description"), Definition ? Definition->GetDescription() : FString());
	Out->SetBoolField(TEXT("enabled"), bEnabled);
	SetListing(Out, TEXT("actions"), TEXT("action"), GatewayStringArray(Paged), Filtered.Num(), Offset, Limit, bHasMore);
	Out->SetStringField(TEXT("scope"), TEXT("tool"));
	Out->SetBoolField(TEXT("perActionSchemas"), false);
	const FString First = Paged.Num() > 0 ? Paged[0] : (Actions.Num() > 0 ? Actions[0] : FString());
	Out->SetObjectField(TEXT("drillDown"), GatewayBuildNextCall(TEXT("describe"), Tool, First, FString()));
	Out->SetObjectField(TEXT("browse"), GatewayBuildNextCall(TEXT("search"), Tool, FString(), FString()));
	Out->SetStringField(TEXT("message"), TEXT("Legacy parent-tool summary: action names only. To choose one, send the browse call (search filtered to this tool; add query words and read the row summaries), or drill into an action for its exact contract."));
	return Out;
}

TSharedPtr<FJsonObject> McpStripActionFromInputSchema(const TSharedPtr<FJsonObject>& Schema)
{
	if (!Schema.IsValid())
	{
		return Schema;
	}
	TSharedPtr<FJsonObject> Copy = MakeShared<FJsonObject>();
	Copy->Values = Schema->Values;
	const TSharedPtr<FJsonObject>* Properties = nullptr;
	if (Copy->TryGetObjectField(TEXT("properties"), Properties) && Properties && (*Properties)->HasField(TEXT("action")))
	{
		TSharedPtr<FJsonObject> PropertiesCopy = MakeShared<FJsonObject>();
		PropertiesCopy->Values = (*Properties)->Values;
		PropertiesCopy->RemoveField(TEXT("action"));
		Copy->SetObjectField(TEXT("properties"), PropertiesCopy);
	}
	const TArray<TSharedPtr<FJsonValue>>* Required = nullptr;
	if (Copy->TryGetArrayField(TEXT("required"), Required) && Required)
	{
		TArray<TSharedPtr<FJsonValue>> Kept;
		for (const TSharedPtr<FJsonValue>& Value : *Required)
		{
			if (Value.IsValid() && Value->Type == EJson::String && Value->AsString() == TEXT("action"))
			{
				continue;
			}
			Kept.Add(Value);
		}
		Copy->SetArrayField(TEXT("required"), Kept);
	}
	return Copy;
}
