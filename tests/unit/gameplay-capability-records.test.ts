import { describe, expect, it } from 'vitest';
import { z } from 'zod';
import type { CapabilityRecord, CapabilityRecordSource } from '../../src/tools/catalog/capabilities/index.js';
import {
  GAMEPLAY_CAPABILITY_CATALOG,
  GAMEPLAY_CAPABILITY_RECORD_COUNT,
  GAMEPLAY_HIDDEN_ROUTE_DISPOSITIONS,
  GAMEPLAY_PARENT_RECORDS,
  GAMEPLAY_SOURCE_RECORDS,
} from '../../src/tools/catalog/capabilities/records/gameplay/index.js';
import { consolidatedToolDefinitions } from '../../src/tools/catalog/consolidated-tool-definitions.js';
import type { ToolDefinition } from '../../src/tools/definitions/shared/tool-definition.js';

const GAMEPLAY_PARENT_NAMES = [
  'animation_physics',
  'manage_effect',
  'manage_gas',
  'manage_character',
  'manage_combat',
  'manage_ai',
  'manage_inventory',
  'manage_interaction',
] as const;

const ActionDefinitionSchema = z.object({
  properties: z.object({
    action: z.object({ enum: z.array(z.string()) }),
  }),
});
const DEFINITIONS: readonly ToolDefinition[] = GAMEPLAY_PARENT_NAMES.map((name) => {
  const definition = consolidatedToolDefinitions.find((tool) => tool.name === name);
  if (definition === undefined) throw new TypeError(`Missing gameplay parent: ${name}`);
  return definition;
});

function actions(definition: ToolDefinition): readonly string[] {
  return ActionDefinitionSchema.parse(definition.inputSchema).properties.action.enum;
}

function ids(records: readonly { readonly id: string }[]): readonly string[] {
  return records.map((record) => record.id);
}

function recordById(id: string): CapabilityRecord {
  const record = GAMEPLAY_CAPABILITY_CATALOG.find((candidate) => candidate.id === id);
  if (record === undefined) throw new TypeError(`Missing gameplay capability: ${id}`);
  return record;
}

describe('Task 17 exhaustive gameplay records', () => {
  it('matches all eight parent action enums in exact order', () => {
    for (const definition of DEFINITIONS) {
      const records = GAMEPLAY_PARENT_RECORDS[definition.name];
      expect(ids(records)).toEqual(actions(definition).map((action) => `${definition.name}.${action}`));
    }
  });

  it('contains exactly 356 ordered unique source records', () => {
    const expected = DEFINITIONS.flatMap((definition) =>
      actions(definition).map((action) => `${definition.name}.${action}`));
    expect(ids(GAMEPLAY_SOURCE_RECORDS)).toEqual(expected);
    expect(GAMEPLAY_SOURCE_RECORDS).toHaveLength(356);
    expect(new Set(ids(GAMEPLAY_SOURCE_RECORDS)).size).toBe(356);
  });

  it('builds a frozen fail-closed aggregate with stable sorted IDs', () => {
    expect(GAMEPLAY_CAPABILITY_RECORD_COUNT).toBe(356);
    expect(Object.isFrozen(GAMEPLAY_CAPABILITY_CATALOG)).toBe(true);
    expect(ids(GAMEPLAY_CAPABILITY_CATALOG)).toEqual([...ids(GAMEPLAY_CAPABILITY_CATALOG)].sort());
  });

  it('does not repeat the parent action discriminator in per-action input schemas', () => {
    for (const record of GAMEPLAY_SOURCE_RECORDS) {
      expect(record.schemas.input.properties).not.toHaveProperty('action');
      expect(record.schemas.input.required).not.toContain('action');
    }
  });
});

describe('Task 17 hidden route dispositions', () => {
  it('resolves exactly 16 skeleton, 4 GAS, and 3 AI routes', () => {
    expect(GAMEPLAY_HIDDEN_ROUTE_DISPOSITIONS.skeleton).toHaveLength(16);
    expect(GAMEPLAY_HIDDEN_ROUTE_DISPOSITIONS.gas).toHaveLength(4);
    expect(GAMEPLAY_HIDDEN_ROUTE_DISPOSITIONS.ai).toHaveLength(3);
    for (const route of Object.values(GAMEPLAY_HIDDEN_ROUTE_DISPOSITIONS).flat()) {
      expect(['promote', 'map', 'remove']).toContain(route.disposition);
      expect(route.rationale.length).toBeGreaterThan(20);
      expect(route.evidence.source.endsWith('.cpp')).toBe(true);
    }
  });
});

