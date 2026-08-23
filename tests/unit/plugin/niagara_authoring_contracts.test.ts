// Todo 21 — Niagara authoring source contracts (BB-024, BB-025, BB-027, BB-028).
//
// These assertions read the plugin source text so a contract that exists only
// in a comment cannot pass. Live round-trip proofs run at Todo 39.

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

const systems = () => read('Domains/NiagaraAuthoring/McpAutomationBridge_NiagaraAuthoringHandlersSystems.cpp');
const sysHandlers = () => read('Domains/NiagaraSystem/McpAutomationBridge_NiagaraSystemHandlers.cpp');
const ctx = () => read('Domains/NiagaraAuthoring/McpAutomationBridge_NiagaraAuthoringHandlersContext.cpp');
const infoVal = () => read('Domains/NiagaraAuthoring/McpAutomationBridge_NiagaraAuthoringHandlersInfoValidation.cpp');
const spawn = () => read('Domains/Effect/McpAutomationBridge_EffectHandlersNiagaraSpawn.cpp');

describe('BB-024 create_niagara_system returns systemPath and verifies the package', () => {
  it('authoring variant emits systemPath', () => {
    expect(code(systems())).toContain('SetStringField(TEXT("systemPath")');
  });
  it('authoring variant verifies package existence before success', () => {
    const s = code(systems());
    const sysPathIdx = s.indexOf('SetStringField(TEXT("systemPath")');
    const sendSuccessIdx = s.indexOf('SendSuccess', sysPathIdx);
    expect(sendSuccessIdx).toBeGreaterThan(sysPathIdx);
    const window = s.slice(0, sendSuccessIdx);
    expect(window).toMatch(/DoesPackageExist|DoesAssetExist|PackageName.*Exist/i);
  });
  it('direct handler accepts path as fallback for savePath', () => {
    const s = code(sysHandlers());
    expect(s, 'must not have an unconditional savePath-required refusal').not.toContain('savePath required');
    expect(s).toMatch(/path.*savePath|savePath.*path/i);
  });
});

describe('BB-025 get_niagara_info emits top-level emitterCount', () => {
  it('System branch sets top-level emitterCount', () => {
    const s = code(infoVal());
    const getNiagaraInfoIdx = s.indexOf('GetNiagaraInfo');
    expect(getNiagaraInfoIdx).toBeGreaterThan(-1);
    const slice = s.slice(getNiagaraInfoIdx, getNiagaraInfoIdx + 2000);
    expect(slice).toContain('SetNumberField(TEXT("emitterCount")');
  });
});

describe('BB-027 Context canonicalizes SystemPath from system and assetPath', () => {
  it('MakeActionContext maps system field to SystemPath', () => {
    const s = code(ctx());
    expect(s).toMatch(/system.*SystemPath|SystemPath.*system/i);
  });
  it('MakeActionContext maps assetPath as a SystemPath fallback', () => {
    const s = code(ctx());
    expect(s).toMatch(/assetPath.*SystemPath|SystemPath.*assetPath/i);
  });
});

describe('BB-027 ValidateNiagaraSystem emits top-level valid and errors', () => {
  it('ValidateNiagaraSystem sets top-level valid', () => {
    const s = code(infoVal());
    const validateIdx = s.indexOf('ValidateNiagaraSystem');
    expect(validateIdx).toBeGreaterThan(-1);
    const slice = s.slice(validateIdx);
    expect(slice).toContain('SetBoolField(TEXT("valid")');
  });
  it('ValidateNiagaraSystem sets top-level errors array', () => {
    const s = code(infoVal());
    const validateIdx = s.indexOf('ValidateNiagaraSystem');
    const slice = s.slice(validateIdx);
    expect(slice).toContain('SetArrayField(TEXT("errors")');
  });
});

describe('BB-028 spawn_niagara canonicalizes the system path and verifies the component', () => {
  it('canonicalizes SystemPath before the existence check', () => {
    const s = code(spawn());
    const doesAssetExistIdx = s.indexOf('DoesAssetExist');
    expect(doesAssetExistIdx).toBeGreaterThan(-1);
    const before = s.slice(0, doesAssetExistIdx);
    expect(before).toMatch(/FSoftObjectPath|GetLongPackageName|ObjectPathToPackageName/i);
  });
  it('verifies the component asset is set before success', () => {
    const s = code(spawn());
    const setAssetIdx = s.indexOf('SetAsset');
    const successIdx = s.indexOf('SendAutomationResponse', setAssetIdx);
    expect(setAssetIdx).toBeGreaterThan(-1);
    expect(successIdx).toBeGreaterThan(setAssetIdx);
    const window = s.slice(setAssetIdx, successIdx);
    expect(window).toMatch(/GetAsset|IsActive|IsA.*UNiagaraSystem/i);
  });
});
