// McpNativeResultPages.cpp — a result too large for one reply, paged instead of refused. When one top-level array is
// what makes it too big, the items that fit go out with a page block, and the rest waits here briefly for the same
// principal, capability and params to read with options.pageCursor, without the handler running again. Each page is
// the capability's own output with a slice of that array, so it still meets the declared output schema. Mirrors
// src/server/gateway/gateway-result-pages.ts.

#include "MCP/Execute/McpNativeGatewayReceipt.h"
#include "MCP/Execute/McpNativeReceiptRedaction.h"

#include "HAL/CriticalSection.h"
#include "HAL/PlatformTime.h"
#include "Misc/Guid.h"
#include "Misc/ScopeLock.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
constexpr double McpPageTtlSeconds = 300.0;
constexpr int32 McpMaxKeptPages = 8;
// The page block, its continuation call and the field name, kept out of the items' share of the budget.
constexpr int64 McpPageOverheadChars = 2000;

struct FMcpKeptResult
{
	FMcpPageOwner Owner;
	TSharedPtr<FJsonObject> Rest;
	FString Field;
	TArray<TSharedPtr<FJsonValue>> Items;
	TArray<int64> Sizes;
	int32 Offset = 0;
	double Expires = 0.0;
};

FCriticalSection& McpPagesMutex()
{
	static FCriticalSection Mutex;
	return Mutex;
}

TMap<FString, FMcpKeptResult>& McpKeptPages()
{
	static TMap<FString, FMcpKeptResult> Kept;
	return Kept;
}

TArray<FString>& McpKeptOrder()
{
	static TArray<FString> Order;
	return Order;
}

int64 McpArrayChars(const TArray<TSharedPtr<FJsonValue>>& Values)
{
	FString Out;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
	FJsonSerializer::Serialize(Values, Writer);
	return Out.Len();
}

int64 McpObjectChars(const TSharedPtr<FJsonObject>& Object)
{
	int64 Chars = 0;
	McpSerializedResultExceeds(Object, MAX_int32, &Chars);
	return Chars;
}

// Caller holds McpPagesMutex. Takes at least one item (every item was checked to fit alone), so paging advances.
void McpSlicePage(FMcpKeptResult& Entry, const FString& Cursor, int64 Budget, double Now, FMcpResultPage& Out)
{
	const int64 Room = Budget - McpObjectChars(Entry.Rest) - McpPageOverheadChars;
	int64 Used = 0;
	int32 End = Entry.Offset;
	while (End < Entry.Items.Num() && (End == Entry.Offset || Used + Entry.Sizes[End] + 1 <= Room))
	{
		Used += Entry.Sizes[End] + 1;
		++End;
	}
	Out.Result = MakeShared<FJsonObject>();
	Out.Result->Values = Entry.Rest->Values;
	Out.Result->SetArrayField(Entry.Field, TArray<TSharedPtr<FJsonValue>>(Entry.Items.GetData() + Entry.Offset, End - Entry.Offset));
	Out.Page = MakeShared<FJsonObject>();
	Out.Page->SetStringField(TEXT("field"), Entry.Field);
	Out.Page->SetNumberField(TEXT("offset"), Entry.Offset);
	Out.Page->SetNumberField(TEXT("returned"), End - Entry.Offset);
	Out.Page->SetNumberField(TEXT("total"), Entry.Items.Num());
	if (End < Entry.Items.Num())
	{
		// The same call with the cursor reads the next page; params are checked against the first call's.
		TSharedPtr<FJsonObject> Options = MakeShared<FJsonObject>();
		Options->SetStringField(TEXT("pageCursor"), Cursor);
		TSharedPtr<FJsonObject> NextCall = MakeShared<FJsonObject>();
		NextCall->SetStringField(TEXT("operation"), TEXT("execute"));
		NextCall->SetStringField(TEXT("capability"), Entry.Owner.CapabilityId);
		NextCall->SetObjectField(TEXT("params"), Entry.Owner.Params.IsValid() ? Entry.Owner.Params : MakeShared<FJsonObject>());
		NextCall->SetObjectField(TEXT("options"), Options);
		Out.Page->SetStringField(TEXT("cursor"), Cursor);
		Out.Page->SetObjectField(TEXT("nextCall"), NextCall);
	}
	Entry.Offset = End;
	Entry.Expires = Now + McpPageTtlSeconds;
	if (End >= Entry.Items.Num())
	{
		McpKeptPages().Remove(Cursor);
		McpKeptOrder().Remove(Cursor);
	}
}
} // namespace

