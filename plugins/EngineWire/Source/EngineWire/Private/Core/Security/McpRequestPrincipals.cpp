#include "Core/Security/McpPrequeueGate.h"

#include "Misc/ScopeLock.h"

// RequestId -> the principal the gate allowed it for, until its reply forgets it. Bounded like the burned-nonce map
// beside the gate, so a request that never answers cannot grow it for the life of the editor.
namespace McpPrequeueGate
{
namespace
{
constexpr int32 MaxRemembered = 512;

FCriticalSection& PrincipalMutex()
{
	static FCriticalSection Mutex;
	return Mutex;
}

TMap<FString, FMcpCapabilityPrincipal>& PrincipalByRequest()
{
	static TMap<FString, FMcpCapabilityPrincipal> Map;
	return Map;
}

TArray<FString>& PrincipalOrder()
{
	static TArray<FString> Order;
	return Order;
}
} // namespace

void RememberRequestPrincipal(const FString& RequestId, const FMcpCapabilityPrincipal& Principal)
{
	if (RequestId.IsEmpty())
	{
		return;
	}
	FScopeLock Lock(&PrincipalMutex());
	if (!PrincipalByRequest().Contains(RequestId))
	{
		if (PrincipalOrder().Num() >= MaxRemembered)
		{
			PrincipalByRequest().Remove(PrincipalOrder()[0]);
			PrincipalOrder().RemoveAt(0);
		}
		PrincipalOrder().Add(RequestId);
	}
	PrincipalByRequest().Add(RequestId, Principal);
}

bool FindRequestPrincipal(const FString& RequestId, FMcpCapabilityPrincipal& OutPrincipal)
{
	FScopeLock Lock(&PrincipalMutex());
	const FMcpCapabilityPrincipal* Found = PrincipalByRequest().Find(RequestId);
	if (!Found)
	{
		return false;
	}
	OutPrincipal = *Found;
	return true;
}

void ForgetRequestPrincipal(const FString& RequestId)
{
	FScopeLock Lock(&PrincipalMutex());
	if (PrincipalByRequest().Remove(RequestId) > 0)
	{
		PrincipalOrder().RemoveSingle(RequestId);
	}
}
} // namespace McpPrequeueGate
