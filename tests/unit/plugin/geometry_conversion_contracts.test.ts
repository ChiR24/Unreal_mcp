// Wiring contracts of the materials and collision convert_to_static_mesh and convert_to_nanite give a baked asset: the
// bake needs an editor to run. Behaviour itself belongs to the integration cases in
// tests/mcp-tools/world/manage-geometry-conversion.test.mjs.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const MODULE = join('plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge');
const GEOMETRY = join(MODULE, 'Private', 'Domains', 'Geometry');

/** Block and line comments removed, so no assertion can be satisfied by prose. */
const strip = (source: string): string => source.replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');
const read = (...segments: readonly string[]): string => strip(readFileSync(join(GEOMETRY, ...segments), 'utf8'));

describe('convert_to_static_mesh and convert_to_nanite give the asset its materials and collision', () => {
  const convert = (): string => read('Support', 'McpAutomationBridge_GeometryAssetConversion.cpp');
  const slots = (): string => read('Support', 'McpAutomationBridge_GeometryConversionSlots.cpp');

  it('loads each material through the path sanitizer and refuses what is not a material', () => {
    const source = slots();
    expect(source).toContain('SanitizeProjectRelativePath(Path)');
    expect(source).toContain('McpPathRefusalMessage(TEXT("materials"), Path)');
    expect(source).toContain('Cast<UMaterialInterface>(McpLoadAsset(SafePath))');
    expect(source).toContain('does not load as a material or a material instance');
  });

  it('checks the materials before the asset is created, so a bad path leaves nothing behind', () => {
    const source = convert();
    const load = source.indexOf('LoadConversionMaterials(Payload, SlotCount, Materials, MaterialError)');
    const create = source.indexOf('CreateNewStaticMeshAssetFromMesh(');
    expect(load).toBeGreaterThan(-1);
    expect(create).toBeGreaterThan(load);
    expect(source).toContain('TEXT("INVALID_MATERIALS")');
  });

  it('sizes the slots to the highest material id plus one and refuses a longer materials list', () => {
    const source = slots();
    expect(source).toContain('return HighestId + 1;');
    expect(source).toContain('Entries->Num() > SlotCount');
    expect(source).toContain('Slots[Slot].MaterialInterface = Materials[Slot];');
    expect(source).toContain('while (Slots.Num() < SlotCount)');
  });

  it('collision is box (the default), complex or none, and anything else is refused', () => {
    expect(convert()).toContain('GetJsonStringField(Payload, TEXT("collision"), TEXT("box")).ToLower()');
    expect(convert()).toContain('CollisionMode != TEXT("box") && CollisionMode != TEXT("complex") && CollisionMode != TEXT("none")');
    expect(convert()).toContain('ApplyConversionCollision(CreatedMesh, CollisionMode);');
    const source = slots();
    expect(source).toContain('BodySetup->CollisionTraceFlag = CTF_UseComplexAsSimple;');
    expect(source).toMatch(/Mode == TEXT\("none"\)\)\s*\{\s*BodySetup->CollisionTraceFlag = CTF_UseSimpleAsComplex;/u);
    expect(source).toContain('BodySetup->AggGeom.ConvexElems.Add(ConvexElem);');
    // Only the box mode builds a hull; complex and none leave the shape list empty.
    expect(source.split('ConvexElems.Add(').length - 1).toBe(1);
  });

  it('saves again after the materials and the collision, and lists the slots in the reply', () => {
    const source = convert();
    expect(source.indexOf('ApplyConversionMaterials(CreatedMesh, Materials, SlotCount);')).toBeLessThan(source.indexOf('McpSafeAssetSave(CreatedMesh)'));
    expect(source).toContain('Result->SetArrayField(TEXT("slots"), SlotsJson);');
    expect(source).toContain('Result->SetStringField(TEXT("collision"), CollisionMode);');
  });
});
