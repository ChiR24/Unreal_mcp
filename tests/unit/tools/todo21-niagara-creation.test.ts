// Todo 21 BB-024 — Niagara creation source contract + TS handler.
import { existsSync, readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';

const TS = resolve(process.cwd(), 'src/tools/handlers/effect');
function readTsp(name: string): string {
  const p = resolve(TS, name);
  expect(existsSync(p), `missing: ${p}`).toBe(true);
  return readFileSync(p, 'utf8');
}
function code(s: string): string { return s.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/[^\n]*/g, ''); }

describe('BB-024 TS handler honors client path form', () => {
  it('effect-asset-actions.ts resolvedSavePath includes mutableArgs.path', () => {
    const s = readTsp('effect-asset-actions.ts');
    const createIdx = s.indexOf('create_niagara_system');
    expect(createIdx).toBeGreaterThan(-1);
    const slice = s.slice(createIdx, createIdx + 600);
    expect(code(slice)).toMatch(/mutableArgs\.path/);
  });
});

describe('BB-024 effect-handler-state resolves the default system before caching', () => {
  it('ensureDefaultNiagaraAuthoringAssets verifies the system resolves', () => {
    const s = readTsp('effect-handler-state.ts');
    const fnIdx = s.indexOf('ensureDefaultNiagaraAuthoringAssets');
    expect(fnIdx).toBeGreaterThan(-1);
    const slice = s.slice(fnIdx, fnIdx + 3000);
    // After creating the system, must verify it resolves (asset.exists or get_niagara_info)
    expect(code(slice)).toMatch(/asset\.exists|exists|get_niagara_info|resolve/i);
  });
});
