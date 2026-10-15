#pragma once

#include "Foundation/McpCapabilityPrincipal.h"
#include "Dom/JsonObject.h"

#if WITH_DEV_AUTOMATION_TESTS
// An authenticated test principal with Scopes, confined to Prefixes when any are given.
inline FMcpCapabilityPrincipal McpTestPrincipal(const TCHAR* Identity, const TArray<EMcpCapabilityScope>& Scopes,
	const TArray<FString>& Prefixes = {}, int32 MaxRequestsPerMinute = 0)
{
	FMcpCapabilityPrincipal Principal;
	Principal.Identity = Identity;
	Principal.Scopes = Scopes;
	Principal.AllowedPathPrefixes = Prefixes;
	Principal.MaxRequestsPerMinute = MaxRequestsPerMinute;
	Principal.bAuthenticated = true;
	return Principal;
}

// A JSON object of string fields.
inline TSharedPtr<FJsonObject> McpTestFields(const TMap<FString, FString>& Values)
{
	TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
	for (const TPair<FString, FString>& Value : Values)
	{
		Object->SetStringField(Value.Key, Value.Value);
	}
	return Object;
}
#endif
