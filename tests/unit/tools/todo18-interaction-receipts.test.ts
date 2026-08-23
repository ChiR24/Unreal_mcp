// Plan Todo 18 (BB-007, BB-008, BB-009) - interaction validation, reads, and
// receipts must be truthful.
//
// Written after the fixes landed, so non-vacuity is proven by mutation: toggle
// any one fix off and the case naming it fails. The native cases are
// source-contract reads because no engine root exists here to compile the
// plugin.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { describe, expect, it } from 'vitest';

import { ALL_CAPABILITY_RECORDS } from '../../../src/tools/catalog/capabilities/records/aggregate.js';
import type { CapabilityRecord } from '../../../src/tools/catalog/capabilities/model.js';

const PRIVATE = join(
  'plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private'
);

function nativeSource(...segments: readonly string[]): string {
  return readFileSync(join(PRIVATE, ...segments), 'utf8');
}

/** Blank comments so no assertion can be satisfied by prose. */
function code(source: string): string {
  return source
    .replace(/\/\*[\s\S]*?\*\//gu, ' ')
    .replace(/\/\/[^\n]*/gu, ' ');
}

function record(id: string): CapabilityRecord {
  const found = ALL_CAPABILITY_RECORDS.find((entry) => String(entry.id) === id);
  if (!found) throw new Error(`fixture record '${id}' is absent from the canonical source`);
  return found;
}

function outputKeys(id: string): readonly string[] {
  return Object.keys(JSON.parse(JSON.stringify(record(id).schemas.output.properties)) as object);
}

describe('todo18 BB-007: get_interaction_info declares what the native reader emits', () => {
  const READ_ID = 'manage_interaction.get_interaction_info';
  const EXPECTED_FIELDS = [
    'assetPath', 'assetType', 'blueprintName', 'blueprintPath',
    'actorName', 'actorClass', 'doorPath', 'switchPath',
    'chestPath', 'triggerPath'
  ] as const;

  it('declares every field HandleInteractionInfoAction emits', () => {
    const declared = outputKeys(READ_ID);
    for (const field of EXPECTED_FIELDS) {
      expect(declared, `${READ_ID} must declare ${field}`).toContain(field);
    }
  });

  it('keeps assetPath in the declared set (the identity handle)', () => {
    expect(outputKeys(READ_ID)).toContain('assetPath');
  });

  it('native reader still emits the canonical assetPath', () => {
    const source = code(
      nativeSource('Domains', 'Interaction', 'McpAutomationBridge_InteractionHandlersInfo.cpp')
    );

    expect(source).toContain('SetStringField(TEXT("assetPath"), ResolvedPath)');
  });
});

describe('todo18 BB-008: every mutation receipt carries canonical identity', () => {
  // Every asset-mutating Interaction file that Todo 16 wired through
  // AddMutationEvidence. Each must still route through the shared helper.
  const FILES = [
    'Chest', 'Components', 'Destruction', 'Door', 'Interface',
    'Lever', 'Switch', 'Triggers', 'WidgetEvents'
  ] as const;

  // Existence regexes proved too weak here: an oracle probe deleted one of
  // Chest's TWO call sites and the suite stayed green. Pin exact counts.
  const EXPECTED_CALLS: Record<(typeof FILES)[number], number> = {
    Chest: 2, Components: 2, Destruction: 1, Door: 2, Interface: 1,
    Lever: 1, Switch: 2, Triggers: 2, WidgetEvents: 2,
  };

  it.each(FILES)('%s routes through AddMutationEvidence at every mutating handler', (name) => {
    const source = code(
      nativeSource('Domains', 'Interaction', `McpAutomationBridge_InteractionHandlers${name}.cpp`)
    );

    expect(source).toContain('McpAutomationBridgeHelpersMutationEvidence.h');
    const calls = source.match(/AddMutationEvidence\(/gu) ?? [];
    expect(calls, `${name} must keep all ${EXPECTED_CALLS[name]} evidence calls`).toHaveLength(
      EXPECTED_CALLS[name]
    );
  });

  it('create_door_actor keeps its pre-existing verification call', () => {
    expect(nativeSource('Domains', 'Interaction', 'McpAutomationBridge_InteractionHandlersDoor.cpp'))
      .toContain('McpHandlerUtils::AddVerification(Result, DoorBP)');
  });
});

describe('todo18 BB-009a: duplicate create_interactable_interface refuses before any mutation', () => {
  const iface = (): string =>
    code(nativeSource('Domains', 'Interaction', 'McpAutomationBridge_InteractionHandlersInterface.cpp'));

  it('checks DoesAssetExist BEFORE CreatePackage', () => {
    const source = iface();
    const doesExist = source.indexOf('DoesAssetExist');
    const createPackage = source.indexOf('CreatePackage');

    expect(doesExist).toBeGreaterThan(-1);
    expect(createPackage).toBeGreaterThan(-1);
    expect(doesExist, 'the existence check must precede CreatePackage').toBeLessThan(createPackage);
  });

  it('refuses with typed ALREADY_EXISTS naming the colliding path', () => {
    const source = iface();

    expect(source).toContain('ALREADY_EXISTS');
    expect(source).toContain('already exists at');
  });

  it('does NOT call FactoryCreateNew in the refusal path', () => {
    const source = iface();
    const refusalIdx = source.indexOf('ALREADY_EXISTS');
    const factoryIdx = source.indexOf('FactoryCreateNew');

    expect(refusalIdx).toBeGreaterThan(-1);
    expect(factoryIdx).toBeGreaterThan(-1);
    // The factory must come AFTER the refusal check, not inside it.
    expect(refusalIdx).toBeLessThan(factoryIdx);
  });
});

describe('todo18 BB-009b: configure_door_properties validates target class before mutating', () => {
  const door = (): string =>
    code(nativeSource('Domains', 'Interaction', 'McpAutomationBridge_InteractionHandlersDoor.cpp'));

  it('scans SCS nodes for DoorPivot and DoorMesh before mutating', () => {
    const source = door();

    expect(source).toContain('bHasDoorPivot');
    expect(source).toContain('bHasDoorMesh');
    expect(source).toContain('GetVariableName()');
  });

  it('refuses with INVALID_OBJECT_TYPE when neither node is present', () => {
    const source = door();

    expect(source).toContain('INVALID_OBJECT_TYPE');
    expect(source).toContain('is not a door blueprint');
  });

  it('does NOT mutate a blueprint that lacks both discriminating nodes', () => {
    const source = door();
    const gateIdx = source.indexOf('!bHasDoorPivot || !bHasDoorMesh');
    const addVarIdx = source.indexOf('AddBlueprintVariableIfMissing');

    expect(gateIdx).toBeGreaterThan(-1);
    expect(addVarIdx).toBeGreaterThan(-1);
    // The class check must precede the variable-authoring mutation.
    expect(gateIdx).toBeLessThan(addVarIdx);
  });
});
