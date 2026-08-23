// Todo 21 BB-025 — Niagara verification source contract.
// get_niagara_info must emit top-level emitterCount so the false-success chain
// is structurally detectable. The module gate must fail closed on a missing system.
import { existsSync, readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';

const PRIVATE = resolve(process.cwd(), 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private');
function read(...parts: string[]): string {
  const p = resolve(PRIVATE, ...parts);
  expect(existsSync(p), `missing: ${p}`).toBe(true);
  return readFileSync(p, 'utf8');
}
function code(s: string): string { return s.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/[^\n]*/g, ''); }

const infoVal = () => read('Domains/NiagaraAuthoring/McpAutomationBridge_NiagaraAuthoringHandlersInfoValidation.cpp');
const ctx = () => read('Domains/NiagaraAuthoring/McpAutomationBridge_NiagaraAuthoringHandlersContext.cpp');

describe('BB-025 false-success chain is structurally detectable', () => {
  it('GetNiagaraInfo emits emitterCount at the top level for the System branch', () => {
    const s = code(infoVal());
    const getIdx = s.indexOf('GetNiagaraInfo');
    expect(getIdx).toBeGreaterThan(-1);
    // Scope to the GetNiagaraInfo function body
    const sendSuccessIdx = s.indexOf('SendSuccess', getIdx);
    const slice = s.slice(getIdx, sendSuccessIdx);
    expect(slice).toContain('SetNumberField(TEXT("emitterCount")');
  });
  it('emitterCount is top-level, not nested inside niagaraInfo', () => {
    const s = code(infoVal());
    const getIdx = s.indexOf('GetNiagaraInfo');
    const sendSuccessIdx = s.indexOf('SendSuccess', getIdx);
    const slice = s.slice(getIdx, sendSuccessIdx);
    // The top-level SetNumberField must come AFTER the SetObjectField(niagaraInfo)
    const niagaraInfoIdx = slice.indexOf('SetObjectField(TEXT("niagaraInfo")');
    const emitterCountIdx = slice.indexOf('SetNumberField(TEXT("emitterCount")');
    expect(niagaraInfoIdx).toBeGreaterThan(-1);
    expect(emitterCountIdx).toBeGreaterThan(niagaraInfoIdx);
  });
  it('LoadSystemOrError sends ASSET_NOT_FOUND on a missing system', () => {
    const s = code(ctx());
    expect(s).toContain('ASSET_NOT_FOUND');
  });
});
