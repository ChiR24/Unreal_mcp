// An edited material is recompiled inside an update context, as the material editor's Apply does, so the loaded
// instances that compile shaders of their own (a static switch or a base property override) follow the edit. Every
// material edit goes through McpRecompileMaterialHost; a bare PostEditChange on an edited material left those instances
// drawing the old graph until they were rebuilt.

import { readdirSync, readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const PRIVATE = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private');
const read = (file: string): string => readFileSync(file, 'utf8').replace(/\r\n/gu, '\n');

describe('material recompile', () => {
  it('runs PostEditChange inside an update context that names the material', () => {
    const helper = read(join(PRIVATE, 'Foundation', 'BridgeHelpers', 'Assets', 'McpMaterialRecompile.h'));
    const context = helper.indexOf('FMaterialUpdateContext Context;');
    expect(context).toBeGreaterThan(-1);
    expect(helper).toContain('Context.AddMaterial(Material);');
    expect(context).toBeLessThan(helper.indexOf('Host->PostEditChange();'));
  });

  it('leaves no bare PostEditChange on an edited material', () => {
    const domain = join(PRIVATE, 'Domains', 'MaterialAuthoring');
    const files = readdirSync(domain, { recursive: true, encoding: 'utf8' })
      .filter((file) => /\.(cpp|h)$/u.test(file))
      // A material created by the call has no instances yet.
      .filter((file) => !file.endsWith('McpMatCreateMat.cpp'))
      .map((file) => join(domain, file));
    files.push(join(PRIVATE, 'Domains', 'Property', 'McpPropActorAccess.cpp'));
    for (const file of files) {
      expect(read(file), file).not.toMatch(/\b(?:Material|BaseMaterial|Host)->PostEditChange\(\)/u);
    }
  });
});
