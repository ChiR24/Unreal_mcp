// tests/eval/fixtures.ts
// Deterministic populations, hashes and percentile maths shared by every
// Task-48 measurement module.
//
// Two rules make the report auditable rather than merely reproducible:
//   1. The population is the FINAL registry the gateway actually serves
//      (`capabilityIndex()`), not the 493-record Task-13 pilot subset. Task 48
//      gates the shipping surface, so it must measure the shipping surface.
//   2. Every input that can move a deterministic number is folded into
//      `treeHash()`. A report whose numbers changed but whose tree hash did not
//      would be a lie, so the hash covers the ranking code, the gateway
//      projections, the generated registry and the corpus data together.

import { createHash } from 'node:crypto';
import { type Dirent, readFileSync, readdirSync, statSync } from 'node:fs';
import { join, relative, sep } from 'node:path';
import type { CapabilityRecord } from '../../src/tools/catalog/capabilities/model.js';
import { capabilityIndex, resolveLegacyPair } from '../../src/server/gateway/gateway-capability-index.js';
import { canonicalCapabilityId } from '../../src/tools/catalog/capabilities/retrieval/alias-fold.js';
import { corpus } from './corpus.js';
import type { CapabilityRef, CorpusCase } from './types.js';

/** The gateway's own default `search` page. Task 48's "top-K" is that page. */
export const GATEWAY_DEFAULT_SEARCH_LIMIT = 12 as const;

export function finalRegistryRecords(): readonly CapabilityRecord[] {
  return capabilityIndex().records;
}

/**
 * Resolves a corpus reference to the id search actually answers in. A
 * rationale-declared alias is folded into its primary, so comparing against the
 * alias id would score the ranker against an identity the catalog itself says
 * is the same capability. The relation comes from the registry's own fold, not
 * from the corpus, so this cannot be used to make a wrong answer look right.
 */
export function canonicalIdFor(reference: CapabilityRef): string | undefined {
  const resolved = resolveLegacyPair(reference.tool, reference.action);
  if (resolved.kind === 'unknown') return undefined;
  return canonicalCapabilityId(capabilityIndex().search.aliasFold, String(resolved.record.id));
}

export type RetrievalCase = {
  readonly id: string;
  readonly intent: string;
  readonly kind: CorpusCase['kind'];
  readonly expectedCapabilityId: string;
  readonly acceptedCapabilityIds: readonly string[];
};

/**
 * Corpus cases that assert a positive retrieval outcome. The two negative kinds
 * are deliberately excluded: their whole expectation is that the capability is
 * NOT returned, which the unavailable-filtering metric measures instead.
 */
export function retrievalCases(): readonly RetrievalCase[] {
  const cases: RetrievalCase[] = [];
  for (const entry of corpus.cases) {
    if (entry.kind === 'version_negative' || entry.kind === 'plugin_negative') continue;
    const expectedCapabilityId = canonicalIdFor(entry.expected);
    if (expectedCapabilityId === undefined) continue;
    const accepted = new Set<string>([expectedCapabilityId]);
    for (const alternative of entry.allowedAlternatives) {
      const id = canonicalIdFor(alternative);
      if (id !== undefined) accepted.add(id);
    }
    cases.push({
      id: entry.id,
      intent: entry.intent,
      kind: entry.kind,
      expectedCapabilityId,
      acceptedCapabilityIds: Object.freeze([...accepted].sort()),
    });
  }
  return Object.freeze(cases.sort((left, right) => (left.id < right.id ? -1 : left.id > right.id ? 1 : 0)));
}

export type Percentiles = {
  readonly sampleCount: number;
  readonly p50: number;
  readonly p75: number;
  readonly p90: number;
  readonly p95: number;
  readonly p99: number;
  readonly max: number;
};

