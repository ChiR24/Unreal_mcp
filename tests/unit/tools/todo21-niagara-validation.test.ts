// Todo 21 BB-027 — Niagara validation source contract.
// validate_niagara_system must accept assetPath/system forms and emit top-level valid+errors.
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

const ctx = () => read('Domains/NiagaraAuthoring/McpAutomationBridge_NiagaraAuthoringHandlersContext.cpp');
const infoVal = () => read('Domains/NiagaraAuthoring/McpAutomationBridge_NiagaraAuthoringHandlersInfoValidation.cpp');

describe('BB-027 Context canonicalizes all declared field forms', () => {
  it('system field is mapped to SystemPath as a fallback', () => {
    const s = code(ctx());
    const makeIdx = s.indexOf('MakeActionContext');
    expect(makeIdx).toBeGreaterThan(-1);
    const slice = s.slice(makeIdx, makeIdx + 1200);
    expect(slice).toMatch(/TEXT\("system"\)/);
  });
  it('assetPath is mapped to SystemPath as a final fallback', () => {
    const s = code(ctx());
    const makeIdx = s.indexOf('MakeActionContext');
    const slice = s.slice(makeIdx, makeIdx + 1200);
    expect(slice).toMatch(/assetPath.*SystemPath|SystemPath.*assetPath/i);
  });
});

describe('BB-027 ValidateNiagaraSystem output carries required valid', () => {
  it('top-level valid is set on the Result object', () => {
    const s = code(infoVal());
    const validateIdx = s.indexOf('ValidateNiagaraSystem');
    expect(validateIdx).toBeGreaterThan(-1);
    const slice = s.slice(validateIdx);
    // Must set 'valid' on the top-level Result, not just inside validationResult
    const validIdx = slice.indexOf('SetBoolField(TEXT("valid")');
    expect(validIdx).toBeGreaterThan(-1);
    // And it must be on Context.Result, not just on ValidationResult
    const resultSetIdx = slice.indexOf('Context.Result->SetBoolField(TEXT("valid")');
    expect(resultSetIdx).toBeGreaterThan(-1);
  });
  it('top-level errors array is set on the Result object', () => {
    const s = code(infoVal());
    const validateIdx = s.indexOf('ValidateNiagaraSystem');
    const slice = s.slice(validateIdx);
    const errorsIdx = slice.indexOf('Context.Result->SetArrayField(TEXT("errors")');
    expect(errorsIdx).toBeGreaterThan(-1);
  });
});
