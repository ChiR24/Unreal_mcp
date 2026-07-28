/**
 * Task 43 regression gate: an UNDO_EVIDENCE entry may not cite a handler that
 * writes a package to disk.
 *
 * The defect this exists to prevent was invisible to grep. The ledger claimed
 * the seven Blueprint-graph mutations "never call McpSafeAssetSave or
 * SavePackage" -- true by name, false in effect, because they call
 * `SaveLoadedAssetThrottled`, which calls `McpSafeAssetSave`. So this file does
 * NOT grep for known persistence names. It seeds only the DIRECT package
 * writers, then derives every alias by walking the plugin call graph to a fixed
 * point, and finally asks whether the cited handler can reach one. A future
 * wrapper with a new name is caught the same way this one is.
 */
import { readFileSync, readdirSync } from 'node:fs';
import { join, resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

import { UNDO_EVIDENCE } from '../../../src/tools/catalog/capabilities/records/semantics/evidence-ledger.js';

const PLUGIN_PRIVATE = resolve(
  process.cwd(),
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private'
);

/**
 * Seeds name ONLY functions that write a package themselves. Every wrapper --
 * `SaveLoadedAssetThrottled` included -- has to be discovered by derivation, or
 * this gate would repeat the exact blind spot it was written for.
 */
const DIRECT_PACKAGE_WRITERS = [
  'McpSafeAssetSave',
  'McpSafeLevelSave',
  'SavePackage',
  'SavePackagesForObjects',
  'PromptForCheckoutAndSave'
] as const;

/** Control-flow keywords that look like calls to a name-then-paren scan. */
const NOT_A_CALL = new Set([
  'if',
  'for',
  'while',
  'switch',
  'catch',
  'return',
  'sizeof',
  'alignof',
  'decltype',
  'static_assert',
  'throw',
  'new',
  'delete',
  'case',
  'else',
  'do',
  'template',
  'operator'
]);

const TRAILING_QUALIFIER = /^(?:const|noexcept|override|final|mutable)\b/u;

type CallSite = { readonly name: string; readonly line: number };

type FunctionRange = {
  readonly name: string;
  readonly file: string;
  readonly startLine: number;
  readonly endLine: number;
  readonly callSites: readonly CallSite[];
};

/**
 * Blanks comments and the contents of string / char literals while preserving
 * every byte offset, so a `{` inside `TEXT("...")` cannot derail brace matching
 * and the line numbers reported still address the real file.
 */
function maskLiterals(source: string): string {
  const out = source.split('');
  const n = source.length;
  let i = 0;
  const blank = (at: number): void => {
    if (source[at] !== '\n') out[at] = ' ';
  };
  while (i < n) {
    const c = source[i];
    const next = source[i + 1];
    if (c === '/' && next === '/') {
      while (i < n && source[i] !== '\n') {
        blank(i);
        i += 1;
      }
      continue;
    }
    if (c === '/' && next === '*') {
      blank(i);
      blank(i + 1);
      i += 2;
      while (i < n && !(source[i] === '*' && source[i + 1] === '/')) {
        blank(i);
        i += 1;
      }
      blank(i);
      blank(i + 1);
      i += 2;
      continue;
    }
    if (c === '"' || c === "'") {
      blank(i);
      i += 1;
      while (i < n && source[i] !== c) {
        if (source[i] === '\\') {
          blank(i);
          i += 1;
        }
        blank(i);
        i += 1;
      }
      blank(i);
      i += 1;
      continue;
    }
    i += 1;
  }
  return out.join('');
}

function buildLineStarts(text: string): number[] {
  const starts = [0];
  for (let i = 0; i < text.length; i += 1) {
    if (text[i] === '\n') starts.push(i + 1);
  }
  return starts;
}

function lineOf(lineStarts: readonly number[], offset: number): number {
  let low = 0;
  let high = lineStarts.length - 1;
  while (low < high) {
    const mid = (low + high + 1) >> 1;
    if ((lineStarts[mid] ?? 0) <= offset) low = mid;
    else high = mid - 1;
  }
  return low + 1;
}

function matchClosing(text: string, openIndex: number, open: string, close: string): number {
  let depth = 0;
  for (let i = openIndex; i < text.length; i += 1) {
    if (text[i] === open) depth += 1;
    else if (text[i] === close) {
      depth -= 1;
      if (depth === 0) return i;
    }
  }
  return -1;
}

const NAME_THEN_PAREN = /([A-Za-z_]\w*)\s*\(/gu;

function extractCallSites(masked: string, lineStarts: readonly number[], from: number, to: number): CallSite[] {
  const sites: CallSite[] = [];
  const scan = new RegExp(NAME_THEN_PAREN.source, 'gu');
  const body = masked.slice(from, to);
  let match = scan.exec(body);
  while (match !== null) {
    const name = match[1] ?? '';
    if (!NOT_A_CALL.has(name)) {
      sites.push({ name, line: lineOf(lineStarts, from + match.index) });
    }
    match = scan.exec(body);
  }
  return sites;
}

function extractFunctions(file: string, masked: string, lineStarts: readonly number[]): FunctionRange[] {
  const ranges: FunctionRange[] = [];
  const scan = new RegExp(NAME_THEN_PAREN.source, 'gu');
  let match = scan.exec(masked);
  while (match !== null) {
    const name = match[1] ?? '';
    if (!NOT_A_CALL.has(name)) {
      const parenOpen = match.index + match[0].length - 1;
      const parenClose = matchClosing(masked, parenOpen, '(', ')');
      if (parenClose > 0) {
        let cursor = parenClose + 1;
        for (;;) {
          while (cursor < masked.length && /\s/u.test(masked[cursor] ?? '')) cursor += 1;
          const qualifier = TRAILING_QUALIFIER.exec(masked.slice(cursor, cursor + 12));
          if (!qualifier) break;
          cursor += qualifier[0].length;
        }
        if (masked[cursor] === '{') {
          const bodyEnd = matchClosing(masked, cursor, '{', '}');
          if (bodyEnd > 0) {
            ranges.push({
              name,
              file,
              startLine: lineOf(lineStarts, match.index),
              endLine: lineOf(lineStarts, bodyEnd),
              callSites: extractCallSites(masked, lineStarts, cursor + 1, bodyEnd)
            });
          }
        }
      }
    }
    match = scan.exec(masked);
  }
  return ranges;
}

function collectSources(dir: string, out: string[]): string[] {
  for (const entry of readdirSync(dir, { withFileTypes: true })) {
    const full = join(dir, entry.name);
    if (entry.isDirectory()) collectSources(full, out);
    else if (/\.(?:h|cpp|inl)$/u.test(entry.name)) out.push(full);
  }
  return out;
}

type PluginGraph = {
  readonly functions: readonly FunctionRange[];
  readonly byFile: ReadonlyMap<string, readonly FunctionRange[]>;
  readonly persists: ReadonlySet<string>;
  readonly derived: ReadonlySet<string>;
};

function buildGraph(): PluginGraph {
  const functions: FunctionRange[] = [];
  const byFile = new Map<string, FunctionRange[]>();
  for (const file of collectSources(PLUGIN_PRIVATE, [])) {
    const source = readFileSync(file, 'utf8');
    const masked = maskLiterals(source);
    const lineStarts = buildLineStarts(masked);
    const ranges = extractFunctions(file, masked, lineStarts);
    functions.push(...ranges);
    byFile.set(file, ranges);
  }

  // callee name -> names of functions that call it.
  const callers = new Map<string, Set<string>>();
  for (const fn of functions) {
    for (const site of fn.callSites) {
      let bucket = callers.get(site.name);
      if (!bucket) {
        bucket = new Set<string>();
        callers.set(site.name, bucket);
      }
      bucket.add(fn.name);
    }
  }

  const persists = new Set<string>(DIRECT_PACKAGE_WRITERS);
  const derived = new Set<string>();
  const queue = [...persists];
  while (queue.length > 0) {
    const name = queue.pop() as string;
    for (const caller of callers.get(name) ?? []) {
      if (persists.has(caller)) continue;
      persists.add(caller);
      derived.add(caller);
      queue.push(caller);
    }
  }

  return { functions, byFile, persists, derived };
}

const graph = buildGraph();

/** Innermost indexed function containing `line`, or null when nothing matches. */
function enclosingFunction(file: string, line: number): FunctionRange | null {
  const ranges = graph.byFile.get(file) ?? [];
  let best: FunctionRange | null = null;
  for (const range of ranges) {
    if (line < range.startLine || line > range.endLine) continue;
    if (!best || range.endLine - range.startLine < best.endLine - best.startLine) best = range;
  }
  return best;
}

function resolveCitation(citation: string): { readonly file: string; readonly line: number } {
  const split = citation.lastIndexOf(':');
  const path = citation.slice(0, split);
  const line = Number.parseInt(citation.slice(split + 1), 10);
  return { file: resolve(process.cwd(), path), line };
}

const GRAPH_DIR = 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/BlueprintGraph';

/**
 * The seven claims this gate was written for, at the transaction lines the
 * ledger used to cite. They are the positive control: with UNDO_EVIDENCE empty
 * the gate below is vacuous, so the detector has to prove itself against known
 * defects or a broken parser would look like a pass.
 */
const HISTORICAL_FALSE_UNDO_CLAIMS = [
  { id: 'blueprint.delete_node', file: `${GRAPH_DIR}/McpAutomationBridge_BlueprintGraphHandlersNodeMutations.cpp`, line: 40, direct: true },
  { id: 'blueprint.create_reroute_node', file: `${GRAPH_DIR}/McpAutomationBridge_BlueprintGraphHandlersNodeMutations.cpp`, line: 63, direct: true },
  { id: 'blueprint.set_node_property', file: `${GRAPH_DIR}/McpAutomationBridge_BlueprintGraphHandlersNodeMutations.cpp`, line: 100, direct: true },
  { id: 'blueprint.connect_pins', file: `${GRAPH_DIR}/McpAutomationBridge_BlueprintGraphHandlersPinMutations.cpp`, line: 16, direct: true },
  { id: 'blueprint.break_pin_links', file: `${GRAPH_DIR}/McpAutomationBridge_BlueprintGraphHandlersPinMutations.cpp`, line: 124, direct: true },
  { id: 'blueprint.set_pin_default_value', file: `${GRAPH_DIR}/McpAutomationBridge_BlueprintGraphHandlersPinMutations.cpp`, line: 192, direct: true },
  // The one that hid: the save is several frames below the transaction.
  { id: 'blueprint.create_node', file: `${GRAPH_DIR}/McpAutomationBridge_BlueprintGraphHandlersNodeCreation.cpp`, line: 85, direct: false }
] as const;

describe('UNDO_EVIDENCE durability contract (Task 43)', () => {
  it('indexes the plugin call graph', () => {
    expect(graph.functions.length).toBeGreaterThan(500);
  });

  it('derives SaveLoadedAssetThrottled as a package writer instead of hard-coding it', () => {
    // If this ever needs to be added to the seed list, the derivation is broken
    // and every alias-shaped defect walks straight through the gate.
    expect(DIRECT_PACKAGE_WRITERS).not.toContain('SaveLoadedAssetThrottled');
    expect(graph.derived.has('SaveLoadedAssetThrottled')).toBe(true);
  });

  it('flags every historically false undo claim (positive control)', () => {
    const undetected: string[] = [];
    for (const claim of HISTORICAL_FALSE_UNDO_CLAIMS) {
      const fn = enclosingFunction(resolve(process.cwd(), claim.file), claim.line);
      if (!fn || !graph.persists.has(fn.name)) undetected.push(claim.id);
    }
    expect(undetected).toEqual([]);
  });

  it('sees the save land between the transaction opening and the scope closing', () => {
    const notInsideScope: string[] = [];
    for (const claim of HISTORICAL_FALSE_UNDO_CLAIMS) {
      if (!claim.direct) continue;
      const file = resolve(process.cwd(), claim.file);
      const fn = enclosingFunction(file, claim.line);
      const source = readFileSync(file, 'utf8').split(/\r?\n/u);
      const opensTransaction = (source[claim.line - 1] ?? '').includes('FScopedTransaction');
      const savesInside = (fn?.callSites ?? []).some(
        (site) => graph.persists.has(site.name) && site.line > claim.line && site.line <= (fn?.endLine ?? 0)
      );
      if (!opensTransaction || !savesInside) notInsideScope.push(claim.id);
    }
    expect(notInsideScope).toEqual([]);
  });

  it('reaches create_node through a callee rather than a direct save call', () => {
    const claim = HISTORICAL_FALSE_UNDO_CLAIMS.find((entry) => !entry.direct);
    const fn = enclosingFunction(resolve(process.cwd(), claim?.file ?? ''), claim?.line ?? 0);
    expect(fn?.name).toBe('HandleNodeCreationAction');
    // A name-only grep of this file finds no save; the path is transitive.
    expect((fn?.callSites ?? []).some((site) => site.name === 'SaveLoadedAssetThrottled')).toBe(false);
    expect(graph.persists.has(fn?.name ?? '')).toBe(true);
  });

  it('refuses an undo claim whose cited handler can reach a package write', () => {
    const offenders: string[] = [];
    for (const [id, entry] of UNDO_EVIDENCE) {
      const { file, line } = resolveCitation(entry.citation);
      const fn = enclosingFunction(file, line);
      if (!fn) {
        // Fail closed: an unresolvable citation proves nothing.
        offenders.push(`${id} (citation does not resolve to a function: ${entry.citation})`);
        continue;
      }
      if (graph.persists.has(fn.name)) {
        offenders.push(`${id} (${fn.name} reaches a package write; undo cannot revert bytes on disk)`);
      }
    }
    expect(offenders).toEqual([]);
  });
});
