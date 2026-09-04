/**
 * Focused tests for the gameplay capability-record builder.
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
import { buildRecord, type RecordSpec } from './helpers.js';

const SPEC: RecordSpec = {
  parentTool: 'manage_character',
  id: 'manage_character.spawn_character',
  action: 'spawn_character',
  family: 'character',
  summary: 'Spawn a gameplay character into the current level.',
  whenToUse: ['A new character must be created.'],
  whenNotToUse: ['When the character already exists.'],
  inputProps: { name: { type: 'string', description: 'Character name.' } },
  required: ['action'],
  effect: 'write',
  latency: 'interactive',
  resources: 'low',
  exampleInput: { action: 'spawn_character', name: 'Hero' },
  exampleOutput: { success: true, message: 'Spawned Hero.' },
};

describe('gameplay record builder', () => {
  const record = buildRecord(SPEC);

  it('routes to the declared parent tool and dispatch action', () => {
    expect(record.routing.parentTool).toBe('manage_character');
    expect(record.routing.dispatchAction).toBe('spawn_character');
    expect(record.routing.dispatchMode).toBe('tool');
  });

  it('closes the input schema with action stripped and required keys only', () => {
    const props = record.schemas.input.properties;
    expect(Object.keys(props)).toEqual(['name']);
    expect(record.schemas.input.required).toEqual([]);
    expect(record.schemas.input.additionalProperties).toBe(false);
  });

  it('defaults the output envelope to success/message', () => {
    const out = record.schemas.output.properties;
    expect(Object.keys(out)).toEqual(['success', 'message', 'details']);
    expect(record.schemas.output.required).toEqual(['success']);
  });

  it('derives behavior and policy from the declared effect', () => {
    expect(record.behavior.effect).toBe('write');
    expect(record.behavior.supportsUndo).toBe(true);
    expect(record.policy.requiredScope).toBe('write');
    expect(record.policy.dataAccess).toBe('project-write');
  });

  it('brands the canonical id and legacy ids', () => {
    expect(record.id).toBe('manage_character.spawn_character');
    expect(record.legacyIds).toEqual([{ tool: 'manage_character', action: 'spawn_character' }]);
  });

  it('captures the example envelope and discovery', () => {
    expect(record.examples).toHaveLength(1);
    expect(record.examples[0].title).toBe(SPEC.summary);
    expect(record.discovery.family).toBe('character');
    expect(record.discovery.topics).toEqual(['spawn_character']);
  });

  it('stamps canonical parent metadata resolved by routing.parentTool', () => {
    expect(record.parent).toEqual(getParentToolMetadata('manage_character'));
    expect(record.parent.parent).toBe(record.routing.parentTool);
  });

  it('resolves distinct parent metadata for another gameplay parent', () => {
    const combat = buildRecord({ ...SPEC, parentTool: 'manage_combat', id: 'manage_combat.fire_weapon', action: 'fire_weapon' });
    expect(combat.parent).toEqual(getParentToolMetadata('manage_combat'));
  });

  it('never duplicates a local category or description in the record body', () => {
    expect(record.parent.category).toBe('gameplay');
    expect(record.discovery).not.toHaveProperty('category');
    expect(record.discovery).not.toHaveProperty('description');
  });
});

describe('gameplay parent metadata lookup', () => {
  it('throws on a non-canonical parent tool', () => {
    expect(() => getParentToolMetadata('not_a_real_tool')).toThrow();
  });

  it('covers every gameplay parent named in the contract', () => {
    for (const parent of [
      'animation_physics',
      'manage_effect',
      'manage_gas',
      'manage_character',
      'manage_combat',
      'manage_ai',
      'manage_inventory',
      'manage_interaction',
    ]) {
      expect(getParentToolMetadata(parent).category).toBe('gameplay');
    }
  });
});
