import { describe, expect, it } from 'vitest';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import {
  privateSource,
  publicSource,
  recordSource,
  sliceObject,
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
    // Given: `settings` is one tool-level key, so its schema must be the union
    // of every key any MRQ handler reads from it. configure_output_settings
    // reads handleFrameCount/zeroPadFrameNumbers via TryGetSettingsInt
    // (MovieRenderOutput.cpp:25-30,163-167); configure_anti_aliasing reads the
    // remaining four via TryGetIntEither/TryGetStringEither, which accept
    // either nested or top-level (MovieRenderSettings.cpp:28-46,98-102,125-127).
    // Declaring two competing shapes for one key degrades the generated native
    // schema to AnyValue and silently drops the nested properties.
    const records = recordSource('helpers.ts');

    // When
    const settingsSchema = sliceObject(records, 'mrqSettings: {');

    // Then
    for (const field of [
      'handleFrameCount',
      'zeroPadFrameNumbers',
      'spatialSampleCount',
      'temporalSampleCount',
      'antiAliasingMethod',
      'method',
    ]) {
      expect(settingsSchema).toContain(`${field}:`);
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
