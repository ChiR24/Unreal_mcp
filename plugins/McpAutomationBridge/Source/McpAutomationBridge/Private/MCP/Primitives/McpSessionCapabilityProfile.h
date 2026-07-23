// McpSessionCapabilityProfile.h
// Task 35 primitive C3 (native mirror): adaptive per-session client-capability
// profile. Native counterpart of
// src/server/mcp-primitives/session-capability-profile.ts and the bounded
// fallback-pointer table in src/server/mcp-primitives/fallback-pointers.ts.
// Metadata only: NO transport wiring and NO session/lifecycle edits (Task 37
// wires these into initialize handling). It derives six capability booleans
// STRUCTURALLY from the declared MCP capabilities and never inspects the client
// name or version.
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// The six structural capability booleans. Mirrors the TypeScript
// ClientCapabilityProfile. Two clients that declare identical capabilities get
// identical booleans regardless of brand.
struct FMcpSessionCapabilityProfile
{
	bool bHasResources = false;
	bool bHasPrompts = false;
	bool bHasCompletions = false;
	bool bHasSubscriptions = false;
	bool bHasElicitation = false;
	bool bHasTasks = false;
};

namespace McpClientProfileInternal
{
	// A capability key is present when declared as a nested object or a bare
	// boolean true. Anything else is absent, so a malformed value can never
	// advertise a capability.
	inline bool FieldPresent(const TSharedPtr<FJsonObject>& Object, const FString& Key)
	{
		if (!Object.IsValid())
		{
			return false;
		}
		const TSharedPtr<FJsonObject>* AsObject = nullptr;
		if (Object->TryGetObjectField(Key, AsObject) && AsObject != nullptr && (*AsObject).IsValid())
		{
			return true;
		}
		bool bAsBool = false;
		return Object->TryGetBoolField(Key, bAsBool) && bAsBool;
	}

	// Present at the top level or nested under `experimental`. Reads only the
	// declared capability object, never a name/version field.
	inline bool HasStructuralKey(const TSharedPtr<FJsonObject>& Capabilities, const FString& Key)
	{
		if (FieldPresent(Capabilities, Key))
		{
			return true;
		}
		const TSharedPtr<FJsonObject>* Experimental = nullptr;
		if (Capabilities.IsValid() &&
			Capabilities->TryGetObjectField(TEXT("experimental"), Experimental) &&
			Experimental != nullptr)
		{
			return FieldPresent(*Experimental, Key);
		}
		return false;
	}
}

// Derive the structural profile from the declared client capabilities. An
// invalid/empty object yields the all-false profile rather than asserting, so a
// hostile or broken initialize can neither crash nor falsely enable a feature.
inline FMcpSessionCapabilityProfile McpParseSessionCapabilityProfile(const TSharedPtr<FJsonObject>& Capabilities)
{
	using namespace McpClientProfileInternal;

	FMcpSessionCapabilityProfile Profile;
	if (!Capabilities.IsValid())
	{
		return Profile;
	}

	Profile.bHasResources = HasStructuralKey(Capabilities, TEXT("resources"));
	Profile.bHasPrompts = HasStructuralKey(Capabilities, TEXT("prompts"));
	Profile.bHasCompletions = HasStructuralKey(Capabilities, TEXT("completions"));
	Profile.bHasElicitation = HasStructuralKey(Capabilities, TEXT("elicitation"));
	Profile.bHasTasks = HasStructuralKey(Capabilities, TEXT("tasks"));

	const TSharedPtr<FJsonObject>* Resources = nullptr;
	bool bSubscribe = false;
	if (Capabilities->TryGetObjectField(TEXT("resources"), Resources) &&
		Resources != nullptr &&
		(*Resources)->TryGetBoolField(TEXT("subscribe"), bSubscribe) &&
		bSubscribe)
	{
		Profile.bHasSubscriptions = true;
	}
	else
	{
		Profile.bHasSubscriptions = HasStructuralKey(Capabilities, TEXT("subscriptions"));
	}

	return Profile;
}

// One bounded fallback pointer. Mirrors fallback-pointers.ts: a client that HAS
// the primitive gets a native method reference; one that lacks it gets exactly
// ONE bounded gateway operation. No schema/knowledge dump.
struct FMcpFallbackPointer
{
	FString Primitive;
	FString Mode;      // "native" (client supports it) or "gateway" (bounded fallback)
	FString Reference; // native method or single gateway operation
};

namespace McpClientProfileInternal
{
	inline bool ProfileSupports(const FMcpSessionCapabilityProfile& Profile, const FString& Primitive)
	{
		if (Primitive == TEXT("resources")) return Profile.bHasResources;
		if (Primitive == TEXT("prompts")) return Profile.bHasPrompts;
		if (Primitive == TEXT("completions")) return Profile.bHasCompletions;
		if (Primitive == TEXT("subscriptions")) return Profile.bHasSubscriptions;
		if (Primitive == TEXT("tasks")) return Profile.bHasTasks;
		return false;
	}

	inline FString NativeMethodFor(const FString& Primitive)
	{
		if (Primitive == TEXT("resources")) return TEXT("resources/list");
		if (Primitive == TEXT("prompts")) return TEXT("prompts/list");
		if (Primitive == TEXT("completions")) return TEXT("completion/complete");
		if (Primitive == TEXT("subscriptions")) return TEXT("resources/subscribe");
		if (Primitive == TEXT("tasks")) return TEXT("tasks/list");
		return FString();
	}

	inline FString GatewayOperationFor(const FString& Primitive)
	{
		if (Primitive == TEXT("prompts")) return TEXT("describe");
		if (Primitive == TEXT("tasks")) return TEXT("execute");
		return TEXT("search");
	}
}

// The bounded pointer for one primitive under the given profile. Deterministic.
inline FMcpFallbackPointer McpFallbackPointerFor(const FMcpSessionCapabilityProfile& Profile, const FString& Primitive)
{
	const bool bSupported = McpClientProfileInternal::ProfileSupports(Profile, Primitive);
	FMcpFallbackPointer Pointer;
	Pointer.Primitive = Primitive;
	Pointer.Mode = bSupported ? TEXT("native") : TEXT("gateway");
	Pointer.Reference = bSupported
		? McpClientProfileInternal::NativeMethodFor(Primitive)
		: McpClientProfileInternal::GatewayOperationFor(Primitive);
	return Pointer;
}
