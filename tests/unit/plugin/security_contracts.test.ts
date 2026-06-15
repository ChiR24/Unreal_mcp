import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';
import { z } from 'zod';

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

  it('keeps native MCP fallback identity aligned with the bridge release', () => {
    const transportHeader = privateSource(
      'MCP',
      'Transport',
      'McpNativeTransport.h',
    );
    const pluginDescriptor = readFileSync(
      resolve(
        process.cwd(),
        'plugins/McpAutomationBridge/McpAutomationBridge.uplugin',
      ),
      'utf8',
    );
    const versionName = z
      .object({ VersionName: z.string() })
      .parse(JSON.parse(pluginDescriptor)).VersionName;

    expect(transportHeader).toContain(
      `FString ServerVersion = TEXT("${versionName}")`,
    );
  });

});
