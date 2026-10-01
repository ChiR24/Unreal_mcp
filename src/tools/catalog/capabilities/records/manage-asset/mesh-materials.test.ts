import { describe, expect, it } from 'vitest';
import { ALL_CAPABILITY_RECORDS } from '../aggregate.js';

// set_mesh_materials writes the materials into a mesh ASSET's slots. It is the mesh_materials variant of
// process_asset, so these hold the wiring that lets a call reach it without naming the selector.
describe('asset.process_asset mesh_materials', () => {
  const record = ALL_CAPABILITY_RECORDS.find((entry) => String(entry.id) === 'asset.process_asset');
  const properties = record?.schemas.input.properties ?? {};

  it('dispatches the variant to set_mesh_materials and keeps that name callable', () => {
    expect(record?.routing.dispatchBy?.actions.mesh_materials).toBe('set_mesh_materials');
    const pair = record?.legacyIds.find((entry) => entry.action === 'set_mesh_materials');
    expect(pair?.folded).toEqual({ process: 'mesh_materials' });
  });

  it('is the only variant that declares materials and save, so sending materials selects it', () => {
    expect(record?.routing.dispatchBy?.declaredBy?.materials).toEqual(['mesh_materials']);
    expect(record?.routing.dispatchBy?.declaredBy?.save).toEqual(['mesh_materials']);
  });

  it('takes a slot by index or by name, and a material path, in every entry', () => {
    const items = (properties.materials as { items?: { properties?: Record<string, { type?: unknown }>; required?: string[] } } | undefined)?.items;
    expect(items?.properties?.slot?.type).toEqual(['integer', 'string']);
    expect(items?.required).toEqual(['slot', 'materialPath']);
  });
});
