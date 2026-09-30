#include "MCP/Execute/McpNativeReceiptOutcome.h"
#include "MCP/Resources/McpResourceUri.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"
// McpNativeReceiptOutcome.cpp — see header for the parity contract.


namespace
{
bool IsAllowedPath(const FString& Path)
{
	if (Path.IsEmpty() || Path.Len() > 512 || Path.Contains(TEXT("..")))
	{
		return false;
	}
	for (const FString& Root : McpResourceUri::ContentRoots())
	{
		if (Path == Root || Path.StartsWith(Root + TEXT("/")))
		{
			return true;
		}
	}
	return false;
}

// The running game's objects (GameInstance, a live widget) live in the transient package, so a
// handler names /Engine/Transient as their "asset"; no asset changed. Mirrors
// isTransientPackagePath in src/tools/catalog/capabilities/semantic/receipt-outcome.ts.
bool IsTransientPackagePath(const FString& Path)
{
	return Path == TEXT("/Engine/Transient") || Path.StartsWith(TEXT("/Engine/Transient.")) ||
		   Path.StartsWith(TEXT("/Engine/Transient:"));
}

bool IsRefKind(const FString& Kind)
{
	return Kind == TEXT("actor") || Kind == TEXT("component") || Kind == TEXT("node");
}

bool IsPathKind(const FString& Kind)
{
	return Kind == TEXT("object") || Kind == TEXT("asset") || Kind == TEXT("class");
}

// Read a field from the result root, then its nested `data` payload (the native
// completion carries the verdict separately from the payload), then `details`
// (the gateway folds undeclared handler fields into it) and finally the
// handler's own nested `data.details`. Field order (root, data, details,
// data.details) matches the TypeScript makeReader order exactly, so both
// transports read the same winner.
TSharedPtr<FJsonValue> ReadField(const TSharedPtr<FJsonObject>& Result, const TCHAR* Key)
{
	if (const TSharedPtr<FJsonValue> Rooted = Result->TryGetField(Key))
	{
		return Rooted;
	}
	TSharedPtr<FJsonObject> Data;
	const TSharedPtr<FJsonObject>* DataField = nullptr;
	if (Result->TryGetObjectField(TEXT("data"), DataField) && DataField)
	{
		Data = *DataField;
		if (const TSharedPtr<FJsonValue> Nested = Data->TryGetField(Key))
		{
			return Nested;
		}
	}
	const TSharedPtr<FJsonObject>* Details = nullptr;
	if (Result->TryGetObjectField(TEXT("details"), Details) && Details)
	{
		if (const TSharedPtr<FJsonValue> Nested = (*Details)->TryGetField(Key))
		{
			return Nested;
		}
	}
	if (Data.IsValid())
	{
		const TSharedPtr<FJsonObject>* DataDetails = nullptr;
		if (Data->TryGetObjectField(TEXT("details"), DataDetails) && DataDetails)
		{
			if (const TSharedPtr<FJsonValue> NestedDetails = (*DataDetails)->TryGetField(Key))
			{
				return NestedDetails;
			}
		}
	}
	return nullptr;
}

FString OutcomeReadString(const TSharedPtr<FJsonObject>& Result, const TCHAR* Key)
{
	const TSharedPtr<FJsonValue> Value = ReadField(Result, Key);
	FString Out;
	if (Value.IsValid() && McpHandlerUtils::TryGetJsonValueString(Value, Out))
	{
		return Out;
	}
	return FString();
}

TSharedPtr<FJsonValue> MakeHandle(const TCHAR* Kind, const TCHAR* Field, const FString& Value)
{
	TSharedPtr<FJsonObject> Handle = MakeShared<FJsonObject>();
	Handle->SetStringField(TEXT("kind"), Kind);
	Handle->SetStringField(Field, Value);
	return MakeShared<FJsonValueObject>(Handle);
}

// blueprintPath is how every Blueprint and SCS handler names its asset; without
// it an edit_scs batch published no handle at all.
const TCHAR* const ASSET_FIELDS[] = {
	TEXT("assetPath"), TEXT("createdAssetPath"), TEXT("savedAssetPath"), TEXT("destinationPath"),
	TEXT("widgetPath"), TEXT("deletedPath"), TEXT("blueprintPath")};
// The caller-facing identities first: a receipt handle is what the client
// passes back to a follow-up capability, and the engine's internal object path
// (/Temp/...:PersistentLevel.Actor_UAID_...) is not a stable handle. The path
// is still published under `actorPath` in the payload.
const TCHAR* const ACTOR_FIELDS[] = {TEXT("actorName"), TEXT("actorLabel"), TEXT("actorPath")};
const TCHAR* const CHANGE_ARRAYS[] = {
	TEXT("changes"), TEXT("changedEntities"), TEXT("changedAssets"), TEXT("affectedActors"),
	TEXT("modifiedPaths"), TEXT("deleted")};
// `widgetPath` was listed for handles but not here, so create_game_screen and
// create_widget_template published a handle to a brand-new asset while leaving
// changes[] empty - a caller diffing changes[] missed every widget it authored.
const TCHAR* const CHANGE_ASSET_SINGLES[] = {
	TEXT("assetPath"), TEXT("createdAssetPath"), TEXT("savedAssetPath"),
	TEXT("destinationPath"), TEXT("deletedPath"), TEXT("widgetPath")};
const TCHAR* const CHANGE_ACTOR_SINGLES[] = {TEXT("actorName"), TEXT("actorPath")};

template <int32 Count>
void AddSingleChanges(const TSharedPtr<FJsonObject>& Result, const TCHAR* const (&Fields)[Count], TArray<FString>& Changes)
{
	for (const TCHAR* Field : Fields)
	{
		const FString Text = OutcomeReadString(Result, Field);
		if (!Text.IsEmpty() && !IsTransientPackagePath(Text))
		{
			Changes.AddUnique(Text);
		}
	}
}
}  // namespace

