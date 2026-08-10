// McpClientProfileStore.cpp
// Task 35 (native): standalone explicit-session profile store implementation.
// Thread-safe map operations only — no editor calls, no transport/session
// coupling, no package saves. Mirrors client-profile-store.ts.
#include "McpClientProfileStore.h"

void FMcpClientProfileStore::SetSession(const FString& SessionId, const FMcpSessionCapabilityProfile& Profile)
{
	if (SessionId.IsEmpty())
	{
		return;
	}
	FScopeLock Lock(&ProfilesLock);
	Profiles.Add(SessionId, Profile);
}

bool FMcpClientProfileStore::GetSession(const FString& SessionId, FMcpSessionCapabilityProfile& OutProfile) const
{
	FScopeLock Lock(&ProfilesLock);
	if (const FMcpSessionCapabilityProfile* Found = Profiles.Find(SessionId))
	{
		OutProfile = *Found;
		return true;
	}
	return false;
}

bool FMcpClientProfileStore::HasSession(const FString& SessionId) const
{
	FScopeLock Lock(&ProfilesLock);
	return Profiles.Contains(SessionId);
}

void FMcpClientProfileStore::ClearSession(const FString& SessionId)
{
	FScopeLock Lock(&ProfilesLock);
	Profiles.Remove(SessionId);
}

int32 FMcpClientProfileStore::Num() const
{
	FScopeLock Lock(&ProfilesLock);
	return Profiles.Num();
}
