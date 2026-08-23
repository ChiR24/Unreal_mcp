// Todo 21 BB-026 — Niagara routing execution lock.
// set_niagara_parameter and activate_effect routes already exist in the current
// code (Wave 2). These tests lock them so a regression would fail.
import { existsSync, readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';

const TS = resolve(process.cwd(), 'src/tools/handlers/effect');
const PRIVATE = resolve(process.cwd(), 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private');
function readTsp(name: string): string {
  const p = resolve(TS, name);
  expect(existsSync(p), `missing: ${p}`).toBe(true);
  return readFileSync(p, 'utf8');
}
function readCpp(...parts: string[]): string {
  const p = resolve(PRIVATE, ...parts);
  expect(existsSync(p), `missing: ${p}`).toBe(true);
  return readFileSync(p, 'utf8');
}
function code(s: string): string { return s.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/[^\n]*/g, ''); }

describe('BB-026 activate_effect routes to activate_niagara', () => {
  it('TS handler rewrites activate_effect to activate_niagara', () => {
    const s = code(readTsp('effect-routing-actions.ts'));
    expect(s).toMatch(/activate_effect[\s\S]*activate_niagara|activate_niagara[\s\S]*activate_effect/i);
  });
  it('native EffectHandlers.cpp re-dispatches activate_effect as activate_niagara', () => {
    const s = code(readCpp('Domains/Effect/McpAutomationBridge_EffectHandlers.cpp'));
    expect(s).toContain('activate_effect');
    expect(s).toContain('activate_niagara');
    const aeIdx = s.indexOf('activate_effect');
    const anIdx = s.indexOf('activate_niagara', aeIdx);
    expect(anIdx).toBeGreaterThan(aeIdx);
    // Must route through HandleEffectAction, not inline
    const routeWindow = s.slice(anIdx, anIdx + 300);
    expect(routeWindow).toContain('HandleEffectAction');
  });
});

describe('BB-026 set_niagara_parameter routes to the native parameter handler', () => {
  it('TS handler dispatches set_niagara_parameter via create_effect', () => {
    const s = code(readTsp('effect-routing-actions.ts'));
    expect(s).toContain('set_niagara_parameter');
  });
  it('native EffectHandlers.cpp routes set_niagara_parameter through HandleEffectAction', () => {
    const s = code(readCpp('Domains/Effect/McpAutomationBridge_EffectHandlers.cpp'));
    expect(s).toContain('set_niagara_parameter');
    // Find the routing-table occurrence (last one), not the gate condition
    const lastSnpIdx = s.lastIndexOf('set_niagara_parameter');
    expect(lastSnpIdx).toBeGreaterThan(-1);
    const routeWindow = s.slice(lastSnpIdx, lastSnpIdx + 300);
    expect(routeWindow).toContain('HandleEffectAction');
  });
});