bool McpFirstResultPage(const TSharedPtr<FJsonObject>& Result, int64 Budget, const FMcpPageOwner& Owner, FMcpResultPage& Out)
{
	if (!Result.IsValid())
	{
		return false;
	}
	FString Field;
	int64 Largest = -1;
	for (const auto& Pair : Result->Values)
	{
		const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
		if (!Pair.Value.IsValid() || !Pair.Value->TryGetArray(Array) || !Array || Array->Num() < 2)
		{
			continue;
		}
		const int64 Chars = McpArrayChars(*Array);
		if (Chars > Largest)
		{
			Largest = Chars;
			Field = FString(Pair.Key.Len(), *Pair.Key);
		}
	}
	if (Field.IsEmpty())
	{
		return false;
	}
	FMcpKeptResult Entry;
	Entry.Owner = Owner;
	Entry.Field = Field;
	Entry.Items = Result->GetArrayField(Field);
	Entry.Rest = MakeShared<FJsonObject>();
	Entry.Rest->Values = Result->Values;
	Entry.Rest->RemoveField(Field);
	// Nothing pages when the rest alone, or one item alone, is too large for a reply.
	const int64 Room = Budget - McpObjectChars(Entry.Rest) - McpPageOverheadChars;
	for (const TSharedPtr<FJsonValue>& Item : Entry.Items)
	{
		const int64 Chars = McpArrayChars(TArray<TSharedPtr<FJsonValue>>({Item})) - 2;
		if (Chars + 1 > Room)
		{
			return false;
		}
		Entry.Sizes.Add(Chars);
	}

	const double Now = FPlatformTime::Seconds();
	FScopeLock Lock(&McpPagesMutex());
	for (int32 Index = McpKeptOrder().Num() - 1; Index >= 0; --Index)
	{
		const FMcpKeptResult* Old = McpKeptPages().Find(McpKeptOrder()[Index]);
		if (!Old || Old->Expires <= Now)
		{
			McpKeptPages().Remove(McpKeptOrder()[Index]);
			McpKeptOrder().RemoveAt(Index);
		}
	}
	while (McpKeptOrder().Num() >= McpMaxKeptPages)
	{
		McpKeptPages().Remove(McpKeptOrder()[0]);
		McpKeptOrder().RemoveAt(0);
	}
	const FString Cursor = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
	FMcpKeptResult& Stored = McpKeptPages().Add(Cursor, MoveTemp(Entry));
	McpKeptOrder().Add(Cursor);
	McpSlicePage(Stored, Cursor, Budget, Now, Out);
	return true;
}

FString McpNextResultPage(const FString& Cursor, const FMcpPageOwner& Owner, int64 Budget, FMcpResultPage& Out)
{
	const double Now = FPlatformTime::Seconds();
	FScopeLock Lock(&McpPagesMutex());
	FMcpKeptResult* Entry = McpKeptPages().Find(Cursor);
	if (!Entry || Entry->Expires <= Now)
	{
		McpKeptPages().Remove(Cursor);
		McpKeptOrder().Remove(Cursor);
		return TEXT("PAGE_CURSOR_EXPIRED");
	}
	if (!Entry->Owner.Principal.Equals(Owner.Principal, ESearchCase::CaseSensitive) ||
		!Entry->Owner.CapabilityId.Equals(Owner.CapabilityId, ESearchCase::CaseSensitive) ||
		!Entry->Owner.Fingerprint.Equals(Owner.Fingerprint, ESearchCase::CaseSensitive))
	{
		return TEXT("PAGE_CURSOR_MISMATCH");
	}
	McpSlicePage(*Entry, Cursor, Budget, Now, Out);
	return FString();
}
