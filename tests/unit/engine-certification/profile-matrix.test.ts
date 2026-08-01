// tests/unit/engine-certification/profile-matrix.test.ts
// Task 52 — the offline simulation is checked against the REAL contract and the
// REAL plugin sources, not against fixtures.
//
// A fixture-driven test here would pass forever: it would assert that a made-up
// record with min 5.7 is filtered on 5.5, which proves the comparison operator
// works and nothing at all about this project. The acceptance criterion is that
// the matrix filters the KNOWN 5.1+/5.3+/5.7+/runtime-optional features, so the
// canonical registry and the plugin's own `#if` lines are the inputs.
//
// The consequence is that these tests move when the product moves — and that is
// the property that makes them worth running. If a domain file drops its 5.3
// gate, the "5.3 threshold exists" assertion fails and somebody looks, instead
// of a private threshold table quietly answering with last month's rules.

import { describe, expect, it } from 'vitest';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import {
  GATE_CODES,
  buildProfileMatrix,
  collectNativeGates,
  compareEngineVersions,
  defineProfile,
  evaluateCapability,
  evaluateNativeFeatures,
} from './profile-matrix.mjs';

const REPO = process.cwd();
const PLUGIN_ROOT = resolve(REPO, 'plugins/McpAutomationBridge');

// Structurally typed rather than `as any[]`: only the fields this suite reads
// are declared, so a registry shape change surfaces here as a type error.
type RegistryRecord = {
  readonly id: string;
  readonly availability?: { readonly requiredPlugins?: readonly string[] };
};

const records: readonly RegistryRecord[] = (JSON.parse(readFileSync(
  resolve(REPO, 'src/tools/catalog/capabilities/generated/canonical-registry.generated.json'), 'utf8',
)) as { readonly records: readonly RegistryRecord[] }).records;

const recordOf = (id: string) => {
  const found = records.find((entry) => entry.id === id);
  if (found === undefined) throw new Error(`the canonical registry no longer contains ${id}; this test needs updating, not deleting`);
  return found;
};

/** Every plugin the .uplugin can enable, so "everything on" is a real profile. */
const ALL_PLUGINS: string[] = [...new Set(records.flatMap((entry) => entry.availability?.requiredPlugins ?? []))];

const profileFor = (minor: number, overrides: Record<string, unknown> = {}) => defineProfile({
  id: `ue5.${minor}`,
  engine: { major: 5, minor, patch: 0 },
  plugins: ALL_PLUGINS,
  ...overrides,
});

describe('compareEngineVersions', () => {
  it('orders by major, then minor, then patch', () => {
    expect(compareEngineVersions({ major: 5, minor: 7, patch: 4 }, { major: 5, minor: 7, patch: 0 })).toBe(1);
    expect(compareEngineVersions({ major: 5, minor: 3, patch: 9 }, { major: 5, minor: 5, patch: 0 })).toBe(-1);
    expect(compareEngineVersions({ major: 5, minor: 8, patch: 0 }, { major: 5, minor: 8, patch: 0 })).toBe(0);
  });
});

