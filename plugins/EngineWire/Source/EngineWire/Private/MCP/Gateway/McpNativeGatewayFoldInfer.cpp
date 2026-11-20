#include "MCP/Gateway/McpNativeGatewayFolding.h"

#include "Dom/JsonObject.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"

// A call that omits the selector runs the variant its parameters point to:
// every sent parameter the variants do not all share must be declared by one
// and the same variant.
void McpInferFoldSelector(const FMcpCapabilityRecord& Record, const TSharedPtr<FJsonObject>& Params)
{
	if (Record.DispatchBySelector.IsEmpty() || !Params.IsValid() || Params->HasField(Record.DispatchBySelector))
	{
		return;
	}
	TOptional<TArray<FString>> Candidates;
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
	if (Candidates.IsSet() && Candidates->Num() == 1)
	{
		Params->SetStringField(Record.DispatchBySelector, (*Candidates)[0]);
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
