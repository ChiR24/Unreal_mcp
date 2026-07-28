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

  it('documents the permanent single-`unreal` surface with no MCP_GATEWAY_MODE opt-out', () => {
    expect(readme).not.toContain('MCP_GATEWAY_MODE');
    expect(toolRegistry).not.toContain('config.MCP_GATEWAY_MODE');
    const configSrc = read('src/config.ts');
    expect(configSrc).not.toContain('MCP_GATEWAY_MODE');
    expect(readme).toContain('single **`unreal`** gateway tool');
  });

  it('documents the permanently removed native gateway toggle (single-`unreal` native surface)', () => {
    // Permanent absence: the native gateway toggle is intentionally removed, so
    // the native /mcp surface is the single `unreal` tool like the TS surface.
    expect(settings).not.toContain('bEnableNativeGateway');
    expect(settings).not.toContain('Enable Native Gateway Mode');
    expect(readme).not.toContain('Enable Native Gateway');
    expect(readme).toContain('single **`unreal`** gateway tool');
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

  it('keeps the Unreleased CHANGELOG consistent with the permanent single-`unreal` surface', () => {
    const changelog = read('CHANGELOG.md');
    // Slice off released history: `/^## .*\[\d/m` finds the first semver release
    // heading (e.g. `[0.5.30]`); Unreleased headings have no digit after `[`.
    const releasedIdx = changelog.search(/^## .*\[\d/m);
    const unreleased = releasedIdx >= 0 ? changelog.slice(0, releasedIdx) : changelog;

    expect(unreleased).not.toContain('MCP_GATEWAY_MODE=false');
    expect(unreleased).not.toContain('default-on gateway mode');
    expect(unreleased).not.toContain('tool by default');

    expect(unreleased).toContain('DIRECT_TOOL_CALL_REMOVED');
    expect(unreleased).toMatch(/permanent/i);
  });
});