function quantile(sorted: readonly number[], ratio: number): number {
  if (sorted.length === 0) return 0;
  const index = Math.min(sorted.length - 1, Math.max(0, Math.ceil(sorted.length * ratio) - 1));
  return sorted[index] ?? 0;
}

/** Nearest-rank percentiles, matching the Task-13 benchmark so the two agree. */
export function percentilesOf(samples: readonly number[]): Percentiles {
  const sorted = [...samples].sort((left, right) => left - right);
  return {
    sampleCount: sorted.length,
    p50: quantile(sorted, 0.5),
    p75: quantile(sorted, 0.75),
    p90: quantile(sorted, 0.9),
    p95: quantile(sorted, 0.95),
    p99: quantile(sorted, 0.99),
    max: sorted[sorted.length - 1] ?? 0,
  };
}

export function median(values: readonly number[]): number {
  if (values.length === 0) return 0;
  const sorted = [...values].sort((left, right) => left - right);
  const middle = Math.floor(sorted.length / 2);
  if (sorted.length % 2 === 1) return sorted[middle] ?? 0;
  return ((sorted[middle - 1] ?? 0) + (sorted[middle] ?? 0)) / 2;
}

export type EnvironmentDescriptor = {
  readonly nodeVersion: string;
  readonly v8Version: string;
  readonly platform: string;
  readonly arch: string;
};

export function environmentDescriptor(): EnvironmentDescriptor {
  return {
    nodeVersion: process.versions.node,
    v8Version: process.versions.v8,
    platform: process.platform,
    arch: process.arch,
  };
}

/**
 * Files whose bytes can move a deterministic Task-48 number. Directories are
 * walked in sorted order and `.test.ts` files are excluded, because a test edit
 * cannot change what the gateway returns.
 */
export const TREE_HASH_ROOTS: readonly string[] = Object.freeze([
  'src/tools/catalog/capabilities/retrieval',
  'src/tools/catalog/capabilities/records',
  'src/tools/catalog/capabilities/generated',
  'src/server/gateway',
  'src/gateway',
]);

export const TREE_HASH_FILES: readonly string[] = Object.freeze([
  'tests/eval/corpus.data.ts',
]);

function collect(root: string, projectRoot: string, into: string[]): void {
  let entries: Dirent<string>[];
  try {
    entries = readdirSync(join(projectRoot, root), { withFileTypes: true });
  } catch {
    return;
  }
  for (const entry of [...entries].sort((left, right) => (left.name < right.name ? -1 : 1))) {
    const child = `${root}/${entry.name}`;
    if (entry.isDirectory()) {
      collect(child, projectRoot, into);
      continue;
    }
    if (!entry.isFile()) continue;
    if (child.endsWith('.test.ts')) continue;
    into.push(child);
  }
}

export function treeHashInputs(projectRoot: string = process.cwd()): readonly string[] {
  const files: string[] = [];
  for (const root of TREE_HASH_ROOTS) collect(root, projectRoot, files);
  for (const file of TREE_HASH_FILES) {
    try {
      if (statSync(join(projectRoot, file)).isFile()) files.push(file);
    } catch {
      // A declared input that does not exist must not be silently hashed as
      // absent-equals-empty; it simply drops out and the hash changes, which is
      // exactly the drift signal we want.
    }
  }
  return Object.freeze([...new Set(files)].sort());
}

/** sha256 over (relativePath, sha256(bytes)) pairs, so a rename is drift too. */
export function treeHash(projectRoot: string = process.cwd()): string {
  const hash = createHash('sha256');
  for (const file of treeHashInputs(projectRoot)) {
    const bytes = readFileSync(join(projectRoot, file));
    const normalized = file.split(sep).join('/');
    hash.update(`${normalized}:${createHash('sha256').update(bytes).digest('hex')}\n`);
  }
  return hash.digest('hex');
}

export function relativeToProject(path: string, projectRoot: string = process.cwd()): string {
  return relative(projectRoot, path).split(sep).join('/');
}