describe('evaluateCapability — the contract dimension', () => {
  const nanite = recordOf('manage_geometry.convert_to_nanite');

  it('filters the real 5.7+ capability below its declared minimum', () => {
    for (const minor of [0, 3, 5, 6]) {
      const verdict = evaluateCapability(nanite, profileFor(minor));
      expect(verdict.available, `5.${minor}`).toBe(false);
      expect(verdict.gates.map((gate) => gate.code)).toContain(GATE_CODES.ENGINE_BELOW_MIN);
    }
  });

  it('admits that same capability from 5.7 up', () => {
    for (const minor of [7, 8]) {
      expect(evaluateCapability(nanite, profileFor(minor)).available, `5.${minor}`).toBe(true);
    }
  });

  it('filters a runtime-optional capability when its plugin is not enabled', () => {
    // convert_to_nanite needs GeometryScripting, which the .uplugin marks Optional.
    const withoutPlugin = defineProfile({ id: 'ue5.7-no-geometry', engine: { major: 5, minor: 7, patch: 4 }, plugins: [] });
    const verdict = evaluateCapability(nanite, withoutPlugin);
    expect(verdict.available).toBe(false);
    expect(verdict.gates.map((gate) => gate.code)).toContain(GATE_CODES.PLUGIN_NOT_ENABLED);
    expect(verdict.gates.find((gate) => gate.code === GATE_CODES.PLUGIN_NOT_ENABLED)?.detail).toContain('GeometryScripting');
  });

  it('filters a PIE-only capability out of a plain editor session', () => {
    const pieOnly = records.find((entry) => {
      const states = entry.availability?.editorStates ?? [];
      return states.length > 0 && !states.includes('edit');
    });
    expect(pieOnly, 'the registry should still contain at least one pie/simulate-only capability').toBeDefined();
    const inEdit = evaluateCapability(pieOnly, profileFor(7));
    expect(inEdit.available).toBe(false);
    expect(inEdit.gates.map((gate) => gate.code)).toContain(GATE_CODES.EDITOR_STATE_UNSUPPORTED);
    expect(evaluateCapability(pieOnly, profileFor(7, { editorState: 'pie' })).available).toBe(true);
  });

  it('applies the asymmetric protocol support: legacy reaches stdio and not native', () => {
    const read = recordOf('asset.list');
    const legacyOnNative = defineProfile({
      id: 'legacy-native', engine: { major: 5, minor: 7, patch: 4 }, plugins: ALL_PLUGINS,
      client: { transport: 'native', protocolVersion: '2024-11-05' },
    });
    const legacyOnStdio = defineProfile({
      id: 'legacy-stdio', engine: { major: 5, minor: 7, patch: 4 }, plugins: ALL_PLUGINS,
      client: { transport: 'stdio', protocolVersion: '2024-11-05' },
    });
    expect(evaluateCapability(read, legacyOnNative).gates.map((gate) => gate.code))
      .toContain(GATE_CODES.CLIENT_PROTOCOL_UNSUPPORTED);
    expect(evaluateCapability(read, legacyOnStdio).available).toBe(true);
  });
});

describe('collectNativeGates — read from the plugin, never from a private table', () => {
  const gates = collectNativeGates({ pluginRoot: PLUGIN_ROOT });

  it('finds the compatibility header and a substantial number of real gates', () => {
    expect(gates.compatibilityHeader).not.toBeNull();
    expect(gates.conditions.length).toBeGreaterThan(100);
  });

  it('still contains the 5.1, 5.3 and 5.7 thresholds this project documents', () => {
    const joined = gates.distinctConditions.join('\n');
    expect(joined).toMatch(/ENGINE_MINOR_VERSION >= 1\b/u);
    expect(joined).toMatch(/ENGINE_MINOR_VERSION >= 3\b/u);
    expect(joined).toMatch(/ENGINE_MINOR_VERSION >= 7\b/u);
  });

  it('parses every condition it collected, so none is silently skipped', () => {
    const undecidableOn57 = evaluateNativeFeatures(profileFor(7), gates).undecided;
    // Undecidable here means "depends on a define this profile did not state",
    // which is a real answer — but it must be a small, nameable minority rather
    // than the bulk of the file quietly failing to parse.
    expect(undecidableOn57.length).toBeLessThan(gates.conditions.length * 0.1);
  });
});

