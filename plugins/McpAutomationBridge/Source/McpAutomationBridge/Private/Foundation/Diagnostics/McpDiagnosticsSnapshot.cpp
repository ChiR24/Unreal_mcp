#include "Foundation/Diagnostics/McpDiagnosticsSnapshot.h"

#include "Containers/StringConv.h"
#include "Foundation/Diagnostics/McpDiagnosticsSnapshotSchema.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "McpAutomationBridgeLog.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "openssl/sha.h"

namespace
{
const TCHAR* CurrentFileName() { return TEXT("current-session.json"); }
const TCHAR* CurrentTempName() { return TEXT("current-session.json.tmp"); }
const TCHAR* PreviousFileName() { return TEXT("previous-session.json"); }
const TCHAR* PreviousTempName() { return TEXT("previous-session.json.tmp"); }

bool HasRecordedEvents(const FMcpDiagnosticsSnapshotState& State)
{
	return State.Requests > 0 || State.Refusals > 0 || State.bHasRequest
		|| State.bHasHandshake || State.bHasDisconnect || State.bHasSession;
}

// OpenSSL-backed truncated SHA-256. The engine's FPlatformMisc::GetSHA256Signature
// is checkf(false) on some platforms (see McpIdempotencyLedger.cpp), so the digest
// uses the same OpenSSL SHA256 the plugin already links. The FSHA256 name documents
// the digest intent; only a truncated hex prefix is ever recorded as an identity.
FString FSHA256TruncatedDigest(const FString& Raw, int32 MaxHexChars)
{
	const FTCHARToUTF8 Utf8(*Raw);
	unsigned char Hash[SHA256_DIGEST_LENGTH];
	SHA256(reinterpret_cast<const unsigned char*>(Utf8.Get()), static_cast<size_t>(Utf8.Length()), Hash);
	FString Digest;
	Digest.Reserve(SHA256_DIGEST_LENGTH * 2);
	for (int32 Index = 0; Index < SHA256_DIGEST_LENGTH; ++Index) { Digest += FString::Printf(TEXT("%02x"), Hash[Index]); }
	return Digest.Left(MaxHexChars);
}
} // namespace

FMcpDiagnosticsSnapshot& FMcpDiagnosticsSnapshot::Get()
{
	static FMcpDiagnosticsSnapshot Instance;
	return Instance;
}

void FMcpDiagnosticsSnapshot::SetClock(TFunction<double()> InClock)
{
	FScopeLock Lock(&Mutex);
	Clock = MoveTemp(InClock);
}

void FMcpDiagnosticsSnapshot::SetRootOverride(const FString& InRoot)
{
	FScopeLock Lock(&Mutex);
	RootOverride = InRoot;
}

void FMcpDiagnosticsSnapshot::Reset()
{
	FScopeLock Lock(&Mutex);
	Clock.Reset();
	RootOverride.Reset();
	CachedRoot.Reset();
	bRootCached = false;
	State = FMcpDiagnosticsSnapshotState();
	PreviousState = FMcpDiagnosticsSnapshotState();
	bHasPrevious = false;
	bWarnedAboutCurrent = false;
	bWarnedAboutPrevious = false;
	bDirty = false;
	LastPersistTime = 0.0;
	InstanceId.Reset();
	Pid = 0;
	StartTimeUtc.Reset();
}

void FMcpDiagnosticsSnapshot::InitializeFromGameThread()
{
	FScopeLock Lock(&Mutex);
	if (bRootCached || !RootOverride.IsEmpty())
	{
		return;
	}
	CachedRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MCP"), TEXT("diagnostics"));
	bRootCached = true;
}

void FMcpDiagnosticsSnapshot::EnsureInstanceAttributionLocked()
{
	if (!InstanceId.IsEmpty())
	{
		return;
	}
	Pid = static_cast<int32>(FPlatformProcess::GetCurrentProcessId());
	InstanceId = FSHA256TruncatedDigest(FString::Printf(TEXT("mcp-instance|%d|%.3f"), Pid, Now()), 32);
	StartTimeUtc = FDateTime::UtcNow().ToIso8601();
}

void FMcpDiagnosticsSnapshot::RecordAdmission(const FString& RequestId, const FString& CorrelationId, const FString& CanonicalAction, const FString& Origin, int32 QueueDepth)
{
	FScopeLock Lock(&Mutex);
	EnsureInstanceAttributionLocked();
	State.Requests += 1;
	State.bHasRequest = true;
	State.RequestId = RequestId.Left(McpDiagnosticsSchema::MaxIdLength);
	State.CorrelationId = CorrelationId.Left(McpDiagnosticsSchema::MaxIdLength);
	State.CanonicalAction = McpDiagnosticsSchema::CoerceCanonicalAction(CanonicalAction);
	State.Origin = McpDiagnosticsSchema::CoerceOrigin(Origin);
	State.QueueDepth = QueueDepth;
	State.EnqueueAt = Now();
	State.DispatchAt = 0.0;
	State.TerminalAt = 0.0;
	State.TerminalClass.Reset();
	bDirty = true;
}

void FMcpDiagnosticsSnapshot::RecordPreDispatch(const FString& RequestId, int32 QueueDepth)
{
	FScopeLock Lock(&Mutex);
	if (State.bHasRequest && State.RequestId == RequestId.Left(McpDiagnosticsSchema::MaxIdLength))
	{
		State.QueueDepth = QueueDepth;
		State.DispatchAt = Now();
		bDirty = true;
	}
}

