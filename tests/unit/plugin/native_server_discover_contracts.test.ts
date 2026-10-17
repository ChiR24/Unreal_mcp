/**
 * Source contracts for `server/discover` on the native /mcp transport (MCP 2026-07-28 discovery, issue #614). It is
 * answered without a session or protocol-version header, like initialize, and lists the version a request can name
 * in its _meta (2026-07-28), as the TypeScript SDK does; initialize negotiates the older ones. The capability-token
 * check before session validation still applies to it. The C++ cannot run here.
 */

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';

const transport = resolve(process.cwd(), 'plugins/EngineWire/Source/EngineWire/Private/MCP/Transport');
const code = (file: string): string =>
  readFileSync(resolve(transport, file), 'utf8').replace(/\/\*[\s\S]*?\*\//gu, '').replace(/^[ \t]*\/\/.*$/gmu, '');

describe('native server/discover', () => {
  const connection = code('McpNativeTransportConnection.cpp');
  const discovery = code('McpNativeTransportToolDiscovery.cpp');

  it('skips the session and protocol-version checks exactly as initialize does, after the token check', () => {
    expect(connection).toContain('const bool bSessionless = Rpc.Method == TEXT("initialize") || Rpc.Method == TEXT("server/discover");');
    expect(connection).toContain('if (!bSessionless && !GuardProtocolVersionHeader(ClientSocket, HttpReq, Rpc.Id, true)) return;');
    expect(connection.indexOf('bTokenRequired')).toBeLessThan(connection.indexOf('const bool bSessionless'));
  });

  it('answers with the per-request versions, the initialize capabilities and identity, and cache hints', () => {
    // As the TypeScript SDK answers: only what a request can name in its _meta; initialize negotiates the rest.
    expect(discovery).toContain('TArray<TSharedPtr<FJsonValue>> Versions = {MakeShared<FJsonValueString>(McpModern::Version)};');
    expect(discovery).not.toContain('for (const FString& Version : McpSupportedProtocolVersions())');
    expect(discovery).toContain('Meta->SetObjectField(TEXT("io.modelcontextprotocol/serverInfo"), DescribeServer(Result));');
    expect(discovery).toContain('Result->SetObjectField(TEXT("serverInfo"), DescribeServer(Result));');
    expect(discovery).toContain('Result->SetStringField(TEXT("cacheScope"), TEXT("public"));');
  });
});
