#pragma once

#include "CoreMinimal.h"

// Saved/MCP/endpoint.json (McpEndpointFile.cpp): where this editor's native MCP server listens, for the stdio proxy
// (src/proxy/endpoint.ts): {pid, port, pluginVersion, startedAtUtc, state}. "listening" once bound, "closed_cleanly"
// on a clean shutdown; never deleted, so a proxy that finds the pid gone tells a crash from a close.
namespace McpEndpointFile
{
void WriteListening(int32 Port);
void WriteClosed();

/** NativeMCPPort 0: this project's own port, a hash of its .uproject path into 49152-65535. */
int32 ProjectPort();
}
