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
const parsing = () => read('Domains/Effect/McpAutomationBridge_EffectHandlersParsing.cpp');
function loaderBody(): string {
  const s = code(parsing());
  const body = s.slice(s.indexOf('UObject* LoadEffectAsset'));
  return body.slice(0, body.indexOf('\n}'));
}

describe('BB-028 spawn_niagara canonicalizes the system path', () => {
  it('resolves the system through the shared loader, never UEditorAssetLibrary', () => {
    const s = code(spawn());
    expect(s).toMatch(/LoadEffectAsset\(SystemPath\)/);
    // UEditorAssetLibrary refuses every call while PIE runs: every system read as "not found" during play.
    expect(s).not.toMatch(/UEditorAssetLibrary::/);
  });
  it('the loader canonicalizes before it finds or loads the asset', () => {
    const body = loaderBody();
    const canonicalIdx = body.indexOf('ObjectPathToPackageName');
    expect(canonicalIdx).toBeGreaterThan(-1);
    expect(body.indexOf('FindObject')).toBeGreaterThan(canonicalIdx);
    expect(body.indexOf('LoadObject')).toBeGreaterThan(canonicalIdx);
    expect(body).not.toMatch(/UEditorAssetLibrary::/);
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
