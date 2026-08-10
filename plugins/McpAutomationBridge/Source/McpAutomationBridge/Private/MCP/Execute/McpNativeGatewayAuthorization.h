#pragma once

#include "CoreMinimal.h"

#include "Foundation/McpCapabilityAuthorization.h"
#include "MCP/Execute/McpNativeGatewayReceipt.h"

// Task 40 native /mcp security glue.
//
// The native surface binds a principal to a SESSION (the WebSocket bridge binds
// one to a socket) and re-consults it on every request, so a client cannot
// present one token at initialize and a different one later.

// Resolve the principal for a token presented on the native surface. Uses the
// same constant-time candidate scan as the WebSocket bridge, so both transports
// accept exactly the same tokens with exactly the same authority.
FMcpCapabilityPrincipal McpResolveNativePrincipal(const FString& PresentedToken);

// True when the presented token resolves to a DIFFERENT principal than the one
// bound to the session — a token swap, which is refused with 401 before the
// session is used for anything.
bool McpIsNativePrincipalSwap(
	const FMcpCapabilityPrincipal& Bound, const FMcpCapabilityPrincipal& Presented);

// Map a refusal onto the shared typed-error algebra. Every field mirrors the
// TypeScript discriminated union so a client sees the identical kind/code/payload
// whichever transport refused it.
FMcpSemanticError McpAuthorizationSemanticError(const FMcpAuthorizationDecision& Decision);

// Gate a native MCP primitive (resources/*, prompts/*, tools/list) on a `read`
// demand. Only tools/call used to consult the principal, so any principal past
// the transport token check — including a write-only or path-confined one —
// could read actor, asset and level data outside its grant.
//
// Returns an EMPTY string when the read is allowed, otherwise a ready-to-send
// JSON-RPC error body. ResourceUri may be empty for a non-addressed primitive;
// when it names a content path, that path is checked against the principal's
// allowed prefixes exactly as a tool call's payload paths are.
FString McpAuthorizePrimitiveRead(
	const FMcpCapabilityPrincipal& Principal,
	const FString& ResourceUri,
	const TSharedPtr<FJsonValue>& Id);
