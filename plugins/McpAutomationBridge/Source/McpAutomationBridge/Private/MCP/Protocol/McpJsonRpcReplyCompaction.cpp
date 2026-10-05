#include "MCP/Protocol/McpJsonRpcReplyCompaction.h"

#include "MCP/Protocol/McpJsonRpcReplyFields.h"

namespace McpJsonRpcReplyFields
{
// The idempotency ledger keeps the full receipt; only the copy a client reads loses these.
const TCHAR* const LogOnlyExecuteFields[] = {
	TEXT("capabilityId"), TEXT("capability"), TEXT("tool"), TEXT("action"), TEXT("status"), TEXT("options"),
	TEXT("catalogRevision"), TEXT("capabilityRevision"), TEXT("schemaRevision"), TEXT("correlationId"),
	TEXT("replayedFrom"), TEXT("liveRevisions")};

// The receipt fields that say what the call did; the rest of a receipt is bookkeeping.
const TCHAR* const ReceiptOutcomeLists[] = {TEXT("nextCalls"), TEXT("handles"), TEXT("changes"), TEXT("warnings")};

bool AllListed(const FJsonArray& Items, const TArray<FString>& Listed)
{
	for (const TSharedPtr<FJsonValue>& Item : Items)
	{
		const bool bListed = Item.IsValid() && Item->Type == EJson::String && Listed.ContainsByPredicate(
			[&Item](const FString& Text) { return Text.Equals(Item->AsString(), ESearchCase::CaseSensitive); });
		if (!bListed) return false;
	}
	return true;
}

// A payload's warnings that the receipt already lists, at its root and down details (where the receipt collects them).
TSharedPtr<FJsonObject> WithoutListedWarnings(const TSharedPtr<FJsonObject>& Payload, const TArray<FString>& Listed)
{
	TSharedPtr<FJsonObject> Out = Copy(Payload);
	const FJsonArray* Warnings = ArrayField(Out, TEXT("warnings"));
	if (Warnings && AllListed(*Warnings, Listed)) Out->RemoveField(TEXT("warnings"));
	if (const TSharedPtr<FJsonObject> Details = ObjectField(Out, TEXT("details")))
	{
		Out->SetObjectField(TEXT("details"), WithoutListedWarnings(Details, Listed));
	}
	return Out;
}

TSharedPtr<FJsonObject> ReceiptOutcome(const TSharedPtr<FJsonObject>& Receipt, const TSharedPtr<FJsonObject>& Reply)
{
	TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
	for (const TCHAR* Field : ReceiptOutcomeLists)
	{
		const FJsonArray* List = ArrayField(Receipt, Field);
		if (List && List->Num() > 0) Out->SetArrayField(Field, *List);
	}
	if (const TSharedPtr<FJsonObject> Task = ObjectField(Receipt, TEXT("task"))) Out->SetObjectField(TEXT("task"), Task);
	// Over the bridge the receipt's error repeats typedError; it is kept only for what it alone says.
	if (const TSharedPtr<FJsonObject> Error = ObjectField(Receipt, TEXT("error")))
	{
		const TSharedPtr<FJsonObject> Typed = ObjectField(Reply, TEXT("typedError"));
		TSharedPtr<FJsonObject> Kept = MakeShared<FJsonObject>();
		for (const auto& Field : Error->Values)
		{
			const FString Key(Field.Key.Len(), *Field.Key);  // 5.8 keys are not FString
			if (!SameField(Error, Key, Typed, Key) && !SameField(Error, Key, Reply, Key)) Kept->SetField(Key, Field.Value);
		}
		if (Kept->Values.Num() > 0) Out->SetObjectField(TEXT("error"), Kept);
	}
	return Out->Values.Num() > 0 ? Out : nullptr;
}

TSharedPtr<FJsonObject> CompactTypedError(const TSharedPtr<FJsonObject>& Typed, const TSharedPtr<FJsonObject>& Reply)
{
	TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
	for (const auto& Field : Typed->Values)
	{
		const FString Key(Field.Key.Len(), *Field.Key);
		if (!SameField(Typed, Key, Reply, Key)) Out->SetField(Key, Field.Value);
	}
	if (SameField(Out, TEXT("handlerCode"), Reply, TEXT("errorCode"))) Out->RemoveField(TEXT("handlerCode"));
	const TSharedPtr<FJsonObject> Unreal = ObjectField(Out, TEXT("unrealDetail"));
	if (!Unreal) return Out;
	TSharedPtr<FJsonObject> Detail = Without(Unreal, TEXT("success"));
	const TSharedPtr<FJsonObject> Error = ObjectField(Detail, TEXT("error"));
	if (SameField(Error, TEXT("message"), Reply, TEXT("message")) && SameField(Error, TEXT("code"), Reply, TEXT("errorCode")))
	{
		Detail->RemoveField(TEXT("error"));
	}
	const TSharedPtr<FJsonObject> Data = ObjectField(Detail, TEXT("data"));
	if (Data && Data->Values.Num() == 0) Detail->RemoveField(TEXT("data"));
	if (Detail->Values.Num() > 0) Out->SetObjectField(TEXT("unrealDetail"), Detail);
	else Out->RemoveField(TEXT("unrealDetail"));
	return Out;
}

TSharedPtr<FJsonObject> CompactExecute(const TSharedPtr<FJsonObject>& Reply)
{
	TSharedPtr<FJsonObject> Out = Copy(Reply);
	for (const TCHAR* Field : LogOnlyExecuteFields) Out->RemoveField(Field);
	if (SameField(Out, TEXT("error"), Out, TEXT("message"))) Out->RemoveField(TEXT("error"));
	const TSharedPtr<FJsonObject> Receipt = ObjectField(Reply, TEXT("receipt"));
	const TSharedPtr<FJsonObject> Outcome = Receipt ? ReceiptOutcome(Receipt, Reply) : nullptr;
	if (Outcome) Out->SetObjectField(TEXT("receipt"), Outcome);
	else Out->RemoveField(TEXT("receipt"));
	if (const TSharedPtr<FJsonObject> Typed = ObjectField(Out, TEXT("typedError")))
	{
		Out->SetObjectField(TEXT("typedError"), CompactTypedError(Typed, Out));
	}
	TArray<FString> Listed;
	if (const FJsonArray* Warnings = ArrayField(Outcome, TEXT("warnings")))
	{
		for (const TSharedPtr<FJsonValue>& Warning : *Warnings)
		{
			if (Warning.IsValid() && Warning->Type == EJson::String) Listed.Add(Warning->AsString());
		}
	}
	const FJsonArray* TopWarnings = ArrayField(Out, TEXT("warnings"));
	if (TopWarnings && AllListed(*TopWarnings, Listed)) Out->RemoveField(TEXT("warnings"));
	if (const TSharedPtr<FJsonObject> Data = ObjectField(Out, TEXT("data")))
	{
		TSharedPtr<FJsonObject> Shown = WithoutListedWarnings(Data, Listed);
		if (SameField(Shown, TEXT("success"), Out, TEXT("success"))) Shown->RemoveField(TEXT("success"));
		if (SameField(Shown, TEXT("message"), Out, TEXT("message"))) Shown->RemoveField(TEXT("message"));
		Out->SetObjectField(TEXT("data"), Shown);
	}
	return Out;
}
}

TSharedPtr<FJsonObject> McpJsonRpcReply::MakeCompactReply(const TSharedPtr<FJsonObject>& Reply)
{
	FString Operation;
	if (!Reply.IsValid() || !Reply->TryGetStringField(TEXT("operation"), Operation)) return Reply;
	if (Operation.Equals(TEXT("execute"), ESearchCase::CaseSensitive)) return McpJsonRpcReplyFields::CompactExecute(Reply);
	if (Operation.Equals(TEXT("describe"), ESearchCase::CaseSensitive)) return McpJsonRpcReplyFields::CompactDescribe(Reply);
	if (Operation.Equals(TEXT("search"), ESearchCase::CaseSensitive)) return McpJsonRpcReplyFields::CompactSearch(Reply);
	return Reply;
}