TArray<FString> McpExtractReceiptChanges(const TSharedPtr<FJsonObject>& RawResult)
{
	TArray<FString> Changes;
	if (!RawResult.IsValid())
	{
		return Changes;
	}
	for (const TCHAR* Field : CHANGE_ARRAYS)
	{
		const TSharedPtr<FJsonValue> Value = ReadField(RawResult, Field);
		const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
		if (Value.IsValid() && Value->TryGetArray(Array) && Array)
		{
			for (const TSharedPtr<FJsonValue>& Entry : *Array)
			{
				FString Text;
				if (McpHandlerUtils::TryGetJsonValueString(Entry, Text) && !Text.IsEmpty())
				{
					Changes.AddUnique(Text);
				}
			}
		}
	}
	// A result that carries a changedAssets array states every asset it changed, an
	// empty one included, so the asset paths it merely echoes are not read as changes
	// (a widget preview and a played sound name the asset they looked at or used). An
	// actor it changed is a separate fact and is still inferred. Mirrors extractChanges.
	const TSharedPtr<FJsonValue> StatedAssets = ReadField(RawResult, TEXT("changedAssets"));
	const TArray<TSharedPtr<FJsonValue>>* StatedArray = nullptr;
	if (!StatedAssets.IsValid() || !StatedAssets->TryGetArray(StatedArray))
	{
		AddSingleChanges(RawResult, CHANGE_ASSET_SINGLES, Changes);
	}
	AddSingleChanges(RawResult, CHANGE_ACTOR_SINGLES, Changes);
	return Changes;
}

TArray<TSharedPtr<FJsonValue>> McpExtractReceiptHandles(const TSharedPtr<FJsonObject>& RawResult)
{
	TArray<TSharedPtr<FJsonValue>> Handles;
	TSet<FString> Seen;
	if (!RawResult.IsValid())
	{
		return Handles;
	}
	const auto AddHandle = [&Handles, &Seen](const TCHAR* Kind, const TCHAR* Field, const FString& Value)
	{
		const FString Key = FString(Kind) + TEXT("|") + Value;
		if (!Seen.Contains(Key))
		{
			Seen.Add(Key);
			Handles.Add(MakeHandle(Kind, Field, Value));
		}
	};

	const TSharedPtr<FJsonValue> Explicit = ReadField(RawResult, TEXT("handles"));
	const TArray<TSharedPtr<FJsonValue>>* ExplicitArray = nullptr;
	if (Explicit.IsValid() && Explicit->TryGetArray(ExplicitArray) && ExplicitArray)
	{
		for (const TSharedPtr<FJsonValue>& Entry : *ExplicitArray)
		{
			const TSharedPtr<FJsonObject>* Object = nullptr;
			if (!Entry->TryGetObject(Object) || !Object)
			{
				continue;
			}
			FString Kind;
			(*Object)->TryGetStringField(TEXT("kind"), Kind);
			if (IsRefKind(Kind))
			{
				FString Ref;
				if ((*Object)->TryGetStringField(TEXT("ref"), Ref) && !Ref.IsEmpty() && Ref.Len() <= 512)
				{
					AddHandle(*Kind, TEXT("ref"), Ref);
				}
			}
			else if (IsPathKind(Kind))
			{
				FString Path;
				if ((*Object)->TryGetStringField(TEXT("path"), Path) && IsAllowedPath(Path))
				{
					AddHandle(*Kind, TEXT("path"), Path);
				}
			}
		}
	}

	for (const TCHAR* Field : ASSET_FIELDS)
	{
		const FString Path = OutcomeReadString(RawResult, Field);
		if (IsAllowedPath(Path) && !IsTransientPackagePath(Path))
		{
			AddHandle(TEXT("asset"), TEXT("path"), Path);
			break;
		}
	}
	for (const TCHAR* Field : ACTOR_FIELDS)
	{
		const FString Ref = OutcomeReadString(RawResult, Field);
		if (!Ref.IsEmpty() && Ref.Len() <= 512)
		{
			AddHandle(TEXT("actor"), TEXT("ref"), Ref);
			break;
		}
	}
	return Handles;
}

TSharedPtr<FJsonObject> McpExtractReceiptTask(const TSharedPtr<FJsonObject>& RawResult)
{
	if (!RawResult.IsValid())
	{
		return nullptr;
	}
	TSharedPtr<FJsonValue> Raw = ReadField(RawResult, TEXT("task"));
	if (!Raw.IsValid())
	{
		Raw = ReadField(RawResult, TEXT("taskStatus"));
	}
	const TSharedPtr<FJsonObject>* Object = nullptr;
	if (!Raw.IsValid() || !Raw->TryGetObject(Object) || !Object)
	{
		return nullptr;
	}
	FString TaskId;
	FString State;
	(*Object)->TryGetStringField(TEXT("taskId"), TaskId);
	(*Object)->TryGetStringField(TEXT("state"), State);
	static const TArray<FString> States = {
		TEXT("queued"), TEXT("running"), TEXT("completed"), TEXT("failed"), TEXT("cancelled")};
	if (TaskId.IsEmpty() || !States.Contains(State))
	{
		return nullptr;
	}
	TSharedPtr<FJsonObject> Task = MakeShared<FJsonObject>();
	Task->SetStringField(TEXT("taskId"), TaskId);
	Task->SetStringField(TEXT("state"), State);
	double Progress = 0.0;
	if ((*Object)->TryGetNumberField(TEXT("progress"), Progress) && Progress >= 0.0 && Progress <= 1.0)
	{
		Task->SetNumberField(TEXT("progress"), Progress);
	}
	return Task;
}
