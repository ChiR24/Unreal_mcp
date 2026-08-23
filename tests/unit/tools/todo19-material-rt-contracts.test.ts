// Plan Todo 19 (BB-013, BB-015, BB-016) - render-target and material authoring
// contracts must match their canonical records and advertise their addressing.
//
// Written alongside the fixes; non-vacuity proven by mutation (see evidence).

import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { describe, expect, it } from 'vitest';

import { ALL_CAPABILITY_RECORDS } from '../../../src/tools/catalog/capabilities/records/aggregate.js';

const PRIVATE = join(
  'plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private'
);

function nativeSource(...segments: readonly string[]): string {
  return readFileSync(join(PRIVATE, ...segments), 'utf8');
}

function code(source: string): string {
  return source
    .replace(/\/\*[\s\S]*?\*\//gu, ' ')
    .replace(/\/\/[^\n]*/gu, ' ');
}

function record(id: string) {
  const found = ALL_CAPABILITY_RECORDS.find((entry) => String(entry.id) === id);
  if (!found) throw new Error(`fixture record '${id}' is absent from the canonical source`);
  return found;
}

describe('todo19 BB-013: create_render_target honors the canonical packagePath', () => {
  const handler = (): string =>
    code(nativeSource('Domains', 'Texture', 'McpAutomationBridge_TextureHandlersRenderTarget.cpp'));

  it('reads packagePath as the primary source for the destination folder', () => {
    expect(handler()).toContain('TEXT("packagePath")');
  });

  it('keeps legacy path as the fallback default', () => {
    expect(handler()).toContain('TEXT("path")');
    expect(handler()).toContain('TEXT("/Game/Textures")');
  });

  it('reads packagePath before renderTargetPath overrides apply', () => {
    const source = handler();
    const packageIdx = source.indexOf('TEXT("packagePath")');
    const rtIdx = source.indexOf('TEXT("renderTargetPath")');
    expect(packageIdx).toBeGreaterThan(-1);
    expect(rtIdx).toBeGreaterThan(-1);
    expect(packageIdx, 'packagePath must be resolved before renderTargetPath overrides').toBeLessThan(rtIdx);
  });
});

describe('todo19 BB-015: create_material_function declares its save control', () => {
  const ID = 'material.create_material_function';

  it('input schema declares the optional save boolean', () => {
    const rec = record(ID);
    const props = JSON.parse(JSON.stringify(rec.schemas.input.properties)) as Record<string, unknown>;
    expect(props).toHaveProperty('save');
    expect((props.save as { type?: string }).type).toBe('boolean');
  });

  it('save stays optional (name remains the only required field)', () => {
    expect(record(ID).schemas.input.required).toEqual(['name']);
  });
});

describe('todo19 BB-016: material graph reads advertise the result node', () => {
  const queries = (): string =>
    code(nativeSource('Domains', 'MaterialGraph', 'McpAutomationBridge_MaterialGraphHandlersQueries.cpp'));

  it('graph listing emits resultNode = Main', () => {
    const source = queries();
    const listingIdx = source.indexOf('availableNodes');
    const resultNodeIdx = source.indexOf('TEXT("resultNode")');
    const mainIdx = source.indexOf('TEXT("Main")');

    expect(listingIdx).toBeGreaterThan(-1);
    expect(resultNodeIdx).toBeGreaterThan(-1);
    expect(mainIdx).toBeGreaterThan(-1);
    expect(resultNodeIdx, 'resultNode must be emitted with the node listing').toBeGreaterThan(listingIdx);
  });

  it('advertises the connectable root inputs', () => {
    const source = queries();
    expect(source).toContain('resultNodeInputs');
    for (const input of ['EmissiveColor', 'BaseColor', 'WorldPositionOffset']) {
      expect(source).toContain(input);
    }
  });

  it('get_material_stats also advertises resultNode (the always-reachable read)', () => {
    const source = code(nativeSource(
      'Domains', 'AssetWorkflow', 'Materials', 'McpAutomationBridge_AssetWorkflowMaterialStats.cpp'
    ));
    expect(source).toContain('TEXT("resultNode")');
    expect(source).toContain('TEXT("Main")');
    expect(source).toContain('resultNodeInputs');
  });
});
