import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const privateSource = (...parts: string[]): string =>
  readFileSync(
    resolve(
      process.cwd(),
      'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private',
      ...parts,
    ),
    'utf8',
  );

describe('plugin security contracts', () => {
  it('never delivers an automation response to an unrelated active socket', () => {
    const source = privateSource(
      'Transport',
      'Connection',
      'McpConnectionManagerResponses.cpp',
    );
    const responseFunction = source.slice(
      source.indexOf('void FMcpConnectionManager::SendAutomationResponse'),
      source.indexOf('void FMcpConnectionManager::SendProgressUpdate'),
    );

    expect(responseFunction).not.toContain(
      'for (const TSharedPtr<FMcpBridgeWebSocket> &Sock : ActiveSockets)',
    );
    expect(responseFunction).not.toContain('SendControlMessage(FallbackEvent)');
  });

  it('resolves import sources through the project file security boundary', () => {
    const source = privateSource(
      'Domains',
      'AssetWorkflow',
      'Operations',
      'McpAutomationBridge_AssetWorkflowImportDuplicate.cpp',
    );

    expect(source).toMatch(/McpResolveProjectFilePath\(\s*SourcePath/u);
    expect(source.search(/McpResolveProjectFilePath\(\s*SourcePath/u)).toBeLessThan(
      source.indexOf('FPaths::FileExists(ResolvedSourcePath)'),
    );
  });

  it('compares capability tokens in constant time on both transports', () => {
    // The shared helper exists and performs a length-safe, byte-wise constant
    // comparison (no data-dependent early exit that would leak match length).
    let helper = '';
    try {
      helper = privateSource('Foundation', 'McpSecureTokenCompare.h');
    } catch {
      helper = '';
    }
    expect(helper).toContain('inline bool McpConstantTimeTokenEquals');
    expect(helper).toContain('Diff |=');

    // Native HTTP transport uses it for the X-MCP-Capability-Token check.
    const nativeConn = privateSource(
      'MCP',
      'Transport',
      'McpNativeTransportConnection.cpp',
    );
    expect(nativeConn).toContain('McpConstantTimeTokenEquals(HttpReq.CapabilityToken, Settings->CapabilityToken)');

    // WebSocket bridge_hello uses it for the capabilityToken check.
    const wsMessages = privateSource(
      'Transport',
      'Connection',
      'McpConnectionManagerMessages.cpp',
    );
    expect(wsMessages).toContain('McpConstantTimeTokenEquals(ReceivedToken, CapabilityToken)');
  });

  it('refuses non-loopback native startup unless capability token is required (fail-closed)', () => {
    const lifecycle = privateSource(
      'MCP',
      'Transport',
      'McpNativeTransportLifecycle.cpp',
    );
    // The non-loopback bind branch couples LAN exposure to token auth and
    // refuses to start (return false) when the token is not required.
    expect(lifecycle).toContain('SECURITY: refusing to bind native MCP to non-loopback');
    expect(lifecycle).toContain('bRequireCapabilityToken');
    // Disallowed-host loopback fallback must be preserved (separate branch).
    expect(lifecycle).toContain('falling back to 127.0.0.1');
  });

  it('refuses non-loopback WebSocket startup unless capability token is required and destroys the listen socket before returning (fail-closed)', () => {
    const server = privateSource(
      'Transport',
      'WebSocket',
      'McpBridgeWebSocketServer.cpp',
    );
    // The non-loopback bind branch couples LAN exposure to token auth and
    // refuses to start the listener (return 0) when the token is not required.
    // There is NO unauthenticated loopback fallback for the unsafe explicit
    // non-loopback path — the operator must enable Require Capability Token.
    expect(server).toContain(
      'SECURITY: refusing to bind WebSocket bridge to non-loopback',
    );
    expect(server).toContain('bRequireCapabilityToken');
    // Default loopback behavior and the disallowed-host loopback fallback must
    // still be preserved (separate branches).
    expect(server).toContain('Falling back to 127.0.0.1');

    // The refusal must not leak the already-created listen socket: it must
    // explicitly destroy it before the function returns (the ON_SCOPE_EXIT
    // guard is only a backstop).
    const secIdx = server.indexOf(
      'SECURITY: refusing to bind WebSocket bridge to non-loopback',
    );
    const destroyIdx = server.indexOf('DestroyListenSocket();', secIdx);
    const retIdx = server.indexOf('return 0;', secIdx);
    // Both must live after the refusal log, and the destroy must precede return.
    expect(destroyIdx).toBeGreaterThan(secIdx);
    expect(retIdx).toBeGreaterThan(secIdx);
    expect(destroyIdx).toBeLessThan(retIdx);
  });

  it('bounds render workload controls before applying CVars', () => {
    const consoleSource = privateSource(
      'Domains',
      'Render',
      'McpAutomationBridge_RenderConsole.cpp',
    );
    const renderTargetSource = privateSource(
      'Domains',
      'Render',
      'McpAutomationBridge_RenderTargets.cpp',
    );

    expect(consoleSource).toContain('ReadBoundedNumberSetting');
    expect(consoleSource).toContain('SamplesPerPixel');
    expect(consoleSource).toContain('MaxBounces');
    expect(consoleSource).toContain('MaxRoughness');
    expect(consoleSource).toContain('Radius');

    const boundedReader = consoleSource.slice(
      consoleSource.indexOf('bool ReadBoundedNumberSetting'),
      consoleSource.indexOf('void ApplyNumberSetting'),
    );
    expect(boundedReader).toContain('!Settings->TryGetNumberField(Field, Value)');
    expect(boundedReader).toContain('!FMath::IsFinite(Value)');
    expect(boundedReader).toContain('Value < MinValue || Value > MaxValue');
    expect(boundedReader).toContain('Value != FMath::FloorToDouble(Value)');
    expect(consoleSource).not.toContain('void AddNumberSetting');

    expect(renderTargetSource).toContain('MaxAllocationBytes = 512ll * 1024ll * 1024ll');
    expect(renderTargetSource).toContain('WidthValue > 8192.0');
    expect(renderTargetSource).not.toContain('Width > 16384');
  });

});
