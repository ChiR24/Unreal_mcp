#include "MCP/Primitives/McpSubscriptionStore.h"
#include "Misc/ScopeLock.h"

FMcpSubscriptionStore::FMcpSubscriptionStore(int32 InMaxPerSession)
	: MaxPerSession(InMaxPerSession > 0 ? InMaxPerSession : 1)
{
}

void FMcpSubscriptionStore::SetReleaseHook(FReleaseHook InHook)
{
	OnRelease = MoveTemp(InHook);
}

bool FMcpSubscriptionStore::IsValidSession(const FString& SessionId)
{
	return !SessionId.TrimStartAndEnd().IsEmpty();
}

void FMcpSubscriptionStore::FireRelease(const FString& SessionId, const FString& Uri)
{
	if (OnRelease)
	{
		OnRelease(SessionId, Uri);
	}
}

FMcpSubscribeResult FMcpSubscriptionStore::Subscribe(const FString& SessionId, const FString& Uri)
{
	FMcpSubscribeResult Result;
	if (!IsValidSession(SessionId))
	{
		Result.Reason = TEXT("INVALID_SESSION");
		return Result;
	}
	if (!McpIsSubscribableUri(Uri))
	{
		Result.Reason = TEXT("NOT_SUBSCRIBABLE");
		return Result;
	}

	FString EvictedToRelease;
	{
		FScopeLock Lock(&StateMutex);
		TArray<FString>& Uris = Sessions.FindOrAdd(SessionId);
		if (Uris.Contains(Uri))
		{
			Result.bAccepted = true;
			Result.bAlreadySubscribed = true;
			return Result;
		}
		// At the cap a NEW subscription evicts the oldest (index 0) deterministically.
		if (Uris.Num() >= MaxPerSession && Uris.Num() > 0)
		{
			EvictedToRelease = Uris[0];
			Uris.RemoveAt(0);
			Result.Evicted = EvictedToRelease;
		}
		Uris.Add(Uri);
		Result.bAccepted = true;
	}

	if (!EvictedToRelease.IsEmpty())
	{
		FireRelease(SessionId, EvictedToRelease);
	}
	return Result;
}

bool FMcpSubscriptionStore::Unsubscribe(const FString& SessionId, const FString& Uri)
{
	if (!McpIsSubscribableUri(Uri))
	{
		return false;
	}
	{
		FScopeLock Lock(&StateMutex);
		TArray<FString>* Uris = Sessions.Find(SessionId);
		if (!Uris || !Uris->Contains(Uri))
		{
			return false;
		}
		Uris->Remove(Uri);
		if (Uris->Num() == 0)
		{
			Sessions.Remove(SessionId);
		}
	}
	FireRelease(SessionId, Uri);
	return true;
}

bool FMcpSubscriptionStore::IsSubscribed(const FString& SessionId, const FString& Uri) const
{
	if (!McpIsSubscribableUri(Uri))
	{
		return false;
	}
	FScopeLock Lock(&StateMutex);
	const TArray<FString>* Uris = Sessions.Find(SessionId);
	return Uris && Uris->Contains(Uri);
}

TArray<FString> FMcpSubscriptionStore::Subscriptions(const FString& SessionId) const
{
	FScopeLock Lock(&StateMutex);
	const TArray<FString>* Uris = Sessions.Find(SessionId);
	return Uris ? *Uris : TArray<FString>();
}

int32 FMcpSubscriptionStore::Count(const FString& SessionId) const
{
	FScopeLock Lock(&StateMutex);
	const TArray<FString>* Uris = Sessions.Find(SessionId);
	return Uris ? Uris->Num() : 0;
}

bool FMcpSubscriptionStore::HasSession(const FString& SessionId) const
{
	FScopeLock Lock(&StateMutex);
	return Sessions.Contains(SessionId);
}

int32 FMcpSubscriptionStore::SessionCount() const
{
	FScopeLock Lock(&StateMutex);
	return Sessions.Num();
}

TArray<FString> FMcpSubscriptionStore::SessionsSubscribedTo(const FString& Uri) const
{
	TArray<FString> Out;
	if (!McpIsSubscribableUri(Uri))
	{
		return Out;
	}
	FScopeLock Lock(&StateMutex);
	for (const auto& Pair : Sessions)
	{
		if (Pair.Value.Contains(Uri))
		{
			Out.Add(Pair.Key);
		}
	}
	return Out;
}

int32 FMcpSubscriptionStore::ClearSession(const FString& SessionId)
{
	TArray<FString> Released;
	{
		FScopeLock Lock(&StateMutex);
		if (TArray<FString>* Uris = Sessions.Find(SessionId))
		{
			Released = *Uris;
			Sessions.Remove(SessionId);
		}
	}
	for (const FString& Uri : Released)
	{
		FireRelease(SessionId, Uri);
	}
	return Released.Num();
}
