// Subscriptions on the native door: subscriptions/listen (2026-07-28), one long-lived SSE answer that is acknowledged
// first and then carries notifications/resources/updated, and resources/subscribe (earlier versions), whose updates
// ride the session's GET stream. The only resource that changes while the server runs is ue://changes, so updates
// come from the keepalive thread when the change journal's number moves: at most one per stream a second.
// Transport members kept here because Transport/ is full.

#include "MCP/Transport/McpNativeTransportPrivate.h"

#include "Core/Journal/McpChangeJournal.h"
#include "MCP/Execute/McpNativeGatewayAuthorization.h"
#include "MCP/Resources/McpResourceCatalog.h"

namespace
{
const TCHAR* const SubscriptionIdKey = TEXT("io.modelcontextprotocol/subscriptionId");

// Every listed field present has its type: three flags and a list of resource URIs (the TypeScript SDK's filter).
bool IsValidFilter(const TSharedPtr<FJsonObject>& Filter, TArray<FString>& OutUris)
{
	for (const TCHAR* Flag : { TEXT("toolsListChanged"), TEXT("promptsListChanged"), TEXT("resourcesListChanged") })
	{
		const TSharedPtr<FJsonValue> Value = Filter->TryGetField(Flag);
		if (Value.IsValid() && Value->Type != EJson::Boolean)
		{
			return false;
		}
	}
	const TSharedPtr<FJsonValue> Uris = Filter->TryGetField(TEXT("resourceSubscriptions"));
	if (!Uris.IsValid())
	{
		return true;
	}
	if (Uris->Type != EJson::Array)
	{
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Uri : Uris->AsArray())
	{
		if (!Uri.IsValid() || Uri->Type != EJson::String)
		{
			return false;
		}
		OutUris.Add(Uri->AsString());
	}
	return true;
}

// A notification as a listen stream carries it: the listen request's id, verbatim, in params._meta.
FString StreamNotification(const TCHAR* Method, const TSharedPtr<FJsonObject>& Params, const TSharedPtr<FJsonValue>& ListenId)
{
	if (ListenId.IsValid())
	{
		const TSharedPtr<FJsonObject> Meta = MakeShared<FJsonObject>();
		Meta->SetField(SubscriptionIdKey, ListenId);
		Params->SetObjectField(TEXT("_meta"), Meta);
	}
	return FMcpJsonRpc::BuildNotification(Method, Params);
}

FString ChangesUpdated(const TSharedPtr<FJsonValue>& ListenId)
{
	const TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(TEXT("uri"), McpResourceCatalog::ChangesUri());
	return StreamNotification(TEXT("notifications/resources/updated"), Params, ListenId);
}
} // namespace

void FMcpNativeTransport::HandleSubscriptionsListen(FSocket* ClientSocket, const FParsedHttpRequest& HttpReq,
	const FMcpJsonRpcRequest& Rpc, const FString& SessionId)
{
	const auto Refuse = [&](const FString& Body)
	{
		SendAndClose(ClientSocket, 200, TEXT("application/json"), Body, {}, HttpReq.Origin);
	};
	const TSharedPtr<FJsonObject>* Filter = nullptr;
	TArray<FString> Uris;
	if (!Rpc.Params.IsValid() || !Rpc.Params->TryGetObjectField(TEXT("notifications"), Filter) || !Filter ||
		!IsValidFilter(*Filter, Uris))
	{
		Refuse(FMcpJsonRpc::BuildError(Rpc.Id, FMcpJsonRpc::ErrorInvalidParams,
			TEXT("Invalid params: 'notifications' is required and must be a valid SubscriptionFilter")));
		return;
	}
	// Each resource is gated as reading it is: a token that may not read it hears nothing of it.
	const FMcpCapabilityPrincipal Principal = GetSessionPrincipal(SessionId);
	for (const FString& Uri : Uris)
	{
		const FString Refusal = McpAuthorizePrimitiveRead(Principal, Uri, Rpc.Id);
		if (!Refusal.IsEmpty())
		{
			Refuse(Refusal);
			return;
		}
	}

	const TSharedPtr<FNotificationStream> Stream = MakeShared<FNotificationStream>();
	Stream->Socket = ClientSocket;
	Stream->SessionId = SessionId;
	Stream->StreamId = FGuid::NewGuid().ToString();
	Stream->StartTime = Stream->LastKeepaliveTime = FPlatformTime::Seconds();
	Stream->ListenId = Rpc.Id;
	Stream->bListenChanges = Uris.Contains(McpResourceCatalog::ChangesUri());
	Stream->ChangesSeq = McpChangeJournal::LatestSeq();
	{
		// The same caps as GET /mcp streams; modern clients share one session per principal, so this is per token.
		FScopeLock Lock(&NotificationStreamsMutex);
		int32 Total = 0;
		int32 Mine = 0;
		for (const auto& [Id, Existing] : NotificationStreams)
		{
			if (Existing.IsValid() && !Existing->bMarkedForRemoval.load())
			{
				++Total;
				if (Existing->SessionId == SessionId)
				{
					++Mine;
				}
			}
		}
		if (Total < MaxTotalNotificationStreams && Mine < MaxNotificationStreamsPerSession)
		{
			NotificationStreams.Add(Stream->StreamId, Stream);
		}
		else
		{
			Stream->Socket = nullptr;
		}
	}
	if (!Stream->Socket)
	{
		Refuse(FMcpJsonRpc::BuildError(Rpc.Id, FMcpJsonRpc::ErrorInternalError, TEXT("Subscription limit reached")));
		return;
	}

	// What this server can deliver: its tool, prompt and resource lists never change while it runs (no listChanged),
	// so the acknowledgement keeps only the resource subscriptions. Nothing goes before it on the stream.
	const TSharedPtr<FJsonObject> Honored = MakeShared<FJsonObject>();
	if (Uris.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		for (const FString& Uri : Uris)
		{
			Values.Add(MakeShared<FJsonValueString>(Uri));
		}
		Honored->SetArrayField(TEXT("resourceSubscriptions"), Values);
	}
	const TSharedPtr<FJsonObject> AckParams = MakeShared<FJsonObject>();
	AckParams->SetObjectField(TEXT("notifications"), Honored);
	const FString Ack = StreamNotification(TEXT("notifications/subscriptions/acknowledged"), AckParams, Rpc.Id);
	bool bOpened = false;
	{
		FScopeLock WriteLock(&Stream->WriteMutex);
		bOpened = SendSSEHeaders(ClientSocket, FString(), HttpReq.Origin) && SendSSEFrame(ClientSocket, Ack);
		if (!bOpened)
		{
			CloseSocket(Stream->Socket);
		}
	}
	if (!bOpened)
	{
		FScopeLock Lock(&NotificationStreamsMutex);
		NotificationStreams.Remove(Stream->StreamId);
		return;
	}
	Stream->bReady.store(true);
	TouchSession(SessionId);
	// Socket parked: the keepalive thread writes to it until the client closes it or the server stops.
}

