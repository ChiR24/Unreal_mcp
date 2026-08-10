/**
 * Focused tests for the world-domain capability-record builder.
 *
 * Proves the parent metadata stamp (by routing.parentTool) and the invalid
 * lookup contract, without re-asserting the shared boilerplate already covered
 * by the core builder tests.
 */
import { describe, expect, it } from 'vitest';
import { type CapabilityRecordSource } from '../../index.js';
import { getParentToolMetadata } from '../parent-metadata.js';
import { buildWorldRecord, type WorldRecordSpec } from './builder.js';

const SPEC: WorldRecordSpec = {
  parentTool: 'manage_level_structure',
  action: 'load_level',
  dispatchAction: 'load_level',
  family: 'level',
  summary: 'Load a level by path.',
  whenToUse: ['Bring a sublevel into the editor world.'],
  whenNotToUse: ['When the level is already loaded.'],
  inputProps: { path: { type: 'string', description: 'Level asset path.' } },
  required: ['action'],
  effect: 'write',
  costLatency: 'interactive',
  costResources: 'low',
  normalizationRationale: 'Distinct manage_level_structure target with unique schema.',
  exampleInput: { action: 'load_level', path: '/Game/Maps/Main' },
  exampleOutput: { success: true, message: 'Loaded Main.' },
};

describe('world record builder — parent metadata', () => {
  const record: CapabilityRecordSource = buildWorldRecord(SPEC);

  it('stamps canonical parent metadata resolved by routing.parentTool', () => {
    expect(record.parent).toEqual(getParentToolMetadata('manage_level_structure'));
    expect(record.parent.parent).toBe(record.routing.parentTool);
  });

  it('resolves distinct parent metadata for sibling world parents', () => {
    const pcg = buildWorldRecord({ ...SPEC, parentTool: 'manage_pcg', action: 'run_graph' });
    expect(pcg.parent).toEqual(getParentToolMetadata('manage_pcg'));
  });

  it('keeps only action-scoped discovery; no local parent category/description', () => {
    expect(record.parent.category).toBe('world');
    expect(record.discovery).not.toHaveProperty('category');
    expect(record.discovery).not.toHaveProperty('description');
  });
});

describe('world record builder — invalid lookup', () => {
  it('lookup throws on a non-canonical parent tool', () => {
    expect(() => getParentToolMetadata('not_a_real_tool')).toThrow();
  });
});
