import { describe, expect, it } from 'vitest';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import {
  privateSource,
  publicSource,
} from './sequence_contract_test_utils.js';

describe('MRQ frame accounting and native schema parity', () => {
  it('accounts for render frames at the effective output rate', () => {
    // Given
    const queueLimits = privateSource(
      'Domains',
      'Sequence',
      'MovieRender',
      'McpAutomationBridge_SequenceMovieRenderQueueLimits.cpp',
    );
    const settings = publicSource('McpAutomationBridgeSettings.h');
    const output = privateSource(
      'Domains',
      'Sequence',
      'MovieRender',
      'McpAutomationBridge_SequenceMovieRenderOutput.cpp',
    );

    // When
    const accountingSource = queueLimits;

    // Then
    expect(accountingSource).toContain('GetTickResolution()');
    expect(accountingSource).toContain('GetDisplayRate()');
    expect(accountingSource).toContain('GetEffectiveFrameRate');
    expect(accountingSource).toContain('FFrameRate::TransformTime');
    expect(accountingSource).toContain('HandleFrameCount');
    expect(accountingSource).toContain(
      'constexpr int32 MaximumEffectiveFrameRate = 240',
    );
    expect(accountingSource).toContain('FMath::Clamp(');
    expect(settings).toContain('MaxMovieRenderEffectiveFrameRate');
    expect(settings).toContain('ClampMax = "240"');
    expect(output).toContain('ValidateJobResourceLimits');
  });

  it('keeps cancellation settling inside the TypeScript transport grace', () => {
    // Given
    const completion = privateSource(
      'Domains',
      'Sequence',
      'MovieRender',
      'McpAutomationBridge_SequenceMovieRenderCompletion.cpp',
    );

    // When
    const cancellationSource = completion;

    // Then
    expect(cancellationSource).toContain('MovieRenderTransportGraceMs = 35000');
    expect(cancellationSource).toContain('MovieRenderResponseBudgetMs = 5000');
    expect(cancellationSource).toContain('FMath::Clamp');
  });

  it('declares every nested MRQ setting accepted by handlers', () => {
    // Given
    const schema = privateSource(
      'MCP',
      'Tools',
      'Utility',
      'McpTool_ManageSequenceSchemaFields.cpp',
    );
    const settingsStart = schema.indexOf('.Object(TEXT("settings")');
    const settingsEnd = schema.indexOf(
      '.FreeformObject(TEXT("platformSources")',
      settingsStart,
    );

    // When
    const settingsSchema = schema.slice(settingsStart, settingsEnd);

    // Then
    for (const field of [
      'handleFrameCount',
      'zeroPadFrameNumbers',
      'spatialSampleCount',
      'temporalSampleCount',
      'antiAliasingMethod',
      'method',
    ]) {
      expect(settingsSchema).toContain(`TEXT("${field}")`);
    }
  });

  it('compares nested schema presence instead of treating freeform as parity', () => {
    const contract = readFileSync(
      resolve(process.cwd(), 'tests', 'audits', 'schema-contract.mjs'),
      'utf8',
    );

    expect(contract).toContain('`${path}.properties.present`');
    expect(contract).toContain('Boolean(typeScriptProperties)');
    expect(contract).toContain('Boolean(nativeProperties)');
  });
});