describe('Task 17 honest behavior metadata', () => {
  it('keeps ragdoll asset setup distinct from the now-reachable activation action', () => {
    const setup = recordById('animation_physics.setup_ragdoll');
    const activate = recordById('animation_physics.activate_ragdoll');
    expect(setup.availability.editorStates).toEqual(['pie', 'simulate']);
    expect(setup.discovery.summary).toContain('PhysicsAsset');
    // Task 21 repaired: activate_ragdoll is a distinct, reachable canonical action.
    expect(activate.normalization.disposition).toBe('canonical');
    expect(activate.deprecation.status).toBe('active');
    expect(activate.discovery.summary).not.toContain('unreachable');
    expect(activate.discovery.summary).toContain('distinct native activate_ragdoll');
  });

  it('labels combat damage, heal, shield, and armor operations as Blueprint authoring', () => {
    for (const action of ['apply_damage', 'heal', 'create_shield', 'modify_armor']) {
      const record = recordById(`manage_combat.${action}`);
      expect(record.availability.editorStates).toEqual(['edit']);
      expect(record.discovery.summary).toContain('Blueprint asset');
      expect(record.behavior.effect).toBe('write');
      expect(record.behavior.safeToRetry).toBe(false);
    }
  });

  // set_focus is authoring, not runtime: HandleSetFocus loads the AIController
  // ASSET via LoadObject<UBlueprint>, adds a FocusActor member variable with
  // FBlueprintEditorUtils::AddMemberVariable, then McpSafeAssetSave()s it
  // (Domains/AI/Controllers/McpAutomationBridge_AIHandlersControllerFocus.cpp,
  // dispatched from McpAutomationBridge_AIHandlers.cpp:252). No manage_ai handler
  // calls AAIController::SetFocus or reads GEditor->PlayWorld, so no manage_ai
  // action may claim a 'pie'/'simulate' state.
  //
  // That same McpSafeAssetSave is why neither action is undoable: the save
  // escapes any editor transaction, so Ctrl+Z cannot reverse it. No AI handler
  // opens an FScopedTransaction at all, so both carry the pessimistic undo
  // default rather than a claim. Authoring and undoable are independent facts.
  it('declares every manage_ai action as editor authoring, set_focus included', () => {
    const focus = recordById('manage_ai.set_focus');
    const authoring = recordById('manage_ai.create_behavior_tree');
    expect(focus.availability.editorStates).toEqual(['edit']);
    expect(authoring.availability.editorStates).toEqual(['edit']);
    for (const record of [focus, authoring]) {
      expect(record.behavior.supportsUndo).toBe(false);
      expect(record.behavior.semantics.undo.mode).toBe('none');
      expect(record.behavior.semantics.undo.transactionScope).toBeNull();
      expect(record.behavior.semantics.undo.evidence.grade).toBe('pessimistic-default');
    }

    const runtimeClaims = GAMEPLAY_SOURCE_RECORDS
      .filter((record) => record.id.startsWith('manage_ai.'))
      .filter((record) => record.availability.editorStates.some((state) => state !== 'edit'))
      .map((record) => record.id);
    expect(runtimeClaims).toEqual([]);
  });

  it('exposes independently identifiable outputs for representative asset creation', () => {
    const creationRecords: readonly CapabilityRecordSource[] = [
      recordById('animation_physics.create_animation_blueprint'),
      recordById('manage_effect.create_niagara_system'),
      recordById('manage_gas.create_gameplay_ability'),
      recordById('manage_character.create_character_blueprint'),
      recordById('manage_combat.create_weapon_blueprint'),
      recordById('manage_ai.create_behavior_tree'),
      recordById('manage_inventory.create_item_data_asset'),
      recordById('manage_interaction.create_door_actor'),
    ];
    for (const record of creationRecords) {
      expect(
        Object.keys(record.schemas.output.properties).some((key) => key.endsWith('Path')),
        record.id,
      ).toBe(true);
    }
  });
});
