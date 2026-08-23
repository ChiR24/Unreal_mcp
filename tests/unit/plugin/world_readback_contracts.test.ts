// Todo 20 — world read-back source contracts (BB-022, BB-023, BB-060).
//
// get_component_property must resolve properties through the SHARED nested-path
// resolver so dotted paths (BodyInstance.CollisionEnabled) resolve, and arrays of
// objects (OverrideMaterials) export as explicit object paths instead of opaque
// export-text. The geometry recalculate_normals path must reuse the shared actor
// resolver, not a per-handler TActorIterator copy.
//
// These assertions read the plugin source text (what the compiler sees) so a
// contract that exists only in a comment cannot pass. Live round-trip proofs
// (set_material -> OverrideMaterials read-back, collision dotted read,
// targetActor-only recalculate_normals) run at Todo 39 against a live editor.

import { existsSync, readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const PRIVATE_ROOT = resolve(
  process.cwd(),
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private'
);

function read(path: string): string {
  expect(existsSync(path), `missing native file: ${path}`).toBe(true);
  return readFileSync(path, 'utf8');
}

const privateSource = (...parts: string[]): string =>
  read(resolve(PRIVATE_ROOT, ...parts));

/** Strip comments so a claim in prose cannot satisfy a code contract. */
function code(source: string): string {
  return source.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/[^\n]*/g, '');
}

const componentProperties = () =>
  privateSource('Domains', 'ControlActor', 'McpAutomationBridge_ControlActorComponentProperties.cpp');
const reflectionArrays = () =>
  privateSource('Foundation', 'Reflection', 'McpPropertyReflectionArrays.cpp');
const meshInfoAndNormals = () =>
  privateSource('Domains', 'Geometry', 'Mesh', 'McpAutomationBridge_GeometryMeshInfoAndNormals.cpp');

describe('Todo 20 BB-022/023 get_component_property resolves through the shared nested-path resolver', () => {
  it('the get handler invokes ResolveNestedPropertyPath, not a bare FindPropertyByName', () => {
    const source = componentProperties();
    // Scope to the GET handler so the SET handler's legitimate FindPropertyByName
    // (single-name write path) does not muddy the read-path contract.
    const getStart = source.indexOf('HandleControlActorGetComponentProperty');
    expect(getStart).toBeGreaterThan(-1);
    const getSlice = source.slice(getStart);

    expect(code(getSlice), 'the read path must use the shared nested resolver').toContain(
      'ResolveNestedPropertyPath'
    );
    expect(code(getSlice), 'a bare single-name lookup cannot resolve dotted paths').not.toContain(
      'FindPropertyByName(*PropertyName)'
    );
    expect(
      code(getSlice),
      'export must read from the resolved container, not the component'
    ).toContain('ExportPropertyToJsonValue(ContainerPtr');
  });

  it('the get handler includes the shared nested-path helper header', () => {
    const source = componentProperties();
    expect(source).toContain(
      '#include "Foundation/BridgeHelpers/Properties/McpAutomationBridgeHelpersNestedPropertyPath.h"'
    );
  });

  it('no domain-local reflection copy is introduced in the get handler', () => {
    // The fix must reuse the shared boundary, not duplicate its traversal inline.
    const source = code(componentProperties());
    const getStart = source.indexOf('HandleControlActorGetComponentProperty');
    const getSlice = source.slice(getStart);
    expect(getSlice, 'the get handler must not inline a property-path parser').not.toMatch(
      /ParseIntoArray/
    );
  });
});

describe('Todo 20 BB-022 arrays of objects export as explicit object paths', () => {
  it('ExportArrayToJson has FObjectProperty and FSoftObjectProperty inner branches before the text fallback', () => {
    const source = code(reflectionArrays());
    const objectIdx = source.indexOf('CastField<FObjectProperty>(Inner)');
    const softIdx = source.indexOf('CastField<FSoftObjectProperty>(Inner)');
    const fallbackIdx = source.indexOf('MCP_PROPERTY_EXPORT_TEXT');

    expect(objectIdx, 'an explicit FObjectProperty inner branch must exist').toBeGreaterThan(-1);
    expect(softIdx, 'an explicit FSoftObjectProperty inner branch must exist').toBeGreaterThan(-1);
    expect(fallbackIdx).toBeGreaterThan(-1);
    expect(objectIdx, 'the object branch must precede the text fallback').toBeLessThan(fallbackIdx);
    expect(softIdx, 'the soft-object branch must precede the text fallback').toBeLessThan(
      fallbackIdx
    );
  });
});

describe('Todo 20 BB-060 geometry recalculate_normals reuses the shared actor resolver', () => {
  it('the mesh info/normals file references the shared resolver and has no inline TActorIterator loop', () => {
    const source = code(meshInfoAndNormals());
    expect(source, 'must call the shared FindDynamicMeshActorForGeometry').toContain(
      'FindDynamicMeshActorForGeometry'
    );
    expect(source, 'no per-handler TActorIterator copy may remain').not.toMatch(
      /TActorIterator<ADynamicMeshActor>/
    );
  });
});

// Live round-trip proofs deferred to Todo 39 (serialized live replay against a
// live editor):
//   T20-R6: set_material on a DynamicMesh actor then get_component_property(
//           OverrideMaterials) returns a non-empty array whose [0] equals the
//           assigned material path.
//   T20-R7: get_component_property(DynamicMesh, CollisionEnabled) returns the
//           enum name matching set_actor_collision.
//   T20-R8: get_component_property(propertyPath=BodyInstance.CollisionEnabled)
//           succeeds and agrees with the single-name read.
//   T20-R11: recalculate_normals with only targetActor succeeds; get_mesh_info
//            read-back confirms normals; neither targetActor nor actorName ->
//            typed INVALID_ARGUMENT.
