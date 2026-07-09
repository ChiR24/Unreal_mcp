#include "MCP/Transport/McpNativeTransportPrivate.h"
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
		}
		OutSessionId = FGuid::NewGuid().ToString();
		ActiveSessions.Add(OutSessionId, Now);
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
	Result->SetStringField(TEXT("protocolVersion"), TEXT("2025-03-26"));

	auto Capabilities = MakeShared<FJsonObject>();
	auto ToolsCap = MakeShared<FJsonObject>();
	ToolsCap->SetBoolField(TEXT("listChanged"), true);
	Capabilities->SetObjectField(TEXT("tools"), ToolsCap);
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
	const TSharedPtr<FJsonValue>& Id)
{
	TSet<FString> EnabledTools = ToolManager.GetEnabledToolNames();
	TSharedPtr<FJsonObject> ToolsList =
		FMcpToolRegistry::Get().GetFilteredToolsResponse(EnabledTools);

	if (ToolsList.IsValid())
	{
		return FMcpJsonRpc::BuildResponse(Id, ToolsList);
	}

	auto EmptyResult = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> EmptyArray;
	EmptyResult->SetArrayField(TEXT("tools"), EmptyArray);
	return FMcpJsonRpc::BuildResponse(Id, EmptyResult);
}

int32 FMcpNativeTransport::GetTotalToolCount() const
{
	return FMcpToolRegistry::Get().GetToolCount();
}
