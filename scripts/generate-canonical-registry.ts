// scripts/generate-canonical-registry.ts
//
// Deterministic Task-23 canonical registry generator (thin entrypoint).
//
// Authoritative inputs:
//   - scripts/qa/capability-metadata-audit.ts#loadAllCapabilityRecords (1,335 records)
//     -- the EXCLUSIVE source for the parent surface (name/category/description
//     from record parent metadata; action enum from record legacyIds; input and
//     output schemas as permissive unions of exact per-action record properties)
//   - scripts/canonical-registry/parent-derivation.ts#deriveParents (record-only
//     derivation; the generated parent file is NOT an input, so there is no
//     circular bootstrap through it)
//   - scripts/canonical-registry/grouping.ts#PARENT_GROUPS (23 -> 13 shard plan)
//
// Emits (all generated, never hand-edited):
//   TS data           src/tools/catalog/capabilities/generated/canonical-registry.generated.ts
//   neutral JSON      src/tools/catalog/capabilities/generated/canonical-registry.generated.json
//   parent defs       src/tools/catalog/capabilities/generated/parent-tool-definitions.generated.ts
//   routing index     src/tools/orchestration/generated-routing-index.generated.ts
//   native aggregator plugins/.../Private/MCP/Tools/McpGeneratedParentRegistry.h + .cpp
//   native shards     plugins/.../Private/MCP/Tools/McpGeneratedParentRegistry_<Group>.cpp (x13)
//
// Run:
//   node --loader ts-node/esm scripts/generate-canonical-registry.ts [--check]
//
// --check fails (exit 1) if any generated target drifts from its
// source-derived content. Generation is atomic (staged temp files, then rename).
// Malformed / incomplete source fails before any write.

import { existsSync, readFileSync, writeFileSync, mkdirSync, renameSync, chmodSync, rmSync } from 'node:fs';
import { randomUUID } from 'node:crypto';
import { dirname } from 'node:path';

process.on('uncaughtException', (err) => {
  console.error('UNCAUGHT:', err);
  process.exit(1);
});
process.on('unhandledRejection', (reason) => {
  console.error('UNHANDLED REJECTION:', reason);
  process.exit(1);
});

// Heavy modules are imported dynamically to avoid a ts-node/esm top-level
// evaluation quirk when this script is the entry point.
async function loadModules(): Promise<{
  loadAllCapabilityRecords: () => readonly CapabilityRecord[];
  migrationMap: { readonly entries: ReadonlyMap<string, { readonly canonicalId?: string | null; readonly disposition: string }> };
  generateAliases: () => {
    readonly aliases: ReadonlyArray<{ alias: string; canonicalId: string; source: string }>;
    readonly conflicts: ReadonlyArray<{ alias: string; canonicalIds: readonly string[] }>;
  };
}> {
  const [qa, migration, aliasMod] = await Promise.all([
    import('./qa/capability-metadata-audit.js'),
    import('../src/tools/catalog/capabilities/migration/migration-map.js'),
    import('../src/tools/catalog/capabilities/migration/alias-generation.js'),
  ]);
  return {
    loadAllCapabilityRecords: qa.loadAllCapabilityRecords,
    migrationMap: migration.migrationMap,
    generateAliases: aliasMod.generateAliases,
  };
}

import type { CapabilityRecord } from '../src/tools/catalog/capabilities/model.js';

import { type CanonicalRecordSummary } from './canonical-registry/types.js';
import { compareSummaryDrift } from './canonical-registry/summary-drift.js';
import { assertGroupingComplete } from './canonical-registry/grouping.js';
import { deriveParents } from './canonical-registry/parent-derivation.js';
import {
  buildTargets,
  listStaleCapabilityShardHeaders,
  type ManifestTarget,
} from './canonical-registry/targets.js';

// ---------------------------------------------------------------------------
// Target assembly is delegated to canonical-registry/targets.ts, which owns
// every generated artifact (TS data, neutral JSON, parent defs, routing index,
// native parent registry, and the generated native capability shards + index).
// The 23 parents are DERIVED EXCLUSIVELY from the records (no hand-authored
// base / allToolDefinitions / generated artifact is an input), so the
// generator's bootstrap stays acyclic with its own generated output.
// Malformed / duplicate / missing input fails inside buildTargets before any
// content is produced, so the generator never emits a partial set.
// ---------------------------------------------------------------------------
function buildManifest(mods: Awaited<ReturnType<typeof loadModules>>): ManifestTarget[] {
  const records = mods.loadAllCapabilityRecords();
  const parents = deriveParents(records);
  // Fail before any write if the 23 parents do not map cleanly onto the plan.
  assertGroupingComplete(parents);
  return buildTargets({
    records,
    migrationMap: mods.migrationMap,
    generateAliases: mods.generateAliases,
  });
}

