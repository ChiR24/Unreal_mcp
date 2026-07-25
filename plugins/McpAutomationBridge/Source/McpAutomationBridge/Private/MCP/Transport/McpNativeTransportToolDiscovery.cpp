#include "MCP/Transport/McpNativeTransportPrivate.h"
#include "MCP/Execute/McpNativeGatewayAuthorization.h"
#include "MCP/Gateway/McpNativeGatewayDefinition.h"
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
					Entry.Value < OldestUnusedActivity)
				{
					EvictedSessionId = Entry.Key;
					OldestUnusedActivity = Entry.Value;
				}
			}
			if (EvictedSessionId.IsEmpty())
			{
				OutSessionId.Reset();
				return FMcpJsonRpc::BuildError(
					Id, FMcpJsonRpc::ErrorInvalidRequest,
					TEXT("Native MCP session limit reached"));
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
	// HandlePrimitiveMethod. Nothing else is claimed (no tasks, no logging, no
	// list-changed member) so the surface never advertises an unbacked primitive.
	auto ResourcesCapability = MakeShared<FJsonObject>();
	ResourcesCapability->SetBoolField(TEXT("subscribe"), true);
	Capabilities->SetObjectField(TEXT("resources"), ResourcesCapability);
	Capabilities->SetObjectField(TEXT("prompts"), MakeShared<FJsonObject>());
	Capabilities->SetObjectField(TEXT("completions"), MakeShared<FJsonObject>());
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
