// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpFabLogCapture.h"

#include "HAL/CriticalSection.h"
#include "Logging/LogVerbosity.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/ScopeLock.h"

namespace McpFabLogCapture
{
namespace
{
constexpr int32 MaxLines = 16;
constexpr int32 MaxLineChars = 240;

/** A log line goes into a reply, so anything URL-shaped or credential-shaped is masked first. */
FString Scrub(const TCHAR* Message)
{
	FString Text(Message);
	if (Text.IsEmpty() || Text.Contains(TEXT("token"), ESearchCase::IgnoreCase))
	{
		return Text.IsEmpty() ? Text : FString(TEXT("[redacted]"));
	}
	int32 From = 0;
	for (;;)
	{
		const int32 Scheme = Text.Find(TEXT("://"), ESearchCase::IgnoreCase, ESearchDir::FromStart, From);
		if (Scheme == INDEX_NONE)
		{
			break;
		}
		int32 Start = Scheme;
		while (Start > 0 && FChar::IsAlpha(Text[Start - 1]))
		{
			--Start;
		}
		int32 End = Scheme + 3;
		while (End < Text.Len() && !FChar::IsWhitespace(Text[End]))
		{
			++End;
		}
		Text = Text.Left(Start) + TEXT("[url]") + Text.Mid(End);
		From = Start + 5;
	}
	return Text.Left(MaxLineChars);
}

class FFabLogDevice final : public FOutputDevice
{
public:
	virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category) override
	{
		static const FName FabCategory(TEXT("LogFab"));
		if (Message == nullptr || Category != FabCategory ||
			(Verbosity & ELogVerbosity::VerbosityMask) > ELogVerbosity::Error)
		{
			return;
		}
		FScopeLock Lock(&Mutex);
		if (Lines.Num() < MaxLines)
		{
			Lines.Add(Scrub(Message));
		}
	}

	void Take(TArray<FString>& OutLines)
	{
		FScopeLock Lock(&Mutex);
		OutLines.Append(Lines);
		Lines.Reset();
	}

private:
	FCriticalSection Mutex;
	TArray<FString> Lines;
};

TUniquePtr<FFabLogDevice> Device;
} // namespace

void Start()
{
	Stop();
	if (GLog != nullptr)
	{
		Device = MakeUnique<FFabLogDevice>();
		GLog->AddOutputDevice(Device.Get());
	}
}

void Stop()
{
	if (Device.IsValid())
	{
		if (GLog != nullptr)
		{
			GLog->RemoveOutputDevice(Device.Get());
		}
		Device.Reset();
	}
}

void Take(TArray<FString>& OutLines)
{
	if (Device.IsValid())
	{
		Device->Take(OutLines);
	}
}

bool IsWorkflowFailure(const FString& Line)
{
	// The lines Fab's import workflows log right before they cancel. Errors from its sign-in or
	// library sync are ignored: they say nothing about the import that is running.
	static const TCHAR* const Signatures[] = {
		TEXT("failed to download"), TEXT("failed to unzip"), TEXT("import files not found"),
		TEXT("failed to import"), TEXT("asset import failed"), TEXT("failed to create the dir"),
		TEXT("failed to open file"), TEXT("failed to rename"), TEXT("invalid manifest"),
		TEXT("failed to load buildpatch"), TEXT("asset type not handled")};
	for (const TCHAR* Signature : Signatures)
	{
		if (Line.Contains(Signature, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	return false;
}
} // namespace McpFabLogCapture
