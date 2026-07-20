import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const read = (rel: string): string =>
  readFileSync(resolve(process.cwd(), rel), 'utf8');

// Doc-contract: the migration instructions in README.md must use the exact
// env var / setting names that exist in source. If a source name changes,
// either the doc or this test must be updated deliberately.
describe('gateway migration doc contract', () => {
  const readme = read('README.md');
  const toolRegistry = read('src/server/tool-registry.ts');
  const settings = read(
    'plugins/McpAutomationBridge/Source/McpAutomationBridge/Public/McpAutomationBridgeSettings.h',
  );

  it('documents the TypeScript MCP_GATEWAY_MODE opt-out (false/0/no)', () => {
    expect(readme).toContain('MCP_GATEWAY_MODE=false');
    // Gateway mode reads the validated config flag, not raw process.env in the registry
    // (src/server/AGENTS.md: raw process.env access is not allowed for this flag).
    expect(toolRegistry).toContain('config.MCP_GATEWAY_MODE');
    // The false/0/no aliases are normalized by the config schema via stringToBoolean.
    const configSrc = read('src/config.ts');
    expect(configSrc).toContain('MCP_GATEWAY_MODE');
    expect(configSrc).toContain('stringToBoolean');
  });

  it('documents the native Enable Native Gateway setting', () => {
    expect(readme).toContain('Enable Native Gateway');
    expect(settings).toContain('bEnableNativeGateway');
    expect(settings).toContain('Enable Native Gateway Mode');
  });

  it('distinguishes the stdio and native transports in the docs', () => {
    expect(readme).toContain('TypeScript stdio');
    expect(readme).toContain('native MCP');
  });

  it('documents the intentional native/TS protocol version asymmetry', () => {
    const protocol = read('docs/protocol.md');
    // Native supports exactly the three modern versions.
    for (const v of ['2025-11-25', '2025-06-18', '2025-03-26']) {
      expect(protocol).toContain(v);
    }
    // TypeScript also accepts the two older legacy versions.
    expect(protocol).toContain('2024-11-05');
    expect(protocol).toContain('2024-10-07');
    // The asymmetry is framed: native is intentionally stricter than TS.
    expect(protocol).toContain('intentionally stricter');
    // README must not imply both surfaces share an identical version set.
    expect(readme).toContain('2025-11-25');
    expect(readme).toContain('2024-11-05');
    expect(readme).toContain('2024-10-07');
  });

  it('documents every coordinated version source', () => {
    const protocol = read('docs/protocol.md');
    for (const source of [
      'package.json',
      'package-lock.json',
      'server.json',
      'McpAutomationBridge.uplugin',
      'server-info.json',
      'server-factory.ts',
      'McpNativeTransport.h',
    ]) {
      expect(protocol, `docs omit version source: ${source}`).toContain(source);
    }
    expect(protocol).toContain('version:check');
  });
});
