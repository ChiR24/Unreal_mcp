/**
 * Focused tests for the build_environment capability-record builder.
 *
 * Proves the fixed-parent metadata stamp and the invalid lookup contract,
 * without re-asserting shared boilerplate covered elsewhere.
 */
import { describe, expect, it } from 'vitest';
import { type CapabilityRecordSource } from '../../index.js';
import { getParentToolMetadata } from '../parent-metadata.js';
import { buildRecord, type RecordSpec } from './helpers.js';

const SPEC: RecordSpec = {
  id: 'build_environment.create_landscape',
  action: 'create_landscape',
  family: 'landscape',
  summary: 'Create a landscape actor.',
  whenToUse: ['Scaffold terrain for a level.'],
  whenNotToUse: ['When terrain already exists.'],
  inputProps: { size: { type: 'number', description: 'Landscape size.' } },
  required: ['action'],
  effect: 'write',
  latency: 'interactive',
  resources: 'medium',
  exampleInput: { action: 'create_landscape', size: 1024 },
  exampleOutput: { success: true, message: 'Landscape created.' },
};

describe('build_environment record builder — parent metadata', () => {
  const record: CapabilityRecordSource = buildRecord(SPEC);

  it('stamps canonical build_environment parent metadata', () => {
    expect(record.parent).toEqual(getParentToolMetadata('build_environment'));
    expect(record.parent.parent).toBe('build_environment');
    expect(record.routing.parentTool).toBe('build_environment');
  });

  it('keeps only action-scoped discovery; no local parent category/description', () => {
    expect(record.parent.category).toBe('world');
    expect(record.discovery).not.toHaveProperty('category');
    expect(record.discovery).not.toHaveProperty('description');
  });
});

describe('build_environment record builder — invalid lookup', () => {
  it('lookup throws on a non-canonical parent tool', () => {
    expect(() => getParentToolMetadata('not_a_real_tool')).toThrow();
  });
});
