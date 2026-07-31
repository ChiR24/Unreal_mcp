// @ts-check
// tests/unit/engine-certification/profile-matrix.mjs
// Task 52 — the OFFLINE simulation of "what would this capability do on that setup?"
//
// Certification runs are expensive: a package, a compile and an editor per engine
// minor. Most of the questions people ask about version support do not need one.
// "Is convert_to_nanite reachable on 5.5?" and "does a 5.0 build compile the
// DataLayer path?" are decidable from the contract and the sources, with no
// engine present, and this adapter answers them.
//
// A PROFILE IS FOUR INDEPENDENT DIMENSIONS, and conflating any two of them
// produces a confident wrong answer:
//
//   engine     the version from Build.version — bounds the contract's min/max AND
//              decides which `#if ENGINE_MINOR_VERSION` branches compile.
//   plugins    which UE plugins are enabled. Most are `Optional` in the .uplugin,
//              so their absence is a normal configuration, not a broken install.
//   editorState edit / pie / simulate. A capability declared `["pie","simulate"]`
//              is genuinely unavailable in a plain editor session.
//   client     the transport and the negotiated protocol version. Native /mcp
//              implements exactly three versions; the TypeScript SDK accepts two
//              more. A profile pinned to 2024-11-05 can reach one and not the other.
//
// UNDECIDABLE IS A VERDICT. A native gate that depends on a build define the
// profile never stated is reported UNDECIDABLE, not unavailable. Answering
// "unavailable" would under-report support with exactly the confidence that
// answering "available" would over-report it, and the whole value of a simulation
// is that its output can be trusted without an engine to check it against.

import { existsSync, readFileSync, readdirSync, statSync } from 'node:fs';
import { join, relative } from 'node:path';

import { evaluateCompatibilityMacros, evaluateCondition } from './preprocessor-conditions.mjs';

/** Closed taxonomy of reasons a capability is not reachable on a profile. */
export const GATE_CODES = Object.freeze({
  ENGINE_BELOW_MIN: 'ENGINE_BELOW_MIN',
  ENGINE_ABOVE_MAX: 'ENGINE_ABOVE_MAX',
  PLUGIN_NOT_ENABLED: 'PLUGIN_NOT_ENABLED',
  EDITOR_STATE_UNSUPPORTED: 'EDITOR_STATE_UNSUPPORTED',
  CLIENT_PROTOCOL_UNSUPPORTED: 'CLIENT_PROTOCOL_UNSUPPORTED',
  NATIVE_FEATURE_NOT_COMPILED: 'NATIVE_FEATURE_NOT_COMPILED',
});

/** The native `/mcp` transport implements exactly these, newest first. */
export const NATIVE_PROTOCOL_VERSIONS = Object.freeze(['2025-11-25', '2025-06-18', '2025-03-26']);
/** The TypeScript SDK also accepts two legacy versions. */
export const STDIO_PROTOCOL_VERSIONS = Object.freeze([...NATIVE_PROTOCOL_VERSIONS, '2024-11-05', '2024-10-07']);

/**
 * UE plugins whose presence Build.cs turns into a compile-time define. Enabling
 * the plugin is what makes the native feature exist at all, which is why the
 * runtime-optional dimension has to reach the preprocessor and not stop at the
 * contract's `requiredPlugins` list.
 */
export const PLUGIN_DEFINES = Object.freeze({
  PCG: 'MCP_HAS_PCG',
  MovieRenderPipeline: 'MCP_HAS_MOVIE_RENDER_PIPELINE',
  MoviePipelineMaskRenderPass: 'MCP_HAS_MOVIE_PIPELINE_OBJECT_ID_PASS',
  Takes: 'MCP_HAS_TAKE_RECORDER',
  ElectraPlayer: 'MCP_HAS_MEDIA_ASSETS',
  CinematicCamera: 'MCP_HAS_CINEMATIC_CAMERA',
  ControlRig: 'MCP_HAS_CONTROLRIG_BLUEPRINT',
});

/** @typedef {{ major: number, minor: number, patch: number, channel?: string, preview?: number|null }} EngineVersion */

