// Todo 20 BB-060 — geometry target normalization source contract.
//
// The record declares targetActor as required (actorName optional), but many
// native geometry handlers read only actorName. The TS bridge already copies
// targetActor -> actorName (geometry-handlers.ts), which covers the TS stdio
// surface; the NATIVE /mcp surface bypasses TS normalization, so the native
// dispatcher must apply the same non-clobbering copy once, centrally, before
// routing any subAction. No per-handler copies.
//
// Live proof (recalculate_normals with only targetActor succeeds; get_mesh_info
// read-back confirms normals) runs at Todo 39 against a live editor.

import { existsSync, readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const PRIVATE_ROOT = resolve(
  process.cwd(),
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private'
);
const TS_ROOT = resolve(process.cwd(), 'src/tools/handlers/geometry');

function read(path: string): string {
  expect(existsSync(path), `missing file: ${path}`).toBe(true);
  return readFileSync(path, 'utf8');
}

/** Strip comments so a claim in prose cannot satisfy a code contract. */
function code(source: string): string {
  return source.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/[^\n]*/g, '');
}

const geometryDispatcher = () =>
  read(resolve(PRIVATE_ROOT, 'Domains', 'Geometry', 'McpAutomationBridge_GeometryHandlers.cpp'));
const meshInfoAndNormals = () =>
  read(
    resolve(
      PRIVATE_ROOT,
      'Domains',
      'Geometry',
      'Mesh',
      'McpAutomationBridge_GeometryMeshInfoAndNormals.cpp'
    )
  );
const tsGeometryHandlers = () => resolve(TS_ROOT, 'geometry-handlers.ts');

describe('Todo 20 BB-060 the native geometry dispatcher normalizes targetActor centrally', () => {
  it('the dispatcher copies targetActor -> actorName before the first subAction route', () => {
    const source = code(geometryDispatcher());
    const seamIdx = source.indexOf('targetActor');
    const firstRouteIdx = source.indexOf('if (SubAction == TEXT("create_box"))');
    expect(firstRouteIdx, 'the subAction routing block must exist').toBeGreaterThan(-1);
    expect(seamIdx, 'the dispatcher must read targetActor').toBeGreaterThan(-1);
    expect(seamIdx, 'the normalization seam must precede subAction routing').toBeLessThan(
      firstRouteIdx
    );
    // The seam must set actorName.
    const seamWindow = source.slice(seamIdx, firstRouteIdx);
    expect(seamWindow).toContain('SetStringField(TEXT("actorName")');
  });

  it('the seam is non-clobbering: an explicit actorName is not overwritten', () => {
    const source = code(geometryDispatcher());
    const seamIdx = source.indexOf('targetActor');
    expect(seamIdx, 'the normalization seam must exist').toBeGreaterThan(-1);
    const firstRouteIdx = source.indexOf('if (SubAction == TEXT("create_box"))');
    const seamWindow = source.slice(seamIdx, firstRouteIdx);
    // Must guard on actorName being absent/empty before copying.
    expect(seamWindow).toMatch(/IsEmpty|TryGetStringField.*actorName/i);
  });

  it('no per-handler geometry file re-implements the targetActor copy', () => {
    // The fix is centralized in the dispatcher; the per-handler mesh file must
    // not carry its own targetActor->actorName seam.
    const source = code(meshInfoAndNormals());
    // The seam writes actorName onto the PAYLOAD (Payload->SetStringField); the
    // per-handler files legitimately echo actorName onto their RESULT object, so
    // scope the guard to a payload write, not any SetStringField(actorName).
    expect(source, 'per-handler files must not duplicate the dispatcher seam').not.toContain(
      'Payload->SetStringField(TEXT("actorName")'
    );
  });
});

describe('Todo 20 BB-060 the TS bridge already covers the stdio surface (protected file)', () => {
  // geometry-handlers.ts is PROTECTED (hash-backed union). Todo 20 must not edit
  // it; the TS-side copyAlias already normalizes targetActor -> actorName, so the
  // native dispatcher seam is the complementary fix for the native /mcp surface.
  it('geometry-handlers.ts carries the non-clobbering targetActor -> actorName copyAlias', () => {
    const source = read(tsGeometryHandlers());
    expect(source).toContain("copyAlias(normalized, args, 'targetActor', 'actorName')");
  });
});

// Live proof deferred to Todo 39 (serialized live replay against a live editor):
//   T20-R11: recalculate_normals with only targetActor succeeds and get_mesh_info
//            read-back confirms normals; neither targetActor nor actorName ->
//            typed INVALID_ARGUMENT (unchanged negative).
