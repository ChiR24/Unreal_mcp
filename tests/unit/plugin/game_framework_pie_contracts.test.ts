// Todo 23 BB-034..BB-035 — set_hud_class record + PIE GameMode persistence source contracts.
import { existsSync, readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';

const PRIVATE = resolve(process.cwd(), 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private');
const TS_ROOT = resolve(process.cwd(), 'src');
function readCpp(...parts: string[]): string {
  const p = resolve(PRIVATE, ...parts);
  expect(existsSync(p), `missing: ${p}`).toBe(true);
  return readFileSync(p, 'utf8');
}
function readTs(...parts: string[]): string {
  const p = resolve(TS_ROOT, ...parts);
  expect(existsSync(p), `missing: ${p}`).toBe(true);
  return readFileSync(p, 'utf8');
}
function code(s: string): string { return s.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/[^\n]*/g, ''); }

const classConfig = () => readCpp('Domains/GameFramework/McpAutomationBridge_GameFrameworkHandlersClassConfig.cpp');
const ctx = () => readCpp('Domains/GameFramework/McpAutomationBridge_GameFrameworkHandlersContext.cpp');
const frameworkRecord = () => readTs('tools/catalog/capabilities/records/manage-networking/framework.data.ts');
const frameworkHandler = () => readTs('tools/handlers/game-framework/game-framework-handlers.ts');

describe('BB-034 set_hud_class record exists', () => {
  it('framework.data.ts declares set_hud_class', () => {
    const s = code(frameworkRecord());
    expect(s).toContain('set_hud_class');
  });
  it('set_hud_class record requires gameModeBlueprint and hudClass', () => {
    const s = code(frameworkRecord());
    const idx = s.indexOf('set_hud_class');
    const slice = s.slice(idx, idx + 300);
    expect(slice).toMatch(/hudClass/);
    expect(slice).toMatch(/gameModeBlueprint/);
  });
});

describe('BB-034 TS handler dispatches set_hud_class', () => {
  it('game-framework-handlers.ts has a set_hud_class case', () => {
    const s = code(frameworkHandler());
    expect(s).toMatch(/case.*set_hud_class/i);
  });
  it('set_hud_class case validates gameModeBlueprint + hudClass and dispatches', () => {
    const s = code(frameworkHandler());
    const idx = s.indexOf('set_hud_class');
    expect(idx).toBeGreaterThan(-1);
    const slice = s.slice(idx, idx + 500);
    expect(slice).toMatch(/hudClass/);
    expect(slice).toMatch(/sendRequest/);
  });
});

describe('BB-034 native handler has set_hud_class branch', () => {
  it('ClassConfig.cpp routes set_hud_class through SetGameModeClass with HUDClass', () => {
    const s = code(classConfig());
    const idx = s.indexOf('set_hud_class');
    expect(idx).toBeGreaterThan(-1);
    const slice = s.slice(idx, idx + 400);
    expect(slice).toContain('HUDClass');
    expect(slice).toContain('SetGameModeClass');
  });
});

describe('BB-035 PersistEffectiveGameFramework touches UGameMapsSettings + WorldSettings', () => {
  it('ClassConfig.cpp or Context.cpp contains PersistEffectiveGameFramework', () => {
    const s1 = code(classConfig());
    const s2 = code(ctx());
    const combined = s1 + s2;
    expect(combined).toMatch(/PersistEffectiveGameFramework/i);
  });
  it('the helper writes GlobalDefaultGameMode via GConfig and calls ReloadConfig', () => {
    const s1 = code(classConfig());
    const s2 = code(ctx());
    const combined = s1 + s2;
    expect(combined).toMatch(/GlobalDefaultGameMode/i);
    expect(combined).toMatch(/GConfig->SetString/i);
    expect(combined).toMatch(/ReloadConfig/i);
  });
  it('the helper sets WorldSettings DefaultGameMode', () => {
    const s1 = code(classConfig());
    const s2 = code(ctx());
    const combined = s1 + s2;
    expect(combined).toMatch(/DefaultGameMode/i);
  });
  it('SetGameModeClass invokes PersistEffectiveGameFramework after a successful class mutation', () => {
    const s = code(classConfig());
    const setGmIdx = s.indexOf('SetGameModeClass');
    expect(setGmIdx).toBeGreaterThan(-1);
    expect(s, 'SetGameModeClass must call PersistEffectiveGameFramework').toMatch(/PersistEffectiveGameFramework/i);
  });
});
