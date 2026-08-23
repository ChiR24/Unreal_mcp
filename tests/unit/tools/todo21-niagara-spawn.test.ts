// Todo 21 BB-028 — Niagara spawn path canonicalization source contract.
// spawn_niagara must canonicalize the system path (accept both package and
// object forms) and verify the spawned component's asset is set before success.
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

const spawn = () => read('Domains/Effect/McpAutomationBridge_EffectHandlersNiagaraSpawn.cpp');

describe('BB-028 spawn_niagara canonicalizes the system path', () => {
  it('uses FSoftObjectPath or FPackageName to canonicalize before DoesAssetExist', () => {
    const s = code(spawn());
    const doesAssetExistIdx = s.indexOf('DoesAssetExist');
    expect(doesAssetExistIdx).toBeGreaterThan(-1);
    const before = s.slice(0, doesAssetExistIdx);
    expect(before).toMatch(/FSoftObjectPath|GetLongPackageName|ObjectPathToPackageName/i);
  });
  it('uses the canonicalized path for both the existence check and the load', () => {
    const s = code(spawn());
    const doesAssetExistIdx = s.indexOf('DoesAssetExist');
    const loadAssetIdx = s.indexOf('LoadAsset', doesAssetExistIdx);
    expect(loadAssetIdx).toBeGreaterThan(-1);
    // Both should use the same canonicalized variable
    const between = s.slice(doesAssetExistIdx, loadAssetIdx);
    expect(between).toMatch(/Canonical|CanonicalPath|PackageName/i);
  });
});

describe('BB-028 spawn verifies the component asset before success', () => {
  it('asserts the component asset is set (no null-asset success)', () => {
    const s = code(spawn());
    const setAssetIdx = s.indexOf('SetAsset');
    expect(setAssetIdx).toBeGreaterThan(-1);
    const successIdx = s.indexOf('SendAutomationResponse', setAssetIdx);
    expect(successIdx).toBeGreaterThan(setAssetIdx);
    const window = s.slice(setAssetIdx, successIdx);
    // Must check GetAsset() != null or IsActive() or similar verification
    expect(window).toMatch(/GetAsset|IsActive|IsValid|!= *nullptr/i);
  });
});
