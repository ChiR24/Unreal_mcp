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

  it('is the only variant that declares materials, so sending materials selects it; save it shares with mesh_collision', () => {
    expect(record?.routing.dispatchBy?.declaredBy?.materials).toEqual(['mesh_materials']);
    expect(record?.routing.dispatchBy?.declaredBy?.save).toEqual(['mesh_materials', 'mesh_collision']);
    expect(record?.routing.dispatchBy?.declaredBy?.collisionType).toEqual(['mesh_collision']);
  });

  // A Fab pack of 32 meshes took 64 calls (one call and one consent per mesh).
  it('takes assetPaths, as lods does, so one call sets a whole pack', () => {
    expect(record?.routing.dispatchBy?.declaredBy?.assetPaths).toEqual(['lods', 'mesh_materials']);
    expect(String((properties.assetPaths as { description?: string } | undefined)?.description)).toContain('passing over a slot a mesh lacks');
  });

  it('takes a slot by index or by name, and a material path, in every entry', () => {
    const items = (properties.materials as { items?: { properties?: Record<string, { type?: unknown }>; required?: string[] } } | undefined)?.items;
    expect(items?.properties?.slot?.type).toEqual(['integer', 'string']);
    expect(items?.required).toEqual(['slot', 'materialPath']);
  });
});
