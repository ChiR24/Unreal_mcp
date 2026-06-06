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
    const source = privateSource('McpConnectionManagerResponses.cpp');
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
      'McpAutomationBridge_AssetWorkflowImportDuplicate.cpp',
    );

    expect(source).toMatch(/McpResolveProjectFilePath\(\s*SourcePath/u);
    expect(source.search(/McpResolveProjectFilePath\(\s*SourcePath/u)).toBeLessThan(
      source.indexOf('FPaths::FileExists(ResolvedSourcePath)'),
    );
  });
});
