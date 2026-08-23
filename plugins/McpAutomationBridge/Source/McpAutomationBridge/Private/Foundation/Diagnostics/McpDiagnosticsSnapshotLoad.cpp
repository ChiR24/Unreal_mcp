#include "Foundation/Diagnostics/McpDiagnosticsSnapshot.h"

#include "Containers/StringConv.h"
#include "Foundation/Diagnostics/McpDiagnosticsSnapshotSchema.h"
#include "HAL/PlatformFileManager.h"
#include "McpAutomationBridgeLog.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
bool LoadHasRecordedEvents(const FMcpDiagnosticsSnapshotState& State)
{
	return State.Requests > 0 || State.Refusals > 0 || State.bHasRequest
		|| State.bHasHandshake || State.bHasDisconnect || State.bHasSession;
}

bool GetNumber(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key, double& Out)
{
	return Obj->TryGetNumberField(Key, Out) && FMath::IsFinite(Out);
}

bool GetIntField(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key, int32& Out)
{
	double Value = 0.0;
	if (!GetNumber(Obj, Key, Value))
	{
		return false;
	}
	Out = static_cast<int32>(Value);
	return true;
}

bool GetTimeSeconds(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key, double& Out)
{
	Out = 0.0;
	FString Text;
	if (!Obj->TryGetStringField(Key, Text) || Text.IsEmpty())
	{
		return true;
	}
	FDateTime DateTime;
	if (!FDateTime::ParseIso8601(*Text, DateTime))
	{
		return false;
	}
	Out = DateTime.ToUnixTimestamp();
	return true;
}

bool GetNullableString(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key, FString& Out)
{
	const TSharedPtr<FJsonValue> Value = Obj->TryGetField(Key);
	if (!Value.IsValid())
	{
		return false;
	}
	Out.Reset();
	if (!Value->IsNull())
	{
		if (Value->Type != EJson::String)
		{
			return false;
		}
		Out = Value->AsString();
	}
	return true;
}

bool ReadOptionalObject(const TSharedPtr<FJsonObject>& Root, const TCHAR* Key, TSharedPtr<FJsonObject>& Out)
{
	const TSharedPtr<FJsonValue> Field = Root->TryGetField(Key);
	if (!Field.IsValid() || Field->IsNull())
	{
		return true;
	}
	Out = Field->AsObject();
	return Out.IsValid();
}

