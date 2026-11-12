#include "Transport/Connection/McpConnectionManagerPrivate.h"

#include "Core/Journal/McpChangeJournal.h"

namespace
{
// A drag in the viewport moves the journal every frame: one event a second carries the latest cursor.
constexpr double ChangesPushIntervalSeconds = 1.0;
}

void FMcpConnectionManager::FlushChangesUpdate()
{
	const uint64 Seq = McpChangeJournal::LatestSeq();
	const double Now = FPlatformTime::Seconds();
	if (Seq == LastChangesSeq || Now - LastChangesPushTime < ChangesPushIntervalSeconds)
	{
		return;
	}
	LastChangesSeq = Seq;
	LastChangesPushTime = Now;

	// Only a principal that may read hears that something changed, as only it may ask what.
	TArray<TSharedPtr<FMcpBridgeWebSocket>> Targets;
	{
		FScopeLock Lock(&AuthSocketsMutex);
		for (const TSharedPtr<FMcpBridgeWebSocket>& Sock : ActiveSockets)
		{
			const FMcpCapabilityPrincipal* Principal = Sock.IsValid() ? SocketPrincipals.Find(Sock.Get()) : nullptr;
			if (Principal && Sock->IsConnected() && AuthenticatedSockets.Contains(Sock.Get()) &&
				Principal->IsScopeAuthorized(EMcpCapabilityScope::Read))
			{
				Targets.Add(Sock);
			}
		}
	}
	if (Targets.Num() == 0)
	{
		return;
	}

	const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetStringField(TEXT("changesCursor"), McpChangeJournal::LatestCursor());
	McpSendAutomationEvent(TEXT("changes_moved"), Payload, Targets);
}
