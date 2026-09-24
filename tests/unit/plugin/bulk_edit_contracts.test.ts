// List forms that turn N consented round trips into one: set_material takes
// actorNames, remove_scs_component takes componentNames. Each item runs the
// ordinary single-item path and is reported; a partial result fails naming the
// items that did not apply.
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';
import { CONTROL_ACTOR_RECORDS } from '../../../src/tools/catalog/capabilities/records/control-actor/index.js';
import { MANAGE_BLUEPRINT_RECORDS } from '../../../src/tools/catalog/capabilities/records/manage-blueprint/index.js';

const DOMAINS = resolve(process.cwd(), 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains');
const source = (path: string): string => readFileSync(resolve(DOMAINS, path), 'utf8');

describe('set_material actorNames', () => {
  it('runs every name through the single-actor handler and reports each', () => {
    const s = source('ControlActor/McpAutomationBridge_ControlActorMaterials.cpp');
    expect(s).toContain('TryGetArrayField(TEXT("actorNames"), Names)');
    expect(s).toMatch(/Capture\.Begin\(ItemId\);\s*HandleControlActorSetMaterial\(ItemId, One, Socket\);\s*const FMcpCapturedResponse Reply = Capture\.End\(ItemId\);/);
    expect(s).toContain('TEXT("MATERIAL_BATCH_INCOMPLETE")');
  });

  it('the record accepts actorName or actorNames', () => {
    const record = CONTROL_ACTOR_RECORDS.find((r) => r.legacyIds[0].action === 'set_material');
    expect(record).toBeDefined();
    expect(record?.schemas.input.properties).toHaveProperty('actorNames');
    expect(record?.schemas.input.required).not.toContain('actorName');
    expect(record?.schemas.input.requiredOneOf).toEqual(['actorName', 'actorNames']);
  });
});

describe('remove_scs_component componentNames', () => {
  it('removes each name and fails naming the ones that stayed', () => {
    const s = source('Blueprint/Components/McpAutomationBridge_BlueprintHandlersScsWrappers.cpp');
    expect(s).toContain('TryGetArrayField(TEXT("componentNames"), Names)');
    expect(s).toContain('FSCSHandlers::RemoveSCSComponent(BPPath, One)');
    expect(s).toContain('TEXT("SCS_REMOVE_INCOMPLETE")');
  });

  it('the record accepts componentName or componentNames', () => {
    const record = MANAGE_BLUEPRINT_RECORDS.find((r) => r.legacyIds[0].action === 'remove_scs_component');
    expect(record).toBeDefined();
    expect(record?.schemas.input.properties).toHaveProperty('componentNames');
    expect(record?.schemas.input.required).not.toContain('componentName');
    expect(record?.schemas.input.requiredOneOf).toEqual(['componentName', 'componentNames']);
  });
});
