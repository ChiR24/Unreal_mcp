// McpClientProfileStore.h
// Task 35 (native): standalone, explicit-session store for per-session client
// capability profiles. Mirrors src/server/mcp-primitives/client-profile-store.ts.
// It is decoupled from the transport session objects — callers pass an explicit
// session id and MUST call ClearSession on disconnect (Task 37 owns the lifecycle
// wiring). No process/global state leaks across sessions.
#pragma once

#include "CoreMinimal.h"
#include "McpSessionCapabilityProfile.h"

class FMcpClientProfileStore
{
public:
	void SetSession(const FString& SessionId, const FMcpSessionCapabilityProfile& Profile);
	bool GetSession(const FString& SessionId, FMcpSessionCapabilityProfile& OutProfile) const;
	bool HasSession(const FString& SessionId) const;
	void ClearSession(const FString& SessionId);
	int32 Num() const;

private:
	TMap<FString, FMcpSessionCapabilityProfile> Profiles;
	mutable FCriticalSection ProfilesLock;
};