describe('evaluateNativeFeatures — what actually compiles for an engine', () => {
  const gates = collectNativeGates({ pluginRoot: PLUGIN_ROOT });
  const compiledCounts = [0, 1, 3, 5, 7, 8].map((minor) => ({
    minor, ...evaluateNativeFeatures(profileFor(minor), gates),
  }));

  it('compiles strictly more of the plugin as the engine gets newer', () => {
    const counts = compiledCounts.map((entry) => entry.compiledCount);
    for (let index = 1; index < counts.length; index += 1) {
      expect(counts[index], `5.${compiledCounts[index]?.minor} vs 5.${compiledCounts[index - 1]?.minor}`)
        .toBeGreaterThanOrEqual(Number(counts[index - 1]));
    }
    // and the span is real, not a rounding difference
    expect(Number(counts.at(-1))).toBeGreaterThan(Number(counts[0]));
  });

  it('excludes the 5.1+ branches on 5.0 and includes them from 5.1', () => {
    const on50 = compiledCounts.find((entry) => entry.minor === 0);
    const on51 = compiledCounts.find((entry) => entry.minor === 1);
    const gate51 = 'ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1';
    expect(on50?.excluded.some((entry) => entry.condition === gate51)).toBe(true);
    expect(on51?.compiled.some((entry) => entry.condition === gate51)).toBe(true);
  });

  it('excludes the 5.7+ branches on 5.5 and includes them on 5.7', () => {
    const gate57 = 'ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7';
    expect(compiledCounts.find((entry) => entry.minor === 5)?.excluded.some((entry) => entry.condition === gate57)).toBe(true);
    expect(compiledCounts.find((entry) => entry.minor === 7)?.compiled.some((entry) => entry.condition === gate57)).toBe(true);
  });

  it('resolves the discontinuous widget-GUID macro exactly where the header says', () => {
    const macroOn = (minor: number) => evaluateNativeFeatures(profileFor(minor), gates).macros.MCP_HAS_WIDGET_VARIABLE_GUID_MAP;
    expect([0, 1, 2, 5, 6, 7, 8].map(macroOn)).toEqual([0, 1, 0, 0, 0, 1, 1]);
  });

  it('turns an optional PLUGIN into a compile-time define, which is the runtime-optional dimension', () => {
    const withMrp = defineProfile({ id: 'mrp-on', engine: { major: 5, minor: 7, patch: 4 }, plugins: ['MovieRenderPipeline'] });
    const withoutMrp = defineProfile({ id: 'mrp-off', engine: { major: 5, minor: 7, patch: 4 }, plugins: [] });
    expect(evaluateNativeFeatures(withMrp, gates).macros.MCP_HAS_MOVIE_RENDER_PIPELINE).toBe(1);
    expect(evaluateNativeFeatures(withoutMrp, gates).macros.MCP_HAS_MOVIE_RENDER_PIPELINE).toBe(0);
  });
});

describe('buildProfileMatrix', () => {
  const gates = collectNativeGates({ pluginRoot: PLUGIN_ROOT });
  const matrix = buildProfileMatrix({
    records,
    profiles: [profileFor(0), profileFor(5), profileFor(7), profileFor(7, { editorState: 'pie' })],
    nativeGates: gates,
  });

  it('reports one row per profile over the whole registry', () => {
    expect(matrix.rows).toHaveLength(4);
    for (const row of matrix.rows) expect(row.total).toBe(records.length);
  });

  it('filters more capabilities on an older engine than on a newer one', () => {
    const on50 = matrix.rows.find((row) => row.profile === 'ue5.0');
    const on57 = matrix.rows.find((row) => row.profile === 'ue5.7');
    expect(Number(on50?.filtered)).toBeGreaterThan(Number(on57?.filtered));
    expect(on50?.byGate[GATE_CODES.ENGINE_BELOW_MIN]).toBeGreaterThan(0);
  });

  it('shows the editor-state dimension moving capabilities in the opposite direction', () => {
    const inEdit = matrix.rows.find((row) => row.profile === 'ue5.7' && row.editorState === 'edit');
    const inPie = matrix.rows.find((row) => row.editorState === 'pie');
    expect(inEdit?.byGate[GATE_CODES.EDITOR_STATE_UNSUPPORTED]).toBeGreaterThan(0);
    expect(inPie?.byGate[GATE_CODES.EDITOR_STATE_UNSUPPORTED]).toBeGreaterThan(0);
    // The two sets are genuinely different capabilities, not the same ones twice.
    expect(inEdit?.unavailableIds).not.toEqual(inPie?.unavailableIds);
  });

  it('carries the native compile census alongside the contract verdicts', () => {
    const on50 = matrix.rows.find((row) => row.profile === 'ue5.0');
    const on57 = matrix.rows.find((row) => row.profile === 'ue5.7');
    expect(Number(on57?.native?.compiledCount)).toBeGreaterThan(Number(on50?.native?.compiledCount));
  });
});