/** @param {EngineVersion} left @param {EngineVersion} right */
export function compareEngineVersions(left, right) {
  for (const key of /** @type {const} */ (['major', 'minor', 'patch'])) {
    const a = Number(left[key] ?? 0);
    const b = Number(right[key] ?? 0);
    if (a !== b) return a < b ? -1 : 1;
  }
  return 0;
}

/**
 * Build a profile, filling in the build defines the enabled plugins imply.
 * @param {{ id: string, engine: EngineVersion, plugins?: readonly string[],
 *   editorState?: string, client?: { transport: string, protocolVersion: string },
 *   buildDefines?: Record<string, number> }} spec
 */
export function defineProfile(spec) {
  const plugins = [...(spec.plugins ?? [])];
  /** @type {Record<string, number>} */
  const defines = {
    ENGINE_MAJOR_VERSION: spec.engine.major,
    ENGINE_MINOR_VERSION: spec.engine.minor,
    ENGINE_PATCH_VERSION: spec.engine.patch,
    WITH_EDITOR: 1,
    WITH_EDITORONLY_DATA: 1,
  };
  for (const [plugin, macro] of Object.entries(PLUGIN_DEFINES)) {
    defines[macro] = plugins.includes(plugin) ? 1 : 0;
  }
  return {
    id: spec.id,
    engine: { channel: 'stable', preview: null, ...spec.engine },
    plugins,
    editorState: spec.editorState ?? 'edit',
    client: spec.client ?? { transport: 'native', protocolVersion: '2025-06-18' },
    buildDefines: { ...defines, ...(spec.buildDefines ?? {}) },
  };
}

/**
 * Decide one capability record against one profile.
 * @param {any} record a canonical registry record
 * @param {ReturnType<typeof defineProfile>} profile
 */
export function evaluateCapability(record, profile) {
  /** @type {Array<{ code: string, detail: string }>} */
  const gates = [];
  const availability = record.availability ?? {};
  const min = availability.unreal?.min;
  const max = availability.unreal?.max;
  if (min !== undefined && compareEngineVersions(profile.engine, min) < 0) {
    gates.push({ code: GATE_CODES.ENGINE_BELOW_MIN, detail: `${record.id} needs UE >= ${min.major}.${min.minor}.${min.patch}` });
  }
  if (max !== undefined && compareEngineVersions(profile.engine, max) > 0) {
    gates.push({ code: GATE_CODES.ENGINE_ABOVE_MAX, detail: `${record.id} is declared up to UE ${max.major}.${max.minor}.${max.patch}` });
  }
  for (const plugin of availability.requiredPlugins ?? []) {
    if (!profile.plugins.includes(plugin)) {
      gates.push({ code: GATE_CODES.PLUGIN_NOT_ENABLED, detail: `${record.id} needs the ${plugin} plugin, which this profile does not enable` });
    }
  }
  const states = availability.editorStates ?? [];
  if (states.length > 0 && !states.includes(profile.editorState)) {
    gates.push({ code: GATE_CODES.EDITOR_STATE_UNSUPPORTED, detail: `${record.id} runs in ${states.join('/')}, not in "${profile.editorState}"` });
  }
  const supported = profile.client.transport === 'native' ? NATIVE_PROTOCOL_VERSIONS : STDIO_PROTOCOL_VERSIONS;
  if (!supported.includes(profile.client.protocolVersion)) {
    gates.push({
      code: GATE_CODES.CLIENT_PROTOCOL_UNSUPPORTED,
      detail: `the ${profile.client.transport} surface does not implement protocol ${profile.client.protocolVersion}`,
    });
  }
  return { id: record.id, available: gates.length === 0, gates };
}

/** Source extensions worth scanning for a version gate. @param {string} root */
function sourceFiles(root) {
  /** @type {string[]} */
  const found = [];
  /** @param {string} dir */
  const walk = (dir) => {
    for (const entry of readdirSync(dir, { withFileTypes: true })) {
      const path = join(dir, entry.name);
      if (entry.isDirectory()) {
        walk(path);
        continue;
      }
      if (/\.(?:h|cpp|inl)$/u.test(entry.name)) found.push(path);
    }
  };
  if (existsSync(root) && statSync(root).isDirectory()) walk(root);
  return found.sort();
}

export const COMPATIBILITY_HEADER = 'Private/Core/Compatibility/McpVersionCompatibility.h';

