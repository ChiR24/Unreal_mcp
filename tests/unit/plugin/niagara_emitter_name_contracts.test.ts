// A created Niagara emitter asset keeps the name it was asked for. Wiring contracts only: the behaviour is pinned
// in-editor by EngineWire.Niagara.CreateEmitter.KeepsName.
//
// create_niagara_emitter passed the requested name to SetUniqueEmitterName. For an asset the engine compares it
// with GetFName().GetPlainNameString(), which drops a trailing "_<number>", and renames the object when they
// differ: name "NE_Spark_0" answered ".../NE_Spark_0.NE_Spark_1", and the package held the emitter NE_Spark_1 plus
// a redirector named NE_Spark_0.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const PRIVATE = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private');
const AUTHORING = join(PRIVATE, 'Domains', 'NiagaraAuthoring');

/** Block and line comments removed, so no assertion can be satisfied (or tripped) by prose. */
const strip = (text: string): string => text.replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');
const code = (...segments: readonly string[]): string => strip(readFileSync(join(...segments), 'utf8').replace(/\r\n/gu, '\n'));

describe('create_niagara_emitter names the emitter without renaming the asset', () => {
  it('names the new emitter through McpNameNewEmitterAsset, never with the requested name', () => {
    const systems = code(AUTHORING, 'McpNiagaraSystems.cpp');
    const start = systems.indexOf('static bool CreateNiagaraEmitter(');
    const creator = systems.slice(start, systems.indexOf('\n}\n', start));
    expect(start).toBeGreaterThan(-1);
    expect(creator).toContain('McpNameNewEmitterAsset(*NewEmitter);');
    expect(creator).not.toContain('SetUniqueEmitterName(');
  });

  it('passes the plain name, as the engine does for an asset', () => {
    const helper = code(AUTHORING, 'McpNiagaraEmitterName.h');
    const calls = [...helper.matchAll(/SetUniqueEmitterName\(([^;]*)\);/gu)].map((m) => m[1].replace(/\s+/gu, ''));
    expect(calls).toEqual(['Emitter.GetFName().GetPlainNameString()']);
  });

  it('is pinned in-editor', () => {
    const test = readFileSync(join(PRIVATE, 'Tests', 'Niagara', 'McpNiagaraEmitterNameTests.cpp'), 'utf8');
    expect(test).toContain('"EngineWire.Niagara.CreateEmitter.KeepsName"');
    expect(test).toContain('McpNameNewEmitterAsset(*Emitter)');
  });
});