// Strict allowlist loader: only the schema fields are read back; unknown on-disk
// fields are structurally ignored, and a wrong type fails the whole snapshot.
bool ReadStateFromJson(const TSharedPtr<FJsonObject>& Root, FMcpDiagnosticsSnapshotState& Out)
{
	double Version = 0.0;
	if (!Root.IsValid() || !GetNumber(Root, TEXT("schemaVersion"), Version)
		|| static_cast<int32>(Version) != McpDiagnosticsSchema::SchemaVersionValue)
	{
		return false;
	}
	const TSharedPtr<FJsonObject> Instance = Root->GetObjectField(TEXT("instance"));
	const TSharedPtr<FJsonObject> Counters = Root->GetObjectField(TEXT("counters"));
	const TSharedPtr<FJsonObject> LastRequest = Root->GetObjectField(TEXT("lastRequest"));
	if (!Instance.IsValid() || !Counters.IsValid() || !LastRequest.IsValid())
	{
		return false;
	}
	if (!GetNullableString(Instance, TEXT("instanceId"), Out.InstanceId)
		|| !GetIntField(Instance, TEXT("pid"), Out.Pid)
		|| !GetNullableString(Instance, TEXT("startTimeUtc"), Out.StartTimeUtc)
		|| !GetIntField(Counters, TEXT("requests"), Out.Requests)
		|| !GetIntField(Counters, TEXT("failures"), Out.Failures)
		|| !GetIntField(Counters, TEXT("refusals"), Out.Refusals)
		|| !GetIntField(Counters, TEXT("queueWaitMs"), Out.QueueWaitMs)
		|| !GetNullableString(LastRequest, TEXT("requestId"), Out.RequestId)
		|| !GetNullableString(LastRequest, TEXT("correlationId"), Out.CorrelationId)
		|| !GetNullableString(LastRequest, TEXT("canonicalAction"), Out.CanonicalAction)
		|| !GetNullableString(LastRequest, TEXT("origin"), Out.Origin)
		|| !GetIntField(LastRequest, TEXT("queueDepth"), Out.QueueDepth)
		|| !GetTimeSeconds(LastRequest, TEXT("enqueueAt"), Out.EnqueueAt)
		|| !GetTimeSeconds(LastRequest, TEXT("dispatchAt"), Out.DispatchAt)
		|| !GetTimeSeconds(LastRequest, TEXT("terminalAt"), Out.TerminalAt)
		|| !GetNullableString(LastRequest, TEXT("terminalClass"), Out.TerminalClass))
	{
		return false;
	}
	// Derived from content, not from the section existing. BuildSnapshotJson
	// always writes a "lastRequest" object (all-null for an event-less session),
	// so marking bHasRequest unconditionally here made every parsed snapshot look
	// like it had recorded a request. HasRecordedEvents then returned true for a
	// fresh session, and RotateOnStartup promoted that empty session over the
	// previous one -- the precise outcome the "promote only a session that
	// recorded at least one event" guard exists to prevent, so a second startup
	// or a commandlet run silently destroyed the crash evidence it was meant to
	// keep. RecordAdmission always stamps a requestId, so its presence is the
	// honest signal.
	Out.bHasRequest = !Out.RequestId.IsEmpty();
	Out.InstanceId = Out.InstanceId.Left(McpDiagnosticsSchema::MaxIdLength);
	Out.RequestId = Out.RequestId.Left(McpDiagnosticsSchema::MaxIdLength);
	Out.CorrelationId = Out.CorrelationId.Left(McpDiagnosticsSchema::MaxIdLength);

	TSharedPtr<FJsonObject> Handshake;
	if (!ReadOptionalObject(Root, TEXT("lastHandshake"), Handshake))
	{
		return false;
	}
	if (Handshake.IsValid())
	{
		if (!GetTimeSeconds(Handshake, TEXT("at"), Out.HandshakeAt)
			|| !Handshake->TryGetBoolField(TEXT("ok"), Out.HandshakeOk))
		{
			return false;
		}
		Out.bHasHandshake = true;
	}

	TSharedPtr<FJsonObject> Disconnect;
	if (!ReadOptionalObject(Root, TEXT("lastDisconnect"), Disconnect))
	{
		return false;
	}
	if (Disconnect.IsValid())
	{
		if (!GetTimeSeconds(Disconnect, TEXT("at"), Out.DisconnectAt)
			|| !GetNullableString(Disconnect, TEXT("reason"), Out.DisconnectReason))
		{
			return false;
		}
		Out.bHasDisconnect = true;
	}

	TSharedPtr<FJsonObject> Session;
	if (!ReadOptionalObject(Root, TEXT("session"), Session))
	{
		return false;
	}
	if (Session.IsValid())
	{
		if (!GetIntField(Session, TEXT("created"), Out.SessionsCreated)
			|| !GetIntField(Session, TEXT("closed"), Out.SessionsClosed)
			|| !GetIntField(Session, TEXT("active"), Out.SessionsActive)
			|| !GetNullableString(Session, TEXT("lastIdentitySha256"), Out.LastIdentitySha256)
			|| !GetTimeSeconds(Session, TEXT("at"), Out.SessionAt))
		{
			return false;
		}
		Out.bHasSession = true;
	}

	return true;
}
} // namespace

bool FMcpDiagnosticsSnapshot::LoadAndValidateFile(const FString& FileName, FString& OutContent, FMcpDiagnosticsSnapshotState& OutState) const
{
	OutContent.Reset();
	OutState = FMcpDiagnosticsSnapshotState();
	const FString Root = DiagnosticsRoot();
	if (Root.IsEmpty())
	{
		return false;
	}
	const FString Path = FPaths::Combine(Root, FileName);
	if (!FPaths::FileExists(Path))
	{
		return false;
	}
	if (!FFileHelper::LoadFileToString(OutContent, *Path))
	{
		WarnOnce(Path, TEXT("unreadable"), Path.Contains(TEXT("current")) ? bWarnedAboutCurrent : bWarnedAboutPrevious);
		return false;
	}
	if (FTCHARToUTF8(OutContent).Length() > McpDiagnosticsSchema::MaxSnapshotBytes)
	{
		WarnOnce(Path, TEXT("oversized"), Path.Contains(TEXT("current")) ? bWarnedAboutCurrent : bWarnedAboutPrevious);
		return false;
	}
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(OutContent);
	TSharedPtr<FJsonObject> RootJson;
	if (!FJsonSerializer::Deserialize(Reader, RootJson) || !ReadStateFromJson(RootJson, OutState))
	{
		WarnOnce(Path, TEXT("corrupt"), Path.Contains(TEXT("current")) ? bWarnedAboutCurrent : bWarnedAboutPrevious);
		return false;
	}
	return true;
}

void FMcpDiagnosticsSnapshot::WarnOnce(const FString& FileName, const TCHAR* Reason, bool& bWarned) const
{
	if (bWarned) { return; }
	bWarned = true;
	UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
		TEXT("McpDiagnosticsSnapshot: ignored %s diagnostics snapshot (%s)"), Reason, *FileName);
}

void FMcpDiagnosticsSnapshot::InitializeFreshCurrent()
{
	State = FMcpDiagnosticsSnapshotState();
	EnsureInstanceAttributionLocked();
	WriteFileAtomic(CurrentFileName(), CurrentTempName(), McpDiagnosticsSchema::SerializeState(State, false));
}
