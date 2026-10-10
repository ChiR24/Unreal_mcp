// McpRequestOriginRegistry.cpp — see header for why the origin cannot live in
// a per-dispatch global.

#include "Core/Requests/McpRequestOriginRegistry.h"

FMcpRequestOriginRegistry& FMcpRequestOriginRegistry::Get()
{
	static FMcpRequestOriginRegistry Instance;
	return Instance;
}

void FMcpRequestOriginRegistry::Record(const FString& RequestId, ERequestOrigin Origin)
{
	if (RequestId.IsEmpty())
	{
		return;
	}
	FScopeLock Lock(&Mutex);
	if (Origins.Contains(RequestId))
	{
		Origins.Add(RequestId, Origin);
		return;
	}
	// Evict oldest-first, never the recording being made, so a flood of
	// never-answered requests cannot grow this map without bound while a
	// legitimately in-flight request keeps its entry.
	while (InsertionOrder.Num() >= MaxTrackedRequests && InsertionOrder.Num() > 0)
	{
		const FString Evicted = InsertionOrder[0];
		InsertionOrder.RemoveAt(0);
		Origins.Remove(Evicted);
	}
	Origins.Add(RequestId, Origin);
	InsertionOrder.Add(RequestId);
}

bool FMcpRequestOriginRegistry::Resolve(const FString& RequestId, ERequestOrigin& OutOrigin) const
{
	FScopeLock Lock(&Mutex);
	const ERequestOrigin* Found = Origins.Find(RequestId);
	if (Found == nullptr)
	{
		return false;
	}
	OutOrigin = *Found;
	return true;
}

void FMcpRequestOriginRegistry::Forget(const FString& RequestId)
{
	FScopeLock Lock(&Mutex);
	if (Origins.Remove(RequestId) > 0)
	{
		InsertionOrder.RemoveSingle(RequestId);
	}
}

void FMcpRequestOriginRegistry::Reset()
{
	FScopeLock Lock(&Mutex);
	Origins.Reset();
	InsertionOrder.Reset();
}

int32 FMcpRequestOriginRegistry::Num() const
{
	FScopeLock Lock(&Mutex);
	return Origins.Num();
}
