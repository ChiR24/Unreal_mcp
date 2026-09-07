#include "Foundation/McpLiveStateRevisions.h"

#include "Dom/JsonObject.h"

int64 FMcpLiveStateRevisionSnapshot::Max() const
{
	return FMath::Max(FMath::Max(Selection, Level), FMath::Max(AssetRegistry, Package));
}

TSharedRef<FJsonObject> FMcpLiveStateRevisionSnapshot::ToJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("selection"), static_cast<double>(Selection));
	Json->SetNumberField(TEXT("level"), static_cast<double>(Level));
	Json->SetNumberField(TEXT("assetRegistry"), static_cast<double>(AssetRegistry));
	Json->SetNumberField(TEXT("package"), static_cast<double>(Package));
	// A restart resets every counter; the instance id lets a client tell a restart from a rollback (dogfood #44).
	static const FString ServerInstanceId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	Json->SetStringField(TEXT("serverInstanceId"), ServerInstanceId);
	return Json;
}

FMcpLiveStateRevisions& FMcpLiveStateRevisions::Get()
{
	static FMcpLiveStateRevisions Registry;
	return Registry;
}

void FMcpLiveStateRevisions::Advance(EMcpStateKind Kind)
{
	FScopeLock Lock(&Mutex);
	int64& Value = Revisions.FindOrAdd(Kind, McpInitialStateRevision);
	++Value;
}

int64 FMcpLiveStateRevisions::Current(EMcpStateKind Kind) const
{
	FScopeLock Lock(&Mutex);
	const int64* Value = Revisions.Find(Kind);
	return Value ? *Value : McpInitialStateRevision;
}

FMcpLiveStateRevisionSnapshot FMcpLiveStateRevisions::Snapshot() const
{
	FScopeLock Lock(&Mutex);
	const auto Read = [this](EMcpStateKind Kind)
	{
		const int64* Value = Revisions.Find(Kind);
		return Value ? *Value : McpInitialStateRevision;
	};
	return {
		Read(EMcpStateKind::Selection),
		Read(EMcpStateKind::Level),
		Read(EMcpStateKind::AssetRegistry),
		Read(EMcpStateKind::Package),
	};
}

bool FMcpLiveStateRevisions::CheckPreconditions(
	const TMap<EMcpStateKind, int64>& Expected,
	EMcpStateKind& OutKind,
	int64& OutExpected,
	int64& OutCurrent) const
{
	FScopeLock Lock(&Mutex);
	// Deterministic order so the "first" stale state a client sees is stable
	// across runs rather than dependent on TMap iteration order.
	static const EMcpStateKind Order[] = {
		EMcpStateKind::Selection, EMcpStateKind::Level,
		EMcpStateKind::AssetRegistry, EMcpStateKind::Package };
	for (EMcpStateKind Kind : Order)
	{
		const int64* ExpectedValue = Expected.Find(Kind);
		if (!ExpectedValue)
		{
			continue;
		}
		const int64* LiveValue = Revisions.Find(Kind);
		const int64 Live = LiveValue ? *LiveValue : McpInitialStateRevision;
		if (*ExpectedValue != Live)
		{
			OutKind = Kind;
			OutExpected = *ExpectedValue;
			OutCurrent = Live;
			return false;
		}
	}
	return true;
}

void FMcpLiveStateRevisions::Reset()
{
	FScopeLock Lock(&Mutex);
	Revisions.Empty();
}

const TCHAR* FMcpLiveStateRevisions::KeyFor(EMcpStateKind Kind)
{
	switch (Kind)
	{
	case EMcpStateKind::Selection: return TEXT("selection");
	case EMcpStateKind::Level: return TEXT("level");
	case EMcpStateKind::AssetRegistry: return TEXT("assetRegistry");
	case EMcpStateKind::Package: return TEXT("package");
	default: return TEXT("");
	}
}

const TArray<FString>& FMcpLiveStateRevisions::AllKeys()
{
	static const TArray<FString> Keys = {
		KeyFor(EMcpStateKind::Selection),
		KeyFor(EMcpStateKind::Level),
		KeyFor(EMcpStateKind::AssetRegistry),
		KeyFor(EMcpStateKind::Package),
	};
	return Keys;
}

const TCHAR* FMcpLiveStateRevisions::StaleStateErrorCode()
{
	return TEXT("STALE_STATE");
}

bool FMcpLiveStateRevisions::KindFor(const FString& Key, EMcpStateKind& OutKind)
{
	if (Key == TEXT("selection")) { OutKind = EMcpStateKind::Selection; return true; }
	if (Key == TEXT("level")) { OutKind = EMcpStateKind::Level; return true; }
	if (Key == TEXT("assetRegistry")) { OutKind = EMcpStateKind::AssetRegistry; return true; }
	if (Key == TEXT("package")) { OutKind = EMcpStateKind::Package; return true; }
	return false;
}

FMcpExpectedRevisionsParseResult FMcpLiveStateRevisions::ParseExpectedRevisions(
	const TSharedPtr<FJsonValue>& Field)
{
	FMcpExpectedRevisionsParseResult Result;
	if (!Field.IsValid())
	{
		return Result;
	}

	const TSharedPtr<FJsonObject>* Pins = nullptr;
	if (!Field->TryGetObject(Pins) || Pins == nullptr || !Pins->IsValid())
	{
		Result.bSuccess = false;
		Result.ErrorCode = TEXT("INVALID_OPTIONS");
		Result.Pointer = TEXT("/options/expectedRevisions");
		Result.Message = TEXT("options.expectedRevisions must be an object of state revisions.");
		return Result;
	}

	for (const TPair<FString, TSharedPtr<FJsonValue>> Entry : (*Pins)->Values)
	{
		EMcpStateKind Kind = EMcpStateKind::Selection;
		if (!KindFor(Entry.Key, Kind))
		{
			Result.bSuccess = false;
			Result.ErrorCode = TEXT("UNSUPPORTED_OPTION");
			Result.Field = FString::Printf(TEXT("expectedRevisions.%s"), *Entry.Key);
			Result.Message = FString::Printf(
				TEXT("Unsupported expected revision '%s'. Supported: [%s]"),
				*Entry.Key, *FString::Join(AllKeys(), TEXT(", ")));
			Result.Revisions.Empty();
			return Result;
		}

		const bool bNumeric = Entry.Value.IsValid() && Entry.Value->Type == EJson::Number;
		const double Value = bNumeric ? Entry.Value->AsNumber() : 0.0;
		const bool bWholeNumber = bNumeric && FMath::IsFinite(Value) &&
			Value == FMath::TruncToDouble(Value);
		if (!bWholeNumber || Value < static_cast<double>(McpInitialStateRevision))
		{
			Result.bSuccess = false;
			Result.ErrorCode = TEXT("OUT_OF_RANGE");
			Result.Field = FString::Printf(TEXT("expectedRevisions.%s"), *Entry.Key);
			Result.Message = FString::Printf(
				TEXT("options.expectedRevisions.%s must be an integer >= %lld"),
				*Entry.Key, McpInitialStateRevision);
			Result.Revisions.Empty();
			return Result;
		}
		Result.Revisions.Add(Kind, static_cast<int64>(Value));
	}
	return Result;
}
