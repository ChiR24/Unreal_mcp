import { readdirSync, readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const privateSource = (...parts: string[]): string =>
  readFileSync(
    resolve(
      process.cwd(),
      'plugins/EngineWire/Source/EngineWire/Private',
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
      'McpAssetImportDuplicate.cpp',
    );

    expect(source).toMatch(/McpResolveProjectFilePath\(\s*SourcePath/u);
    expect(source.search(/McpResolveProjectFilePath\(\s*SourcePath/u)).toBeLessThan(
      source.indexOf('FPaths::FileExists(ResolvedSourcePath)'),
    );
  });

  // A link made after the resolver's check carried a plain path write outside the project; the checked writer opens
  // the file first and refuses one that is not where the path says.
  it('writes a file the caller places through the checked writer', () => {
    const writers = [
      ['Domains', 'AssetWorkflow', 'Analysis', 'McpAssetReports.cpp'],
      ['Domains', 'AssetWorkflow', 'Operations', 'McpAssetThumbnails.cpp'],
      ['Domains', 'Environment', 'McpEnvBuildSnapshots.cpp'],
      ['Domains', 'Environment', 'Landscape', 'McpEnvLandscapeHeightmap.cpp'],
      ['Domains', 'Environment', 'Inspection', 'McpEnvInspectActorQuery.cpp'],
      ['Domains', 'Environment', 'Inspection', 'McpEnvInspectCapturePasses.cpp'],
      ['Domains', 'ControlEditor', 'McpEditorScreenshotSupport.cpp'],
      ['Domains', 'Ui', 'McpUiScreenshot.cpp'],
    ];
    for (const parts of writers) {
      const source = privateSource(...parts);
      expect(source, parts.join('/')).toMatch(/McpWriteProject(File|Text)\(/u);
      expect(source, parts.join('/')).not.toMatch(/FFileHelper::Save(String|Array)ToFile/u);
    }
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
    expect(nativeConn).toContain('McpConstantTimeTokenEquals(HttpReq.CapabilityToken, EffectiveToken)');
    // The effective token is resolved from the store (auto-generated + file-backed).
    expect(nativeConn).toContain('McpCapabilityTokenStore::ResolveEffectiveToken');

    // WebSocket bridge_hello uses it for the capabilityToken check.
    const wsMessages = privateSource(
      'Transport',
      'Connection',
      'McpConnectionManagerMessages.cpp',
    );
    expect(wsMessages).toContain('McpConstantTimeTokenEquals(ReceivedToken, CapabilityToken)');
  });

  it('stores the capability token through a shared header-only store', () => {
    const store = privateSource(
      'Foundation',
      'BridgeHelpers',
      'Security',
      'McpCapToken.h',
    );
    // The store exists and exposes the shared resolution function.
    expect(store).toContain('ResolveEffectiveToken');
    // Token path: <ProjectRoot>/Saved/MCP/capability-token
    expect(store).toContain('TEXT("MCP")');
    expect(store).toContain('TEXT("capability-token")');
    // 32 random bytes → 64 lowercase hex chars, no trailing newline.
    expect(store).toMatch(/TEXT\("%02x"\)/); // hex encoding via TEXT("%02x")
    // SECRET HYGIENE: token value is never logged; file path may appear on error.
    expect(store).not.toMatch(/\*Token\b.*UE_LOG|UE_LOG.*\bToken\b/);
  });

  it('refuses every non-loopback native startup: it has no TLS (fail-closed)', () => {
    const lifecycle = privateSource(
      'MCP',
      'Transport',
      'McpNativeTransportLifecycle.cpp',
    );
    // An allowed non-loopback host refuses to start (return false) whatever the token setting: the token would cross
    // the network in clear text, so remote clients come in through a tunnel to loopback.
    const refusal = lifecycle.indexOf('SECURITY: refusing to bind native MCP to non-loopback');
    expect(refusal).toBeGreaterThan(-1);
    expect(lifecycle.slice(lifecycle.lastIndexOf('else if (!bIsLoopback)', refusal), refusal)).not.toContain('bRequireCapabilityToken');
    expect(lifecycle.slice(refusal, lifecycle.indexOf('McpSetNativeListenerLoopback', refusal))).toMatch(/return false;\s*\}\s*$/u);
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

  it('accepts in-project absolute file paths but still enforces containment', () => {
    // The resolver lives in the header the sanitizer header includes at its end.
    const source = privateSource(
      'Foundation',
      'BridgeHelpers',
      'Security',
      'McpProjectPathsResolve.h',
    );
    const resolver = source.slice(
      source.indexOf('bool McpResolveProjectFilePath'),
    );

    // An absolute path is no longer rejected up front...
    expect(resolver).not.toMatch(
      /ProjectRelativePath\.IsEmpty\(\)\s*\|\|\s*!FPaths::IsRelative/u,
    );
    expect(resolver).toContain('const bool bWasAbsolute');

    // ...but it must still be collapsed and then containment-checked. The
    // containment call is the security boundary, so it has to come after the
    // absolute branch and before any success return.
    const absoluteBranch = resolver.indexOf('if (bWasAbsolute)');
    const traversalGuard = resolver.indexOf('CollapseRelativeDirectories');
    const containment = resolver.indexOf('McpValidateProjectSnapshotFilePath(');
    const successReturn = resolver.indexOf('OutAbsolutePath = MoveTemp');

    expect(absoluteBranch).toBeGreaterThan(-1);
    expect(traversalGuard).toBeGreaterThan(absoluteBranch);
    expect(containment).toBeGreaterThan(traversalGuard);
    expect(successReturn).toBeGreaterThan(containment);
  });

  it('bounds render workload controls before applying CVars', () => {
    const consoleSource = privateSource(
      'Domains',
      'Render',
      'McpRenderConsole.cpp',
    );
    const renderTargetSource = privateSource(
      'Domains',
      'Render',
      'McpRenderTargets.cpp',
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

  // One pass over every plugin source file: 0.1 s alone, but over the 10 s default on a loaded
  // machine (the suite's other whole-tree tests carry 60 s for the same reason).
  it('never calls UPackage::SavePackage outside the Safety wrappers', () => {
    const root = resolve(process.cwd(), 'plugins/EngineWire/Source');
    const offenders = readdirSync(root, { recursive: true, encoding: 'utf8' })
      .filter((file) => file.endsWith('.cpp') || file.endsWith('.h'))
      .filter((file) => readFileSync(resolve(root, file), 'utf8').includes('UPackage::SavePackage'));
    expect(offenders).toEqual([]);
  }, 60_000);
});