void FMcpDiagnosticsSnapshot::RecordRefusal(const FString& RequestId, const FString& RefusalCode, int32 QueueDepth)
{
	FScopeLock Lock(&Mutex);
	EnsureInstanceAttributionLocked();
	State.Refusals += 1;
	State.TerminalClass = McpDiagnosticsSchema::CoerceTerminalClass(RefusalCode);
	State.TerminalAt = Now();
	if (!RequestId.IsEmpty())
	{
		State.bHasRequest = true;
		State.RequestId = RequestId.Left(McpDiagnosticsSchema::MaxIdLength);
		State.QueueDepth = QueueDepth;
	}
	bDirty = true;
}

void FMcpDiagnosticsSnapshot::RecordTerminal(const FString& RequestId, const FString& TerminalClass)
{
	FScopeLock Lock(&Mutex);
	if (!State.bHasRequest || (!RequestId.IsEmpty() && State.RequestId != RequestId.Left(McpDiagnosticsSchema::MaxIdLength)))
	{
		return;
	}
	const FString Coerced = McpDiagnosticsSchema::CoerceTerminalClass(TerminalClass);
	State.TerminalClass = Coerced;
	State.TerminalAt = Now();
	if (Coerced != TEXT("success") && Coerced != McpDiagnosticsSchema::UnknownValue()) { ++State.Failures; }
	State.QueueWaitMs = FMath::Max(0, FMath::TruncToInt((State.DispatchAt - State.EnqueueAt) * 1000.0));
	bDirty = true;
}

void FMcpDiagnosticsSnapshot::RecordHandshake(bool bSuccess)
{
	FScopeLock Lock(&Mutex);
	EnsureInstanceAttributionLocked();
	State.bHasHandshake = true;
	State.HandshakeOk = bSuccess;
	State.HandshakeAt = Now();
	bDirty = true;
}

void FMcpDiagnosticsSnapshot::RecordDisconnect(const FString& Reason)
{
	FScopeLock Lock(&Mutex);
	EnsureInstanceAttributionLocked();
	State.bHasDisconnect = true;
	State.DisconnectReason = McpDiagnosticsSchema::CoerceDisconnectReason(Reason);
	State.DisconnectAt = Now();
	bDirty = true;
}

void FMcpDiagnosticsSnapshot::RecordSessionCreated(const FString& RawNativeSession)
{
	FScopeLock Lock(&Mutex);
	EnsureInstanceAttributionLocked();
	State.bHasSession = true;
	State.SessionsCreated += 1;
	State.SessionsActive += 1;
	State.LastIdentitySha256 = RawNativeSession.IsEmpty() ? FString() : FSHA256TruncatedDigest(RawNativeSession, 32);
	State.SessionAt = Now();
	bDirty = true;
}

void FMcpDiagnosticsSnapshot::RecordSessionClosed()
{
	FScopeLock Lock(&Mutex);
	State.bHasSession = true;
	State.SessionsClosed += 1;
	State.SessionsActive = FMath::Max(0, State.SessionsActive - 1);
	State.SessionAt = Now();
	bDirty = true;
}

bool FMcpDiagnosticsSnapshot::PersistCurrent()
{
	FScopeLock Lock(&Mutex);
	if (DiagnosticsRoot().IsEmpty() || !EnsureDiagnosticsDirectory())
	{
		return false;
	}
	return WriteFileAtomic(CurrentFileName(), CurrentTempName(), McpDiagnosticsSchema::SerializeState(State, false));
}

bool FMcpDiagnosticsSnapshot::TryPersistCoalesced()
{
	FScopeLock Lock(&Mutex);
	if (!bDirty)
	{
		return false;
	}
	const double CurrentTime = Now();
	if (CurrentTime - LastPersistTime < CoalesceIntervalSeconds)
	{
		return false;
	}
	if (DiagnosticsRoot().IsEmpty() || !EnsureDiagnosticsDirectory())
	{
		return false;
	}
	bDirty = false;
	LastPersistTime = CurrentTime;
	return WriteFileAtomic(CurrentFileName(), CurrentTempName(), McpDiagnosticsSchema::SerializeState(State, false));
}

TSharedRef<FJsonObject> FMcpDiagnosticsSnapshot::CurrentSummaryJson() const
{
	FScopeLock Lock(&Mutex);
	return McpDiagnosticsSchema::BuildSnapshotJson(State, false);
}

TSharedRef<FJsonObject> FMcpDiagnosticsSnapshot::PreviousSummaryJson() const
{
	FScopeLock Lock(&Mutex);
	return bHasPrevious ? McpDiagnosticsSchema::BuildSnapshotJson(PreviousState, false) : MakeShared<FJsonObject>();
}

FString FMcpDiagnosticsSnapshot::DiagnosticsRoot() const
{
	return !RootOverride.IsEmpty() ? RootOverride : CachedRoot;
}

double FMcpDiagnosticsSnapshot::Now() const
{
	return Clock ? Clock() : static_cast<double>(FDateTime::UtcNow().ToUnixTimestamp());
}

bool FMcpDiagnosticsSnapshot::EnsureDiagnosticsDirectory() const
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	return !DiagnosticsRoot().IsEmpty()
		&& (PlatformFile.DirectoryExists(*DiagnosticsRoot()) || PlatformFile.CreateDirectoryTree(*DiagnosticsRoot()));
}

bool FMcpDiagnosticsSnapshot::WriteFileAtomic(const FString& TargetName, const FString& TempName, const FString& Content) const
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	const FString TempPath = FPaths::Combine(DiagnosticsRoot(), TempName);
	const FString TargetPath = FPaths::Combine(DiagnosticsRoot(), TargetName);
	if (!FFileHelper::SaveStringToFile(Content, *TempPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		return false;
	}
	return PlatformFile.MoveFile(*TargetPath, *TempPath);
}


