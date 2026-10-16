// A Niagara create action writes only a package it checked. Wiring contracts only: the behaviour is pinned
// in-editor by EngineWire.Niagara.CreatePackage.Names.
//
// create_niagara_emitter / create_niagara_system built the package from the sanitized folder plus the name and
// handed it to CreatePackage. SanitizeProjectRelativePath accepts any character under /Game, so "/Game/My FX"
// passed: the emitter answered "Emitter created." with nothing saved, and the system answered CREATE_FAILED.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const PRIVATE = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private');
const AUTHORING = join(PRIVATE, 'Domains', 'NiagaraAuthoring');

/** Block and line comments removed, so no assertion can be satisfied (or tripped) by prose. */
const strip = (text: string): string => text.replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');
const code = (...segments: readonly string[]): string => strip(readFileSync(join(...segments), 'utf8').replace(/\r\n/gu, '\n'));
const body = (source: string, signature: string): string => {
  const start = source.indexOf(signature);
  expect(start, `missing ${signature}`).toBeGreaterThan(-1);
  const end = source.indexOf('\n}\n', start);
  expect(end, `no end found for ${signature}`).toBeGreaterThan(start);
  return source.slice(start, end);
};

describe('Niagara create actions create only the package they checked', () => {
  const systems = code(AUTHORING, 'McpNiagaraSystems.cpp');

  for (const signature of ['static bool CreateNiagaraSystem(', 'static bool CreateNiagaraEmitter(']) {
    it(`${signature.slice(12, -1)} resolves the package first and refuses with INVALID_ARGUMENT`, () => {
      const handler = body(systems, signature);
      const resolveAt = handler.indexOf('McpNiagaraCreatePackageName(Context.Path, Context.Name, PathError)');
      const createAt = handler.indexOf('CreatePackage(*PackageName)');
      expect(resolveAt).toBeGreaterThan(-1);
      expect(createAt).toBeGreaterThan(resolveAt);
      expect(handler.slice(resolveAt, createAt)).toMatch(/Context\.SendError\(PathError, TEXT\("INVALID_ARGUMENT"\)\);\s*return true;/u);
      expect(handler.match(/(?<!\w)CreatePackage\(/gu)).toHaveLength(1);
      expect(handler).not.toMatch(/Context\.Path \+=/u);
    });
  }
});

describe('McpNiagaraCreatePackageName', () => {
  const helper = body(code(AUTHORING, 'McpNiagaraCreatePackage.h'), 'inline FString McpNiagaraCreatePackageName(');

  it('sanitizes the whole package name, then requires a long package name under a writable root', () => {
    expect(helper).toMatch(/const FString Candidate = Requested \+ TEXT\("\/"\) \+ AssetName;/u);
    const sanitize = helper.indexOf('SanitizeProjectRelativePath(Candidate)');
    const longName = helper.indexOf('FPackageName::IsValidLongPackageName(PackageName, false');
    expect(sanitize).toBeGreaterThan(-1);
    expect(longName).toBeGreaterThan(sanitize);
  });

  it('is pinned in-editor', () => {
    const test = readFileSync(join(PRIVATE, 'Tests', 'Niagara', 'McpNiagaraCreatePackageTests.cpp'), 'utf8');
    expect(test).toContain('"EngineWire.Niagara.CreatePackage.Names"');
    expect(test).toContain('McpNiagaraCreatePackageName(');
  });
});
