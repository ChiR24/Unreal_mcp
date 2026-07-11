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
    expect(toolRegistry).toContain('MCP_GATEWAY_MODE');
    expect(toolRegistry).toContain("val !== 'false'");
    expect(toolRegistry).toContain("val !== '0'");
    expect(toolRegistry).toContain("val !== 'no'");
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
});
