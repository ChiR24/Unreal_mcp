// Plan Todo 14 (BB-017, BB-026, BB-029, BB-039, BB-057, BB-066) - every
// advertised action must reach exactly one handler. Three rows were already
// wired at dispatch time (BB-029 audio, BB-057 console, BB-066 remove_variable)
// and are locked here so they cannot silently regress; three were real defects
// (BB-017 native stub, BB-026 top-level gate, BB-039 unregistered bridge tool).
//
// Written after the fixes landed, so non-vacuity is proven by mutation: toggle
// any one fix off and the case naming it fails.
//
// No Unreal engine root exists here, so the native rows are source contracts -
// the same evidence class tests/unit/plugin/*contracts.test.ts uses. Live
// execution of these actions is Todo 39.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { describe, expect, it } from 'vitest';

import { ALL_CAPABILITY_RECORDS } from '../../../src/tools/catalog/capabilities/records/aggregate.js';
import type { CapabilityRecord } from '../../../src/tools/catalog/capabilities/model.js';

const PLUGIN_PRIVATE = join(
  'plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private'
);

const source = (...segments: string[]): string =>
  readFileSync(join(...segments), 'utf8');

/** Resolve by the executable legacy pair, never by a guessed capability id. */
function byLegacy(tool: string, action: string): CapabilityRecord {
  const found = ALL_CAPABILITY_RECORDS.find((entry) =>
    entry.legacyIds.some((legacy) => legacy.tool === tool && legacy.action === action));
  if (!found) throw new Error(`no canonical record advertises ${tool}.${action}`);
  return found;
}

describe('todo14 BB-017: set_material_parameter dispatches instead of refusing', () => {
  const stub = (): string =>
    source(PLUGIN_PRIVATE, 'Domains', 'MaterialAuthoring', 'Parameters',
      'McpAutomationBridge_MaterialAuthoringHandlersSetMaterialParameter.cpp');

  it('no longer answers the advertised action with AMBIGUOUS_ACTION', () => {
    expect(stub()).not.toContain('AMBIGUOUS_ACTION');
  });

  it.each([
    ['HandleSetScalarParameterValue', 'set_scalar_parameter_value'],
    ['HandleSetVectorParameterValue', 'set_vector_parameter_value'],
    ['HandleSetTextureParameterValue', 'set_texture_parameter_value']
  ])('delegates to %s with the sub-action it keys on', (handler, subAction) => {
    const text = stub();
    expect(text).toContain(handler);
    expect(text).toContain(`TEXT("${subAction}")`);
  });

  it('declares parameterType so vector and texture are selectable', () => {
    const properties = byLegacy('manage_asset', 'set_material_parameter').schemas.input.properties;
    expect(Object.hasOwn(properties, 'parameterType')).toBe(true);
  });
});

describe('todo14 BB-026: activate_effect survives the top-level effect gate', () => {
  const effect = (): string =>
    source(PLUGIN_PRIVATE, 'Domains', 'Effect', 'McpAutomationBridge_EffectHandlers.cpp');

  it('exempts the canonical action from the early-exit gate', () => {
    expect(effect()).toContain('!Lower.Equals(TEXT("activate_effect"))');
  });

  it('re-dispatches it onto the internal lifecycle spelling', () => {
    const text = effect();
    expect(text).toContain('Lower.Equals(TEXT("activate_effect"))');
    expect(text).toContain('TEXT("activate_niagara")');
  });

  it('is advertised as a canonical manage_effect action', () => {
    expect(byLegacy('manage_effect', 'activate_effect').routing.parentTool).toBe('manage_effect');
  });
});

describe('todo14 BB-039: sequence set_metadata reaches a registered handler', () => {
  it('routes the prefixed sub-action in the sequence dispatch table', () => {
    const dispatch = source(PLUGIN_PRIVATE, 'Domains', 'Sequence',
      'McpAutomationBridge_SequenceHandlers.cpp');
    expect(dispatch).toContain('TEXT("sequence_set_metadata")');
    expect(dispatch).toContain('HandleSequenceSetMetadata');
  });

  it('defines the handler and declares it on the subsystem', () => {
    const impl = source(PLUGIN_PRIVATE, 'Domains', 'Sequence', 'Metadata',
      'McpAutomationBridge_SequenceHandlersSetMetadata.cpp');
    expect(impl).toContain('UMcpAutomationBridgeSubsystem::HandleSequenceSetMetadata');
    expect(impl).toContain('SetMetadataTag');

    const declarations = source('plugins', 'McpAutomationBridge', 'Source',
      'McpAutomationBridge', 'Public', 'McpAutomationBridgeSubsystemSequenceDeclarations.h');
    expect(declarations).toContain('MCP_DECLARE_PAYLOAD_HANDLER(HandleSequenceSetMetadata)');
  });

  it('stops dispatching a top-level bridge tool no shard registers', () => {
    const ts = source('src', 'tools', 'handlers', 'sequence', 'sequence-asset-actions.ts');
    expect(ts).not.toContain("executeAutomationRequest(tools, 'set_metadata'");
    expect(ts).toContain("subAction: 'set_metadata'");
  });
});

describe('todo14 BB-029/BB-057/BB-066: the already-wired rows stay wired', () => {
  it.each([
    'create_ambient_sound',
    'create_audio_component',
    'play_sound_2d',
    'play_sound_at_location',
    'push_sound_mix',
    'prime_sound'
  ])('runtime audio action %s belongs to manage_audio, not a level parent', (action) => {
    expect(byLegacy('manage_audio', action).routing.parentTool).toBe('manage_audio');
  });

  it('console_command belongs to system_control', () => {
    expect(byLegacy('system_control', 'console_command').routing.parentTool).toBe('system_control');
  });

  it('native accepts both remove_variable spellings', () => {
    const text = source(PLUGIN_PRIVATE, 'Domains', 'Blueprint', 'Variables',
      'McpAutomationBridge_BlueprintHandlersVariableRemovalRename.cpp');
    expect(text).toContain('TEXT("blueprint_remove_variable")');
    expect(text).toContain('TEXT("remove_variable")');
  });
});