// ---------------------------------------------------------------------------
// Atomic writer + drift checker.
// ---------------------------------------------------------------------------
function checkDrift(targets: readonly ManifestTarget[]): boolean {
  let drift = false;
  for (const path of listStaleCapabilityShardHeaders()) {
    console.error(`[canonical-registry] DRIFT: ${path} is a superseded shard header. Run scripts/generate-canonical-registry.ts`);
    drift = true;
  }
  for (const [path, content] of targets) {
    if (!existsSync(path)) {
      console.error(`[canonical-registry] DRIFT: ${path} is missing. Run scripts/generate-canonical-registry.ts`);
      drift = true;
      continue;
    }
    const existing = readFileSync(path, 'utf8');
    if (existing !== content) {
      console.error(`[canonical-registry] DRIFT: ${path} is stale. Run scripts/generate-canonical-registry.ts`);
      drift = true;
    }
  }
  return drift;
}

export interface RegistryDriftEntry {
  readonly id: string;
  readonly pointer: string;
}

// Validated boundary shape for the neutral canonical-registry JSON. Input is
// `unknown` (untrusted comparison payload); it is parsed exactly once at the
// boundary and rejected if it is not the expected object contract. No `as any`
// / `as unknown` past this point -- everything downstream is typed.
interface CanonicalRegistryModel {
  readonly catalogRevision: string;
  readonly recordCount: number;
  readonly summaries: readonly CanonicalRecordSummary[];
  readonly lexicalIndex: Readonly<Record<string, readonly string[]>>;
  readonly migrationData: unknown;
  readonly aliasData: unknown;
  readonly docsData: unknown;
}

function parseRegistry(value: unknown): CanonicalRegistryModel | null {
  if (typeof value !== 'object' || value === null) return null;
  const obj = value as Record<string, unknown>;
  if (
    typeof obj.catalogRevision !== 'string' ||
    typeof obj.recordCount !== 'number' ||
    !Array.isArray(obj.summaries) ||
    typeof obj.lexicalIndex !== 'object' ||
    obj.lexicalIndex === null
  ) {
    return null;
  }
  return {
    catalogRevision: obj.catalogRevision,
    recordCount: obj.recordCount,
    summaries: obj.summaries as CanonicalRecordSummary[],
    lexicalIndex: obj.lexicalIndex as Readonly<Record<string, readonly string[]>>,
    migrationData: obj.migrationData,
    aliasData: obj.aliasData,
    docsData: obj.docsData,
  };
}

// Deterministic JSON-pointer reporting for an arbitrary value difference. Walks
// objects/arrays in canonical (sorted-key / index) order and recurses into
// nested objects/arrays so the FIRST leaf mismatch is reported with an exact,
// stable, reproducible pointer.
function pushPointerDiff(
  entries: RegistryDriftEntry[],
  id: string,
  pointer: string,
  expected: unknown,
  actual: unknown,
): void {
  const eType = typeof expected;
  const aType = typeof actual;
  const isObj = (v: unknown): v is Record<string, unknown> | unknown[] =>
    typeof v === 'object' && v !== null;
  if (eType !== aType || !isObj(expected) || !isObj(actual)) {
    entries.push({ id, pointer });
    return;
  }
  const eIsArr = Array.isArray(expected);
  const aIsArr = Array.isArray(actual);
  if (eIsArr !== aIsArr) {
    entries.push({ id, pointer });
    return;
  }
  if (eIsArr) {
    const eArr = expected as unknown[];
    const aArr = actual as unknown[];
    const len = Math.min(eArr.length, aArr.length);
    for (let i = 0; i < len; i += 1) {
      const child = `${pointer}/${i}`;
      if (isObj(eArr[i]) && isObj(aArr[i])) {
        pushPointerDiff(entries, id, child, eArr[i], aArr[i]);
        if (entries.length > 0) return;
      } else if (JSON.stringify(eArr[i]) !== JSON.stringify(aArr[i])) {
        entries.push({ id, pointer: child });
        return;
      }
    }
    if (eArr.length !== aArr.length) {
      entries.push({ id, pointer: `${pointer}/${eArr.length}` });
    }
    return;
  }
  const eObj = expected as Record<string, unknown>;
  const aObj = actual as Record<string, unknown>;
  const eKeys = Object.keys(eObj).sort();
  const aKeys = Object.keys(aObj).sort();
  for (const key of eKeys) {
    if (!(key in aObj)) {
      entries.push({ id, pointer: `${pointer}/${key}` });
      return;
    }
    const child = `${pointer}/${key}`;
    if (isObj(eObj[key]) && isObj(aObj[key])) {
      pushPointerDiff(entries, id, child, eObj[key], aObj[key]);
      if (entries.length > 0) return;
    } else if (JSON.stringify(eObj[key]) !== JSON.stringify(aObj[key])) {
      entries.push({ id, pointer: child });
      return;
    }
  }
  for (const key of aKeys) {
    if (!(key in eObj)) {
      entries.push({ id, pointer: `${pointer}/${key}` });
      return;
    }
  }
}

