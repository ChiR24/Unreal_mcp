#include "MCP/Transport/McpNativeTransportPrivate.h"
#include "MCP/Execute/McpNativeGatewayAuthorization.h"
#include "MCP/Gateway/McpNativeGatewayDefinition.h"
#include "Foundation/Diagnostics/McpDiagnosticsSnapshot.h"
#include "Misc/SecureHash.h"

namespace
{
FString BuildClientRateKey(
	const FString& ConnectionRemoteAddr,
	const FString& ClientName, const FString& ClientVersion)
{
	// ConnectionRemoteAddr is the primary input (anchored to the OS-level
	// remote IP:port, not attacker-controlled). ClientName/ClientVersion are
	// included as a secondary dimension so that two different clients behind
	// the same NAT (e.g. two browsers on the same machine) still get distinct
	// rate buckets. ClientName/ClientVersion alone MUST NOT be the key —
	// they are attacker-controlled via the initialize clientInfo JSON field.
	const FString Identity =
		ConnectionRemoteAddr.Left(128) + TEXT("\x1f") +
		ClientName.Left(64) + TEXT("\x1f") +
		ClientVersion.Left(32);
	return FMD5::HashAnsiString(*Identity);
}

// H7: the session-create hook defers its disk write to the game thread; the
// helper keeps the AsyncTask(GameThread) wrap in ONE file-local place.
void PersistSnapshotAsync()
{
	AsyncTask(ENamedThreads::GameThread,
		[]() { FMcpDiagnosticsSnapshot::Get().PersistCurrent(); });
}
}

