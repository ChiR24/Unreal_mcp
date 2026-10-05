#include "MCP/Protocol/McpJsonRpcReplyCompaction.h"

#include "MCP/Protocol/McpJsonRpcReplyFields.h"

namespace McpJsonRpcReplyFields
{
// The idempotency ledger keeps the full receipt; only the copy a client reads loses these.
const TCHAR* const LogOnlyExecuteFields[] = {
	TEXT("capabilityId"), TEXT("capability"), TEXT("tool"), TEXT("action"), TEXT("status"), TEXT("options"),
	TEXT("catalogRevision"), TEXT("capabilityRevision"), TEXT("schemaRevision"), TEXT("correlationId"),
	TEXT("replayedFrom"), TEXT("liveRevisions")};

// What ran, kept when the call named it differently (an alias or another action name) and dropped as an echo otherwise.
const TCHAR* const ResolvedIdentityFields[] = {TEXT("capabilityId"), TEXT("capability"), TEXT("tool"), TEXT("action")};

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

// A payload list (warnings, changedAssets) whose every entry the receipt already lists, at its root and down
// details (where the receipt collects them).
TSharedPtr<FJsonObject> WithoutListed(const TSharedPtr<FJsonObject>& Payload, const TCHAR* Field, const TArray<FString>& Listed)
{
	TSharedPtr<FJsonObject> Out = Copy(Payload);
	const FJsonArray* List = ArrayField(Out, Field);
	if (List && AllListed(*List, Listed)) Out->RemoveField(Field);
	if (const TSharedPtr<FJsonObject> Details = ObjectField(Out, TEXT("details")))
	{
		Out->SetObjectField(TEXT("details"), WithoutListed(Details, Field, Listed));
	}
	return Out;
}

// The projection puts the declared fields at the top of data and the rest in details: a value there (a path or a
// name of 8 or more characters) that repeats a declared field's (the read-back assetPath of a widgetPath, actorName
// beside name) says nothing new; the receipt read its fields from the full result before this.
TSharedPtr<FJsonObject> WithoutRepeatedValues(const TSharedPtr<FJsonObject>& Data)
{
	const TSharedPtr<FJsonObject> Details = ObjectField(Data, TEXT("details"));
	if (!Details) return Data;
	TArray<FString> Declared;
	for (const auto& Field : Data->Values)
	{
		if (Field.Value.IsValid() && Field.Value->Type == EJson::String) Declared.Add(Field.Value->AsString());
	}
	TSharedPtr<FJsonObject> Kept = MakeShared<FJsonObject>();
	for (const auto& Field : Details->Values)
	{
		const FString Key(Field.Key.Len(), *Field.Key);  // 5.8 keys are not FString
		const bool bString = Field.Value.IsValid() && Field.Value->Type == EJson::String;
		const FString Value = bString ? Field.Value->AsString() : FString();
		const bool bRepeat = bString && Value.Len() >= 8 &&
			Declared.ContainsByPredicate([&Value](const FString& Text) { return Text.Equals(Value, ESearchCase::CaseSensitive); });
		if (!bRepeat) Kept->SetField(Key, Field.Value);
	}
	TSharedPtr<FJsonObject> Out = Copy(Data);
	if (Kept->Values.Num() > 0) Out->SetObjectField(TEXT("details"), Kept);
	else Out->RemoveField(TEXT("details"));
	return Out;
}

TArray<FString> StringItems(const FJsonArray* Items)
{
	TArray<FString> Out;
	if (!Items) return Out;
	for (const TSharedPtr<FJsonValue>& Item : *Items)
	{
		if (Item.IsValid() && Item->Type == EJson::String) Out.Add(Item->AsString());
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

// The bridge frame's own fields.
const TCHAR* const DetailBookkeeping[] = {TEXT("type"), TEXT("requestId"), TEXT("liveRevisions")};

// What Unreal reported beside a refusal (typedError.unrealDetail here, the reply's result over stdio), less what
// the reply already says: its message, its code, an empty data, the frame's own fields. Partial results stay.
TSharedPtr<FJsonObject> CompactDetail(const TSharedPtr<FJsonObject>& Detail, const TSharedPtr<FJsonObject>& Reply)
{
	TSharedPtr<FJsonObject> Out = Copy(Detail);
	for (const TCHAR* Field : DetailBookkeeping) Out->RemoveField(Field);
	// A handler that said success while the gateway refused its output keeps saying so.
	if (SameField(Out, TEXT("success"), Reply, TEXT("success"))) Out->RemoveField(TEXT("success"));
	if (SameField(Out, TEXT("message"), Reply, TEXT("message"))) Out->RemoveField(TEXT("message"));
	if (SameField(Out, TEXT("error"), Reply, TEXT("message")) || SameField(Out, TEXT("error"), Reply, TEXT("errorCode")))
	{
		Out->RemoveField(TEXT("error"));
	}
	const TSharedPtr<FJsonObject> Error = ObjectField(Out, TEXT("error"));
	if (SameField(Error, TEXT("message"), Reply, TEXT("message")) && SameField(Error, TEXT("code"), Reply, TEXT("errorCode")))
	{
		Out->RemoveField(TEXT("error"));
	}
	const TSharedPtr<FJsonObject> Data = ObjectField(Out, TEXT("data"));
	if (Data && Data->Values.Num() == 0) Out->RemoveField(TEXT("data"));
	if (const TSharedPtr<FJsonObject> Inner = ObjectField(Out, TEXT("result")))
	{
		const TSharedPtr<FJsonObject> Kept = CompactDetail(Inner, Reply);
		if (Kept) Out->SetObjectField(TEXT("result"), Kept);
		else Out->RemoveField(TEXT("result"));
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
	const TSharedPtr<FJsonObject> Detail = CompactDetail(Unreal, Reply);
	if (Detail) Out->SetObjectField(TEXT("unrealDetail"), Detail);
	else Out->RemoveField(TEXT("unrealDetail"));
	return Out;
}

TSharedPtr<FJsonObject> CompactExecute(const TSharedPtr<FJsonObject>& Reply)
{
	TSharedPtr<FJsonObject> Out = Copy(Reply);
	for (const TCHAR* Field : LogOnlyExecuteFields) Out->RemoveField(Field);
	const TSharedPtr<FJsonObject> Migrated = ObjectField(Reply, TEXT("migratedFrom"));
	const bool bTranslated = Reply->HasField(TEXT("resolvedFromAlias")) || (Migrated &&
		!(SameField(Migrated, TEXT("tool"), Reply, TEXT("tool")) && SameField(Migrated, TEXT("action"), Reply, TEXT("action"))));
	for (const TCHAR* Field : ResolvedIdentityFields)
	{
		if (bTranslated && Reply->HasField(Field)) Out->SetField(Field, Reply->TryGetField(Field));
	}
	if (!bTranslated) Out->RemoveField(TEXT("migratedFrom"));
	if (SameField(Out, TEXT("error"), Out, TEXT("message"))) Out->RemoveField(TEXT("error"));
	// A success's raw `result` only repeats `data`, its projection (stdio sends one; mirrored for parity).
	if (IsTrue(Out, TEXT("success")) && ObjectField(Out, TEXT("data"))) Out->RemoveField(TEXT("result"));
	if (const TSharedPtr<FJsonObject> Detail = IsTrue(Out, TEXT("success")) ? nullptr : ObjectField(Out, TEXT("result")))
	{
		const TSharedPtr<FJsonObject> Kept = CompactDetail(Detail, Out);
		if (Kept) Out->SetObjectField(TEXT("result"), Kept);
		else Out->RemoveField(TEXT("result"));
	}
	const TSharedPtr<FJsonObject> Receipt = ObjectField(Reply, TEXT("receipt"));
	const TSharedPtr<FJsonObject> Outcome = Receipt ? ReceiptOutcome(Receipt, Reply) : nullptr;
	if (Outcome) Out->SetObjectField(TEXT("receipt"), Outcome);
	else Out->RemoveField(TEXT("receipt"));
	if (const TSharedPtr<FJsonObject> Typed = ObjectField(Out, TEXT("typedError")))
	{
		Out->SetObjectField(TEXT("typedError"), CompactTypedError(Typed, Out));
	}
	const TArray<FString> Listed = StringItems(ArrayField(Outcome, TEXT("warnings")));
	const FJsonArray* TopWarnings = ArrayField(Out, TEXT("warnings"));
	if (TopWarnings && AllListed(*TopWarnings, Listed)) Out->RemoveField(TEXT("warnings"));
	if (const TSharedPtr<FJsonObject> Data = ObjectField(Out, TEXT("data")))
	{
		TSharedPtr<FJsonObject> Shown = WithoutRepeatedValues(WithoutListed(
			WithoutListed(Data, TEXT("warnings"), Listed), TEXT("changedAssets"), StringItems(ArrayField(Outcome, TEXT("changes")))));
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
