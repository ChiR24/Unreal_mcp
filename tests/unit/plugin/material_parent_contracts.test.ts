// A material instance's parent is set the way the material instance editor sets it. The engine compiles an instance's
// own shaders anew only when its parent changes, so the helper rebuilds it from no parent through SetParentEditorOnly
// and stores an undo record that rebuilds it again on undo and redo. set_property's Parent write goes through the same
// helper, after the raw write and before the object's own change handling.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

import { sliceBetween } from './plugin-contract-fixtures.js';

const PRIVATE = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private');
const read = (file: string): string => readFileSync(join(PRIVATE, file), 'utf8').replace(/\r\n/gu, '\n');

describe('material instance parent', () => {
  it('rebuilds from no parent through SetParentEditorOnly and records an undo change', () => {
    const helper = read(join('Domains', 'AssetWorkflow', 'Materials', 'McpMaterialParent.cpp'));
    const rebuild = sliceBetween(helper, 'void Rebuild(', 'class FParentChange');
    const cleared = rebuild.indexOf('Instance->Parent = nullptr;');
    expect(cleared).toBeGreaterThan(-1);
    expect(cleared).toBeLessThan(rebuild.indexOf('Instance->SetParentEditorOnly(To, true);'));
    expect(rebuild).toContain('FMaterialUpdateContext');
    expect(helper).toContain('GUndo->StoreUndo(Instance, MakeUnique<FParentChange>(Old, To));');
  });

  it('set_property applies a written Parent through the helper before the change handling runs', () => {
    const set = read(join('Domains', 'Property', 'McpPropObjectSet.cpp'));
    const before = set.indexOf('McpMaterialParent::HeldParent(RootObject)');
    const apply = set.indexOf('McpMaterialParent::ApplyWrittenParent(');
    expect(before).toBeGreaterThan(-1);
    expect(before).toBeLessThan(set.indexOf('RootObject->Modify(bMarkDirty);'));
    expect(set.indexOf('McpPropertyTarget::WriteValue(')).toBeLessThan(apply);
    expect(apply).toBeLessThan(set.indexOf('RootObject->PostEditChangeProperty(Changed);'));
  });
});