FString FMcpNativeTransport::HandleResourceSubscription(
	const FString& Method, const FString& Uri, const TSharedPtr<FJsonValue>& Id, const FString& SessionId)
{
	if (Uri.IsEmpty())
	{
		return FMcpJsonRpc::BuildError(Id, FMcpJsonRpc::ErrorInvalidParams, TEXT("Invalid params: uri is required"));
	}
	// Any listed resource may be subscribed to; only ue://changes ever moves, so only it is tracked.
	if (Uri == McpResourceCatalog::ChangesUri())
	{
		FScopeLock Lock(&LogEventSubscriptionsMutex);
		if (Method == TEXT("resources/subscribe"))
		{
			ChangesSubscribedSessions.Add(SessionId, McpChangeJournal::LatestSeq());
		}
		else
		{
			ChangesSubscribedSessions.Remove(SessionId);
		}
	}
	return FMcpJsonRpc::BuildResponse(Id, MakeShared<FJsonObject>());
}

void FMcpNativeTransport::SweepChangeNotifications()
{
	const uint64 Seq = McpChangeJournal::LatestSeq();
	TSet<FString> DueSessions;
	{
		FScopeLock Lock(&LogEventSubscriptionsMutex);
		for (TPair<FString, uint64>& Heard : ChangesSubscribedSessions)
		{
			if (Heard.Value != Seq)
			{
				DueSessions.Add(Heard.Key);
				Heard.Value = Seq;
			}
		}
	}
	TArray<TPair<TSharedPtr<FNotificationStream>, FString>> Sends;
	{
		FScopeLock Lock(&NotificationStreamsMutex);
		for (const auto& [StreamId, Stream] : NotificationStreams)
		{
			if (!Stream.IsValid() || !Stream->bReady.load() || Stream->bMarkedForRemoval.load())
			{
				continue;
			}
			if (Stream->ListenId.IsValid())
			{
				if (Stream->bListenChanges && Stream->ChangesSeq != Seq)
				{
					Stream->ChangesSeq = Seq;
					Sends.Emplace(Stream, ChangesUpdated(Stream->ListenId));
				}
			}
			else if (DueSessions.Contains(Stream->SessionId))
			{
				Sends.Emplace(Stream, ChangesUpdated(nullptr));
			}
		}
	}
	for (const TPair<TSharedPtr<FNotificationStream>, FString>& Send : Sends)
	{
		if (WriteNotificationEvent(*Send.Key, Send.Value))
		{
			TouchSession(Send.Key->SessionId);
		}
		else
		{
			Send.Key->bMarkedForRemoval.store(true);
		}
	}
}

FString FMcpNativeTransport::ListenEndFrame(const TSharedPtr<FJsonValue>& ListenId) const
{
	// The graceful close of a listen: its own result, naming the subscription and this server.
	const TSharedPtr<FJsonObject> ServerInfo = MakeShared<FJsonObject>();
	ServerInfo->SetStringField(TEXT("name"), ServerName);
	ServerInfo->SetStringField(TEXT("version"), ServerVersion.IsEmpty() ? FString(TEXT("unknown")) : ServerVersion);
	const TSharedPtr<FJsonObject> Meta = MakeShared<FJsonObject>();
	Meta->SetField(SubscriptionIdKey, ListenId);
	Meta->SetObjectField(TEXT("io.modelcontextprotocol/serverInfo"), ServerInfo);
	const TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("resultType"), TEXT("complete"));
	Result->SetObjectField(TEXT("_meta"), Meta);
	return FMcpJsonRpc::BuildResponse(ListenId, Result);
}
