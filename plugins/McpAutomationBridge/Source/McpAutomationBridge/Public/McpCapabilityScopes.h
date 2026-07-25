#pragma once

#include "CoreMinimal.h"

#include "McpCapabilityScopes.generated.h"

// Task 40 canonical capability scopes. Exact-set semantics with an Admin
// wildcard: a principal is authorized iff it holds Admin OR the exact required
// scope. This is NOT rank-based — Write does NOT imply Read or Destructive. The
// same predicate is mirrored by the TypeScript fail-fast layer; the plugin
// remains the sole authority and re-enforces every request.
UENUM()
enum class EMcpCapabilityScope : uint8
{
	Read        UMETA(DisplayName = "Read"),
	Write       UMETA(DisplayName = "Write"),
	Destructive UMETA(DisplayName = "Destructive"),
	Admin       UMETA(DisplayName = "Admin")
};

// A configured scoped capability token. Configured settings remain the only
// durable token storage; the secret Token is never emitted in a log, receipt,
// authority descriptor, principal identity, or evidence. A scoped token may list
// only Read/Write/Destructive, never Admin; a scoped-Admin entry, an empty
// token, or a duplicate profile/token is invalid and ignored with a token-free
// warning at resolve time. A scoped token that collides with the legacy token
// wins (narrower).
USTRUCT()
struct FMcpScopedCapabilityToken
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "MCP|Scoped Token")
	FString Profile;

	UPROPERTY(EditAnywhere, Category = "MCP|Scoped Token")
	FString Token;

	UPROPERTY(EditAnywhere, Category = "MCP|Scoped Token")
	TArray<EMcpCapabilityScope> Scopes;

	UPROPERTY(EditAnywhere, Category = "MCP|Scoped Token")
	TArray<FString> AllowedPathPrefixes;

	UPROPERTY(EditAnywhere, Category = "MCP|Scoped Token")
	TArray<FString> AllowedProjects;

	UPROPERTY(EditAnywhere, Category = "MCP|Scoped Token", meta = (ClampMin = "0"))
	int32 MaxRequestsPerMinute = 0;

	UPROPERTY(EditAnywhere, Category = "MCP|Scoped Token", meta = (ClampMin = "0"))
	int32 MaxToolCallsPerMinute = 0;
};
