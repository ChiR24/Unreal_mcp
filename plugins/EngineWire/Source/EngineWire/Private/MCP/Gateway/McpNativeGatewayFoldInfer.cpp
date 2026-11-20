#include "MCP/Gateway/McpNativeGatewayFolding.h"

#include "Dom/JsonObject.h"
#include "MCP/Execute/McpNativeGatewayCanonicalRecords.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"

FString McpToolActionFitting(const FString& Tool, const TSharedPtr<FJsonObject>& Sent)
{
	const FMcpCanonicalRecordIndex& Index = FMcpCanonicalRecordIndex::Get();
	const FString Parent = Index.ResolveToolNamespace(Tool);
	FString Fit;
	for (const FMcpCapabilityRecord& Record : FMcpCapabilityStore::Get().GetRecords())
	{
		const TSharedPtr<FJsonObject>* Properties = nullptr;
		if (Record.Parent != Parent || !Sent.IsValid() || Sent->Values.Num() == 0 || !Record.InputSchema.IsValid() ||
			!Record.InputSchema->TryGetObjectField(TEXT("properties"), Properties))
		{
			continue;
		}
		bool bDeclaresAll = true;
		for (const auto& Param : Sent->Values)
		{
			bDeclaresAll &= (*Properties)->HasField(FString(*Param.Key));
		}
		if (bDeclaresAll)
		{
			if (!Fit.IsEmpty())
			{
				return FString(); // several fit: the action list decides
			}
			Fit = Index.GetLegacyActionForCapability(Record.Id);
		}
	}
	return Fit;
}

TArray<FString> McpFoldSelectorCandidates(const FMcpCapabilityRecord& Record, const TSharedPtr<FJsonObject>& Params)
{
	TOptional<TArray<FString>> Candidates;
	if (!Params.IsValid())
	{
		return {};
	}
	for (const auto& Param : Params->Values)
	{
		if (const TArray<FString>* Owners = Record.DispatchByDeclaredBy.Find(FString(*Param.Key)))
		{
			if (!Candidates.IsSet())
			{
				Candidates = *Owners;
			}
			Candidates->RemoveAll([Owners](const FString& Value) { return !Owners->Contains(Value); });
		}
	}
	return Candidates.Get(TArray<FString>());
}

// A call that omits the selector runs the variant its parameters point to:
// every sent parameter the variants do not all share must be declared by one
// and the same variant.
void McpInferFoldSelector(const FMcpCapabilityRecord& Record, const TSharedPtr<FJsonObject>& Params)
{
	if (Record.DispatchBySelector.IsEmpty() || !Params.IsValid() || Params->HasField(Record.DispatchBySelector))
	{
		return;
	}
	const TArray<FString> Candidates = McpFoldSelectorCandidates(Record, Params);
	if (Candidates.Num() == 1)
	{
		Params->SetStringField(Record.DispatchBySelector, Candidates[0]);
		return;
	}
	// A family whose primary is none of its members runs the variant its selector's schema default names when the call
	// leaves the selector out (get_editor_state: world_settings); a primary that is a member dispatches itself.
	const TSharedPtr<FJsonObject>* Properties = nullptr;
	const TSharedPtr<FJsonObject>* Selector = nullptr;
	FString Default;
	if (!Record.DispatchByActions.FindKey(Record.DispatchAction) && Record.InputSchema.IsValid() &&
		Record.InputSchema->TryGetObjectField(TEXT("properties"), Properties) &&
		(*Properties)->TryGetObjectField(Record.DispatchBySelector, Selector) &&
		(*Selector)->TryGetStringField(TEXT("default"), Default) && Record.DispatchByActions.Contains(Default))
	{
		Params->SetStringField(Record.DispatchBySelector, Default);
	}
}
