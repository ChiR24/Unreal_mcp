/**
 * Focused tests for the generic core-only capability-record builder.
 *
 * Proves: parent/action routing, schema closure, output envelope,
 * behavior, policy, normalization, examples, and branded parsing. Does not
 * touch frozen pilot builders.
 */
import { describe, expect, it } from 'vitest';
import { type CapabilityRecordSource } from '../../model.js';
import { createCapabilityRecord } from '../../parser.js';
import { buildCoreRecord, type CoreRecordSpec } from './builder.js';

const SPEC: CoreRecordSpec = {
  parentTool: 'control_actor',
  action: 'spawn_actor',
  dispatchAction: 'spawn_actor',
  domain: 'actor',
  family: 'spawn',
  summary: 'Spawn an actor into the current level.',
  whenToUse: ['Create a new actor programmatically.'],
  whenNotToUse: ['When the actor already exists.'],
  inputProps: { name: { type: 'string', description: 'Actor name.' } },
  required: ['action'],
  effect: 'write',
  behavior: { idempotency: 'non-idempotent' },
  costLatency: 'interactive',
  costResources: 'low',
  exampleInput: { action: 'spawn_actor', name: 'Cube' },
  exampleOutput: { success: true, message: 'Spawned Cube.' },
  aliases: ['actor.spawn'],
};

describe('core generic record builder', () => {
  const record: CapabilityRecordSource = buildCoreRecord(SPEC);

  it('routes to the declared parent tool and dispatch action', () => {
    expect(record.routing.parentTool).toBe('control_actor');
    expect(record.routing.dispatchAction).toBe('spawn_actor');
  });

  it('closes the input schema with action and required keys only', () => {
    const props = record.schemas.input.properties;
    expect(Object.keys(props)).toEqual(['action', 'name']);
    expect(record.schemas.input.required).toEqual(['action']);
    expect(record.schemas.input.additionalProperties).toBe(false);
  });

  it('defaults the output envelope to success/message', () => {
    const out = record.schemas.output.properties;
    expect(Object.keys(out)).toEqual(['success', 'message', 'details']);
    expect(record.schemas.output.required).toEqual(['success']);
  });

  it('derives behavior from the declared effect and overrides', () => {
    expect(record.behavior.effect).toBe('write');
    expect(record.behavior.idempotency).toBe('non-idempotent');
    expect(record.behavior.longRunning).toBe(false);
  });

  it('derives policy from the declared effect', () => {
    expect(record.policy.requiredScope).toBe('write');
    expect(record.policy.consent).toBe('none');
    expect(record.policy.dataAccess).toBe('project-write');
  });

  it('brands the canonical id and aliases via the shared parsers', () => {
    expect(record.id).toBe('control_actor.spawn_actor');
    expect(record.aliases).toEqual(['actor.spawn']);
    expect(record.legacyIds).toEqual([{ tool: 'control_actor', action: 'spawn_actor' }]);
  });

  it('captures the example envelope and discovery', () => {
    expect(record.examples).toHaveLength(1);
    expect(record.examples[0].title).toBe(SPEC.summary);
    expect(record.examples[0].input).toEqual(SPEC.exampleInput);
    expect(record.examples[0].output).toEqual(SPEC.exampleOutput);
    expect(record.discovery.domain).toBe('actor');
    expect(record.discovery.family).toBe('spawn');
    expect(record.discovery.topics).toEqual(['spawn_actor']);
  });

  it('produces a fully valid CapabilityRecord with verifiable hashes', () => {
    const full = createCapabilityRecord(record);
    expect(full.hashes.algorithm).toBe('sha256');
    expect(full.hashes.schema).toMatch(/^[0-9a-f]{64}$/);
    expect(full.hashes.content).toMatch(/^[0-9a-f]{64}$/);
  });

  it('merges caller output props into the envelope when supplied', () => {
    const withOutput = buildCoreRecord({
      ...SPEC,
      outputProps: { actorPath: { type: 'string', description: 'Spawned actor path.' } },
      outputRequired: ['actorPath'],
    });
    const props = Object.keys(withOutput.schemas.output.properties);
    expect(props).toEqual(['success', 'message', 'details', 'actorPath']);
    expect(withOutput.schemas.output.required).toEqual(['success', 'actorPath']);
  });

});

