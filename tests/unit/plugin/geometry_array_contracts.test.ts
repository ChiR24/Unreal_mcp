// Source contract for array_mesh linear, which needs an editor to run. AppendMeshRepeated appends its first copy
// untransformed unless told otherwise, so count 2 laid an exact duplicate over the original and every array came out
// one copy short. Behaviour belongs to the integration cases.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const SUPPORT = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Domains', 'Geometry', 'Support');

describe('array_mesh linear', () => {
  it('offsets the first copy too, so count copies sit one offset apart', () => {
    const source = readFileSync(join(SUPPORT, 'McpGeometryArrays.cpp'), 'utf8');
    expect(source).toContain('Target->Mesh, SourceMesh, RepeatTransform, Count - 1, true, false, FGeometryScriptAppendMeshOptions(), nullptr);');
  });
});
