import { describe, expect, it } from 'vitest';

import { EXECUTION_OPTION_KEYS } from '../../../src/tools/catalog/capabilities/semantic/execution-options.js';
import { validateExecutionOptions } from '../../../src/server/gateway/gateway-execute-validate.js';

// Task 42 parity mirror of the native
// McpAutomationBridge.Foundation.LiveStateRevisions.ExpectedRevisionsParsing
// automation test. Both surfaces must accept the same pin envelope and refuse
// the same malformed input with the same error code, otherwise a client gets a
// different answer depending on which transport it happens to be talking to.
describe('task 42 expectedRevisions execution option', () => {
  it('is an accepted execution option key', () => {
    expect(EXECUTION_OPTION_KEYS).toContain('expectedRevisions');
  });

  it('accepts every live-state key with a positive integer revision', () => {
    const violation = validateExecutionOptions({
      expectedRevisions: { selection: 3, level: 7, assetRegistry: 11, package: 13 }
    });

    expect(violation).toBeUndefined();
  });

  it('accepts a partial pin set, because an absent key is simply not pinned', () => {
    expect(validateExecutionOptions({ expectedRevisions: { selection: 1 } })).toBeUndefined();
    expect(validateExecutionOptions({ expectedRevisions: {} })).toBeUndefined();
    expect(validateExecutionOptions({})).toBeUndefined();
  });

  it('refuses an unknown pin name as an unsupported option', () => {
    const violation = validateExecutionOptions({ expectedRevisions: { selektion: 2 } });

    expect(violation?.errorCode).toBe('UNSUPPORTED_OPTION');
    expect(violation?.message).toContain('selection');
  });

  it('refuses a non-object envelope with a pointer at the option', () => {
    const violation = validateExecutionOptions({ expectedRevisions: 'selection=2' });

    expect(violation?.errorCode).toBe('INVALID_OPTIONS');
    expect(violation?.pointer).toBe('/options/expectedRevisions');
  });

  it.each([
    ['zero', 0],
    ['negative', -4],
    ['fractional', 2.5],
    ['non-numeric', '2']
  ])('refuses a %s revision as out of range', (_label, value) => {
    const violation = validateExecutionOptions({ expectedRevisions: { selection: value } });

    expect(violation?.errorCode).toBe('OUT_OF_RANGE');
    expect(violation?.option).toBe('expectedRevisions.selection');
  });
});
