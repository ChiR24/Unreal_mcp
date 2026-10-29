// A created Niagara emitter asset keeps the name it was asked for. Wiring contracts only: the behaviour is pinned
// in-editor by EngineWire.Niagara.CreateEmitter.KeepsName.
//
// create_niagara_emitter passed the requested name to SetUniqueEmitterName. From UE 5.4 the engine compares an
// asset's name with GetFName().GetPlainNameString(), which drops a trailing "_<number>", and renames the object when
// they differ: name "NE_Spark_0" answered ".../NE_Spark_0.NE_Spark_1", and the package held the emitter NE_Spark_1
// plus a redirector named NE_Spark_0. UE 5.0-5.3 compare GetName() instead.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const PRIVATE = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private');
const AUTHORING = join(PRIVATE, 'Domains', 'NiagaraAuthoring');

/** Block and line comments removed, so no assertion can be satisfied (or tripped) by prose. */
const strip = (text: string): string => text.replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');
const code = (...segments: readonly string[]): string => strip(readFileSync(join(...segments), 'utf8').replace(/\r\n/gu, '\n'));

/** The text from `open` up to the next `close`, or '' when `open` is absent. */
const between = (text: string, open: string, close: string): string => {
  const at = text.indexOf(open);
  return at < 0 ? '' : text.slice(at, text.indexOf(close, at));
};
const namingCalls = (text: string): string[] =>
  [...text.matchAll(/SetUniqueEmitterName\(([^;]*)\);/gu)].map((m) => m[1].replace(/\s+/gu, ''));

describe('create_niagara_emitter names the emitter without renaming the asset', () => {
  it('names the new emitter through McpNameNewEmitterAsset on both creation paths, never with the requested name', () => {
    const systems = code(AUTHORING, 'McpNiagaraSystems.cpp');
    const start = systems.indexOf('static bool CreateNiagaraEmitter(');
    const creator = systems.slice(start, systems.indexOf('\n}\n', start));
    expect(start).toBeGreaterThan(-1);
    // UE 5.1/5.2 create through FactoryCreateNew, 5.3+ through InitializeEmitter: each path names the emitter itself.
    const factoryPath = between(creator, '#if MCP_HAS_NIAGARA_EMITTER_FACTORY_NEW && ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 3\n', '#else');
    const initializePath = between(creator, '#if MCP_HAS_NIAGARA_EMITTER_FACTORY_NEW && (ENGINE_MAJOR_VERSION > 5 || ENGINE_MINOR_VERSION >= 3)\n', '#endif');
    expect(factoryPath).toContain('FactoryCreateNew(');
    expect(factoryPath).toContain('McpNameNewEmitterAsset(*NewEmitter);');
    expect(initializePath).toContain('InitializeEmitter(NewEmitter, true);');
    expect(initializePath).toContain('McpNameNewEmitterAsset(*NewEmitter);');
    expect(creator).not.toContain('SetUniqueEmitterName(');
  });

  it('passes the name the engine compares against: the plain name from UE 5.4, the full name before', () => {
    const helper = code(AUTHORING, 'McpNiagaraEmitterName.h');
    const guarded = between(helper, '#if ENGINE_MAJOR_VERSION > 5 || ENGINE_MINOR_VERSION >= 4\n', '#endif');
    const [from54, before54 = ''] = guarded.split('#else\n');
    expect(namingCalls(from54)).toEqual(['Emitter.GetFName().GetPlainNameString()']);
    expect(namingCalls(before54)).toEqual(['Emitter.GetName()']);
    expect(namingCalls(helper)).toHaveLength(2);
  });

  it('is pinned in-editor where InitializeEmitter is exported (UE 5.3+)', () => {
    const test = readFileSync(join(PRIVATE, 'Tests', 'Niagara', 'McpNiagaraEmitterNameTests.cpp'), 'utf8').replace(/\r\n/gu, '\n');
    expect(test).toContain('"EngineWire.Niagara.CreateEmitter.KeepsName"');
    expect(test).toContain('#if MCP_HAS_NIAGARA_EMITTER_FACTORY_NEW && (ENGINE_MAJOR_VERSION > 5 || ENGINE_MINOR_VERSION >= 3)\n');
    expect(test).toContain('McpNameNewEmitterAsset(*Emitter)');
  });
});