/**
 * Read the plugin's REAL engine gates out of its sources.
 *
 * Deliberately not a hand-maintained table of thresholds: a table is a second
 * copy of the rules, and it keeps answering with the old rule the day a domain
 * file moves a gate.
 * @param {{ pluginRoot: string }} spec
 */
export function collectNativeGates(spec) {
  const moduleRoot = join(spec.pluginRoot, 'Source/McpAutomationBridge');
  /** @type {Array<{ file: string, line: number, condition: string }>} */
  const conditions = [];
  for (const file of sourceFiles(join(moduleRoot, 'Private'))) {
    const text = readFileSync(file, 'utf8');
    if (!text.includes('ENGINE_MINOR_VERSION')) continue;
    for (const [index, line] of text.split('\n').entries()) {
      const match = /^\s*#\s*(?:if|elif)\s+(.*ENGINE_(?:MAJOR|MINOR)_VERSION.*)$/u.exec(line);
      if (match === null) continue;
      conditions.push({ file: relative(spec.pluginRoot, file), line: index + 1, condition: String(match[1]).split('//')[0].trim() });
    }
  }
  const headerPath = join(moduleRoot, COMPATIBILITY_HEADER);
  return {
    compatibilityHeader: existsSync(headerPath) ? headerPath : null,
    compatibilityHeaderText: existsSync(headerPath) ? readFileSync(headerPath, 'utf8') : null,
    conditions,
    distinctConditions: [...new Set(conditions.map((entry) => entry.condition))].sort(),
  };
}

/**
 * Which native gates compile in for this profile, and which cannot be decided.
 * @param {ReturnType<typeof defineProfile>} profile
 * @param {ReturnType<typeof collectNativeGates>} gates
 */
export function evaluateNativeFeatures(profile, gates) {
  const macros = gates.compatibilityHeaderText === null
    ? { macros: { ...profile.buildDefines }, undecided: [], unbalanced: 0 }
    : evaluateCompatibilityMacros(gates.compatibilityHeaderText, profile.buildDefines);
  /** @type {Array<{ file: string, line: number, condition: string }>} */
  const undecided = [];
  /** @type {Array<{ file: string, line: number, condition: string }>} */
  const compiled = [];
  /** @type {Array<{ file: string, line: number, condition: string }>} */
  const excluded = [];
  for (const entry of gates.conditions) {
    const verdict = evaluateCondition(entry.condition, macros.macros);
    if (verdict === null) undecided.push(entry);
    else if (verdict) compiled.push(entry);
    else excluded.push(entry);
  }
  return {
    macros: macros.macros,
    macroUndecided: macros.undecided,
    compiledCount: compiled.length,
    excludedCount: excluded.length,
    undecidedCount: undecided.length,
    compiled, excluded, undecided,
  };
}

/**
 * The full matrix: every profile against every record, plus the native gate census.
 * @param {{ records: readonly any[], profiles: readonly ReturnType<typeof defineProfile>[],
 *   nativeGates?: ReturnType<typeof collectNativeGates>|null }} spec
 */
export function buildProfileMatrix(spec) {
  const rows = spec.profiles.map((profile) => {
    const verdicts = spec.records.map((record) => evaluateCapability(record, profile));
    /** @type {Record<string, number>} */
    const byGate = {};
    for (const verdict of verdicts) {
      for (const gate of verdict.gates) byGate[gate.code] = (byGate[gate.code] ?? 0) + 1;
    }
    return {
      profile: profile.id,
      engine: `${profile.engine.major}.${profile.engine.minor}.${profile.engine.patch}`,
      editorState: profile.editorState,
      transport: profile.client.transport,
      protocolVersion: profile.client.protocolVersion,
      pluginCount: profile.plugins.length,
      total: verdicts.length,
      available: verdicts.filter((verdict) => verdict.available).length,
      filtered: verdicts.filter((verdict) => !verdict.available).length,
      byGate,
      unavailableIds: verdicts.filter((verdict) => !verdict.available).map((verdict) => verdict.id),
      native: spec.nativeGates == null ? null : (() => {
        const { compiled: _c, excluded: _e, undecided: _u, ...summary } = evaluateNativeFeatures(profile, spec.nativeGates);
        return summary;
      })(),
    };
  });
  return { profiles: spec.profiles.map((profile) => profile.id), rows };
}