// Drift detector over the neutral canonical-registry JSON.
//
// Returns the exact canonical id and JSON pointer of each top-level and
// record-level mismatch so a mutated output is reported deterministically
// (never a rubber-stamp). Comparison is field-aware: top-level scalar fields
// (catalogRevision, recordCount), the lexical index, and the migration /
// alias / docs payloads are compared by exact value + JSON pointer; the
// `summaries` list is compared per canonical id (preserving the existing
// summary pointer behavior) as well as for dropped records.
export function compareCanonicalRegistry(
  expected: unknown,
  actual: unknown,
): readonly RegistryDriftEntry[] {
  const entries: RegistryDriftEntry[] = [];
  const exp = parseRegistry(expected);
  const act = parseRegistry(actual);
  if (exp === null || act === null) {
    if (exp === null) entries.push({ id: 'registry', pointer: '/' });
    if (act === null) entries.push({ id: 'registry', pointer: '/' });
    return entries;
  }

  if (exp.catalogRevision !== act.catalogRevision) {
    entries.push({ id: 'catalogRevision', pointer: '/catalogRevision' });
  }
  if (exp.recordCount !== act.recordCount) {
    entries.push({ id: 'recordCount', pointer: '/recordCount' });
  }

  pushPointerDiff(entries, 'lexicalIndex', '/lexicalIndex', exp.lexicalIndex, act.lexicalIndex);
  pushPointerDiff(entries, 'migrationData', '/migrationData', exp.migrationData, act.migrationData);
  pushPointerDiff(entries, 'aliasData', '/aliasData', exp.aliasData, act.aliasData);
  pushPointerDiff(entries, 'docsData', '/docsData', exp.docsData, act.docsData);

  compareSummaryDrift(entries, exp.summaries, act.summaries);
  return entries;
}

function writeTargets(targets: readonly ManifestTarget[]): void {
  const staged: string[] = [];
  try {
    for (const [path, content] of targets) {
      const dir = dirname(path);
      if (!existsSync(dir)) mkdirSync(dir, { recursive: true });
      const tmp = `${path}.tmp-${randomUUID()}`;
      writeFileSync(tmp, content, { flag: 'wx', mode: 0o600 });
      staged.push(tmp);
    }
  } catch (error) {
    for (const tmp of staged) rmSync(tmp, { force: true });
    throw error;
  }
  targets.forEach(([path], i) => {
    renameSync(staged[i], path);
    chmodSync(path, 0o644);
  });
  // Only after every live target landed, so a failed generation never leaves
  // the tree with neither the superseded header nor its replacement.
  for (const stale of listStaleCapabilityShardHeaders()) {
    rmSync(stale, { force: true });
  }
}

async function main(): Promise<void> {
  const args = process.argv.slice(2);
  const isCheck = args.includes('--check');
  const mods = await loadModules();
  const targets = buildManifest(mods);
  if (isCheck) {
    if (checkDrift(targets)) {
      process.exitCode = 1;
    } else {
      console.log('[canonical-registry] check: all generated artifacts are up to date.');
    }
  } else {
    writeTargets(targets);
    console.log(`[canonical-registry] wrote ${targets.length} generated artifacts.`);
  }
}

if (process.argv[1]?.endsWith('generate-canonical-registry.ts')) {
  main().catch((error) => {
    console.error('[canonical-registry] FAILED:', error);
    process.exitCode = 1;
  });
}
