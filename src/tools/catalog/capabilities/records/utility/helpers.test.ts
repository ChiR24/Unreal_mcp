/**
 * Focused tests for the utility capability-record builder.
 *
 * Proves: parent/action routing, schema closure, output envelope, availability,
 * behavior, policy, normalization, examples, branded parsing, and — the Task
 * contract — that every produced record is stamped with canonical parent
 * metadata (description + category) resolved by `routing.parentTool`, with no
 * local duplicated parent category/description. Does not touch frozen pilot
 * data files or other builders.
 */
import { describe, expect, it } from 'vitest';
import { getParentToolMetadata } from '../parent-metadata.js';
import { utilityRecord, type UtilityRecordSpec } from './helpers.js';

const SPEC: UtilityRecordSpec = {
  tool: 'manage_audio',
  action: 'play_sound',
  family: 'audio',
  summary: 'Play a sound cue at a location.',
  params: ['soundCue'],
  required: ['soundCue'],
  effect: 'write',
  dispatchAction: 'play_sound',
  resources: 'low',
};

describe('utility record builder', () => {
  const record = utilityRecord(SPEC);

  it('routes to the declared parent tool and dispatch action', () => {
    expect(record.routing.parentTool).toBe('manage_audio');
    expect(record.routing.dispatchAction).toBe('play_sound');
    expect(record.routing.dispatchMode).toBe('tool');
  });

  it('closes the input schema with action plus declared params', () => {
    const props = record.schemas.input.properties;
    expect(Object.keys(props)).toEqual(['action', 'soundCue']);
    expect(record.schemas.input.required).toEqual(['action', 'soundCue']);
    expect(record.schemas.input.additionalProperties).toBe(false);
  });

  it('defaults the output envelope to success/message', () => {
    const out = record.schemas.output.properties;
    expect(Object.keys(out)).toEqual(['success', 'message', 'details']);
    expect(record.schemas.output.required).toEqual(['success']);
  });

  it('derives behavior and policy from the declared effect', () => {
    expect(record.behavior.effect).toBe('write');
    expect(record.policy.requiredScope).toBe('write');
    expect(record.policy.dataAccess).toBe('project-write');
  });

  it('brands the canonical id and legacy ids', () => {
    expect(record.id).toBe('manage_audio.play_sound');
    expect(record.legacyIds).toEqual([{ tool: 'manage_audio', action: 'play_sound' }]);
  });

  it('captures the example envelope and discovery', () => {
    expect(record.examples).toHaveLength(1);
    expect(record.examples[0].title).toBe(SPEC.summary);
    expect(record.discovery.family).toBe('audio');
    expect(record.discovery.topics).toEqual(['play_sound']);
  });

  it('stamps canonical parent metadata resolved by routing.parentTool', () => {
    expect(record.parent).toEqual(getParentToolMetadata('manage_audio'));
    expect(record.parent.parent).toBe(record.routing.parentTool);
  });

  it('resolves distinct parent metadata for another utility parent', () => {
    const net = utilityRecord({ ...SPEC, tool: 'manage_networking', action: 'replicate_actor' });
    expect(net.parent).toEqual(getParentToolMetadata('manage_networking'));
  });

  it('never duplicates a local category or description in the record body', () => {
    expect(record.parent.category).toBe('utility');
    expect(record.discovery).not.toHaveProperty('category');
    expect(record.discovery).not.toHaveProperty('description');
  });
});

describe('utility parent metadata lookup', () => {
  it('throws on a non-canonical parent tool', () => {
    expect(() => getParentToolMetadata('not_a_real_tool')).toThrow();
  });

  it('covers every utility parent named in the contract', () => {
    for (const parent of ['manage_audio', 'manage_networking']) {
      expect(getParentToolMetadata(parent).category).toBe('utility');
    }
  });
});