FString FMcpNativeTransport::HandleInitialize(
	const TSharedPtr<FJsonObject>& Params, const TSharedPtr<FJsonValue>& Id,
	FString& OutSessionId, const FString& ConnectionRemoteAddr)
{
	// Negotiate the protocol version before allocating any session state.
	// Echo a supported version; for any well-formed (non-empty string) request
	// version, negotiate down to the latest supported one instead of erroring.
	// Missing/non-string/empty -> JSON-RPC -32602 with supported/requested data.
	FString NegotiatedVersion;
	FString NegotiationError;
	if (!NegotiateInitializeProtocolVersion(Params, NegotiatedVersion, NegotiationError))
	{
		auto Data = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> Supported;
		for (const FString& V : McpSupportedProtocolVersions())
		{
			Supported.Add(MakeShared<FJsonValueString>(V));
		}
		Data->SetArrayField(TEXT("supported"), Supported);
		FString Requested;
		if (Params.IsValid() && Params->TryGetStringField(TEXT("protocolVersion"), Requested))
		{
			Data->SetStringField(TEXT("requested"), Requested);
		}
		else
		{
			Data->SetField(TEXT("requested"), MakeShared<FJsonValueNull>());
		}
		OutSessionId.Reset();
		return FMcpJsonRpc::BuildError(
			Id, FMcpJsonRpc::ErrorInvalidParams, NegotiationError, Data);
	}

	FString ClientName = TEXT("unknown");
	FString ClientVersion = TEXT("unknown");
	if (Params.IsValid())
	{
		const TSharedPtr<FJsonObject>* ClientInfoObj = nullptr;
		if (Params->TryGetObjectField(TEXT("clientInfo"), ClientInfoObj) &&
			ClientInfoObj)
		{
			(*ClientInfoObj)->TryGetStringField(TEXT("name"), ClientName);
			(*ClientInfoObj)->TryGetStringField(TEXT("version"), ClientVersion);
		}
	}
	// Anchor the rate-limit key to the connection's remote address, not the
	// attacker-controlled clientInfo. The clientName/clientVersion are still
	// stored in the session metadata for diagnostics, but they MUST NOT be
	// the sole input to a rate-limit key — an attacker can rotate them at
	// will to bypass the per-client cap. The remote address changes only on
	// reconnect, which is the right granularity for session-creation rate
	// limiting. Falls back to clientInfo if the remote address is unknown
	// (e.g. a test transport that does not bind a real socket).
	const FString ClientRateKey = BuildClientRateKey(
		ConnectionRemoteAddr, ClientName, ClientVersion);

	// Gathered BEFORE SessionMutex: the second-tier reclaim below must not
	// evict a session that still owns an SSE call or a notification stream, and
	// the collection mutexes are never taken while the session map is locked.
	TSet<FString> SessionsWithLiveConnections;
	CollectSessionsWithLiveConnections(SessionsWithLiveConnections);

	int32 CurrentSessionCount;
	FString EvictedSessionId;
	{
		FScopeLock Lock(&SessionMutex);
		const double Now = FPlatformTime::Seconds();
		FString RateLimitError;
		if (!ConsumeClientRequestBudgetLocked(
				ClientRateKey, false, RateLimitError))
		{
			OutSessionId.Reset();
			return FMcpJsonRpc::BuildError(
				Id, FMcpJsonRpc::ErrorInvalidRequest, RateLimitError);
		}
		if (ActiveSessions.Num() >= MaxActiveSessions)
		{
			double OldestUnusedActivity = TNumericLimits<double>::Max();
			for (const TPair<FString, double>& Entry : ActiveSessions)
			{
				const FSessionRateState* RateState =
					SessionRateStates.Find(Entry.Key);
				if (RateState && !RateState->bHasClientActivity &&
					RateState->InitializationCompletedAt > 0.0 &&
					Now - RateState->InitializationCompletedAt >=
						AbandonedSessionGraceSeconds &&
					!SessionsWithLiveConnections.Contains(Entry.Key) &&
					Entry.Value < OldestUnusedActivity)
				{
					EvictedSessionId = Entry.Key;
					OldestUnusedActivity = Entry.Value;
				}
			}
			if (EvictedSessionId.IsEmpty())
			{
				// Second tier. The abandoned-session pass above also excludes
				// live streams, but it only accepts sessions that NEVER made a
				// request, so a client that issued even one call and then
				// vanished without DELETE (crash, Ctrl-C, container stop) kept
				// its slot until the 1-hour inactivity timer — and once every
				// slot was held that way, initialize hard-failed for everyone.
				// Reclaim the least-recently-active session that is meaningfully
				// idle AND owns no live stream, so nothing in flight is ever
				// cancelled to make room.
				double OldestIdleActivity = TNumericLimits<double>::Max();
				for (const TPair<FString, double>& Entry : ActiveSessions)
				{
					if (SessionsWithLiveConnections.Contains(Entry.Key) ||
						Now - Entry.Value < IdleSessionReclaimSeconds ||
						Entry.Value >= OldestIdleActivity)
					{
						continue;
					}
					EvictedSessionId = Entry.Key;
					OldestIdleActivity = Entry.Value;
				}
			}
			if (EvictedSessionId.IsEmpty())
			{
				OutSessionId.Reset();
				return FMcpJsonRpc::BuildError(
					Id, FMcpJsonRpc::ErrorInvalidRequest,
					TEXT("Native MCP session limit reached: every session is "
						"active or streaming. Close a session with HTTP DELETE "
						"and its Mcp-Session-Id, or retry shortly."));
			}
		ActiveSessions.Remove(EvictedSessionId);
		SessionRateStates.Remove(EvictedSessionId);
		SessionProtocolVersions.Remove(EvictedSessionId);
		SessionPrincipals.Remove(EvictedSessionId);
		}
		OutSessionId = FGuid::NewGuid().ToString();
		ActiveSessions.Add(OutSessionId, Now);
		SessionProtocolVersions.Add(OutSessionId, NegotiatedVersion);
		FSessionRateState RateState;
		RateState.ClientRateKey = ClientRateKey;
		SessionRateStates.Add(OutSessionId, RateState);
		CurrentSessionCount = ActiveSessions.Num();
	}
	// H7: record the native session create AFTER the SessionMutex scope closes
	// (the store's own mutex protects the memory record; only a truncated
	// SHA-256 identity of the raw session id is stored - never the raw value).
	// The disk write is deferred to the game thread.
	FMcpDiagnosticsSnapshot::Get().RecordSessionCreated(OutSessionId);
	PersistSnapshotAsync();
	if (!EvictedSessionId.IsEmpty())
	{
		CloseSessionConnections(EvictedSessionId);
		UE_LOG(LogMcpNativeTransport, Log,
			TEXT("Evicted abandoned native MCP session before initialize"));
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("protocolVersion"), NegotiatedVersion);

	auto Capabilities = MakeShared<FJsonObject>();
	// The public surface is the single static 'unreal' gateway tool, whose shape
	// never changes, so the tools capability omits listChanged entirely (omission
	// and an explicit false are different claims); the tools object is still sent.
	auto ToolsCapability = MakeShared<FJsonObject>();
	Capabilities->SetObjectField(TEXT("tools"), ToolsCapability);
	// Task 37: advertise exactly the implemented session-profile primitives —
	// resources (with subscribe), prompts, and completions — all backed by
	// HandlePrimitiveMethod. Nothing unbacked is ever claimed: no logging and no
	// list-changed member, and tasks only since Task 44 backed it below.
	auto ResourcesCapability = MakeShared<FJsonObject>();
	ResourcesCapability->SetBoolField(TEXT("subscribe"), true);
	Capabilities->SetObjectField(TEXT("resources"), ResourcesCapability);
	Capabilities->SetObjectField(TEXT("prompts"), MakeShared<FJsonObject>());
	Capabilities->SetObjectField(TEXT("completions"), MakeShared<FJsonObject>());
	// Task 44: tasks is advertised ONLY because FMcpTaskSurface answers all four
	// tasks/* methods and accepts a task-augmented tools/call. requests.tools.call
	// is the claim that a tools/call MAY be task-augmented; the surface then
	// refuses the mutating operations per call, which is a policy the capability
	// vocabulary cannot express at parameter granularity.
	auto TasksCapability = MakeShared<FJsonObject>();
	TasksCapability->SetObjectField(TEXT("list"), MakeShared<FJsonObject>());
	TasksCapability->SetObjectField(TEXT("cancel"), MakeShared<FJsonObject>());
	auto TasksToolRequests = MakeShared<FJsonObject>();
	TasksToolRequests->SetObjectField(TEXT("call"), MakeShared<FJsonObject>());
	auto TasksRequests = MakeShared<FJsonObject>();
	TasksRequests->SetObjectField(TEXT("tools"), TasksToolRequests);
	TasksCapability->SetObjectField(TEXT("requests"), TasksRequests);
	Capabilities->SetObjectField(TEXT("tasks"), TasksCapability);
	Result->SetObjectField(TEXT("capabilities"), Capabilities);

	auto ServerInfo = MakeShared<FJsonObject>();
	ServerInfo->SetStringField(TEXT("name"), ServerName);
	ServerInfo->SetStringField(TEXT("version"), ServerVersion);
	Result->SetObjectField(TEXT("serverInfo"), ServerInfo);

	FString CombinedInstructions = BaseInstructions;
	if (!UserInstructions.IsEmpty())
	{
		if (!CombinedInstructions.IsEmpty())
		{
			CombinedInstructions += TEXT("\n\n");
		}
		CombinedInstructions += UserInstructions;
	}
	if (!CombinedInstructions.IsEmpty())
	{
		Result->SetStringField(TEXT("instructions"), CombinedInstructions);
	}

	UE_LOG(LogMcpNativeTransport, Log,
		TEXT("MCP session initialized (active sessions: %d)"),
		CurrentSessionCount);

	return FMcpJsonRpc::BuildResponse(Id, Result);
}

FString FMcpNativeTransport::HandleToolsList(
	const TSharedPtr<FJsonValue>& Id, const FString& SessionId)
{
	// Discovery is a read of project capability state, so it is gated on the
	// session principal exactly like resources/* — not left open to any
	// principal that merely passed the transport token check.
	const FString Refusal =
		McpAuthorizePrimitiveRead(GetSessionPrincipal(SessionId), FString(), Id);
	if (!Refusal.IsEmpty())
	{
		return Refusal;
	}

	// The public surface is permanently the single static 'unreal' gateway tool;
	// the canonical 23 tools stay registered and reachable through it.
	auto Result = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Tools;
	Tools.Add(MakeShared<FJsonValueObject>(BuildUnrealGatewayToolDefinition()));
	Result->SetArrayField(TEXT("tools"), Tools);
	return FMcpJsonRpc::BuildResponse(Id, Result);
}

int32 FMcpNativeTransport::GetTotalToolCount() const
{
	return FMcpToolRegistry::Get().GetToolCount();
}
