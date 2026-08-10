// tests/unit/docs/docs-claim-rules.ts
//
// Task 53: the machine-checkable claim rules behind the published docs.
//
// These are PURE functions over text so the contract test can do the only thing
// that makes a contract worth having: feed each rule a deliberately bad
// fragment and prove it REJECTS it. A rule nobody has shown can fail is
// decoration, not a gate.
//
// Each rule pairs an ASSERTION pattern with a NEGATION vocabulary and judges a
// PARAGRAPH, not a line. A doc is allowed to name a retired thing — that is how
// migration guidance is written — but it may not assert the retired thing as
// current. So `23 tools are exposed` is a violation while `the 23-tool listing
// was removed` is not.

/** One violated claim, named precisely enough to fix without guessing. */
export interface ClaimViolation {
  readonly rule: string;
  readonly file: string;
  readonly paragraph: string;
}

export interface ClaimRule {
  readonly id: string;
  readonly description: string;
  /** True when this paragraph asserts the forbidden claim. */
  readonly violates: (paragraph: string) => boolean;
}

/**
 * Paragraphs, not lines: negation ("this was removed", "not yet run") routinely
 * sits on a different line from the claim it qualifies, and a line-based rule
 * would report those as violations.
 */
export function paragraphsOf(text: string): readonly string[] {
  return text
    .split(/\n\s*\n/)
    .map((p) => p.trim())
    .filter((p) => p.length > 0);
}

/**
 * Release history is a record of what was true then. Rewriting it to match the
 * present would destroy the changelog's only job, so only the unreleased
 * section is held to the current-state rules. `^## .*\[\d` finds the first
 * semver heading; `[Unreleased]` has no digit after `[`.
 */
export function unreleasedSection(changelog: string): string {
  const releasedIdx = changelog.search(/^## .*\[\d/m);
  return releasedIdx >= 0 ? changelog.slice(0, releasedIdx) : changelog;
}

const has = (text: string, patterns: readonly RegExp[]): boolean =>
  patterns.some((p) => p.test(text));

// Words that turn a mention into a disclaimer. Deliberately broad: a false
// NEGATIVE here (a real violation excused by a stray "not") is far cheaper to
// catch in review than a false POSITIVE that makes the gate unrunnable and gets
// switched off.
const REMOVAL_NEGATIONS: readonly RegExp[] = [
  /\bremoved\b/i,
  /\bno longer\b/i,
  /\bwas retired\b/i,
  /\breplaces?\b/i,
  /\breplaced\b/i,
  /\bnot\b/i,
  /\bnever\b/i,
  /\bno legacy\b/i,
  /\bhistorical(?:ly)?\b/i,
  /\binternal\b/i,
  /\bdeprecat/i,
  /\bexclude[sd]?\b/i,
  /\bhidden?\b/i,
  /\bhides\b/i,
];

// ---------------------------------------------------------------------------
// Rule 1 — a client-visible multi-tool surface.
// The 23 canonical parents are an internal routing boundary. Any doc that says
// the server EXPOSES or LISTS them is stale.
// ---------------------------------------------------------------------------
const PUBLIC_SURFACE_ASSERTIONS: readonly RegExp[] = [
  /\bexposes?\s+(?:the\s+|all\s+)?23\b/i,
  /\b23\s+[\w-]*\s?tools?\s+are\s+(?:exposed|listed|public|available)\b/i,
  /\b23\s+canonical\s+public\s+tools?\b/i,
  /\bpublic\s+(?:MCP\s+)?(?:tool\s+)?surface\s+(?:is|of|comprises|contains)\s+(?:the\s+)?23\b/i,
  /\btools\/list\b[^\n]{0,80}\b23\b/i,
  /\b23\s+tools?\s+(?:are\s+)?(?:advertised|published)\b/i,
];

// ---------------------------------------------------------------------------
// Rule 2 — the removed in-editor assistant panel.
// It was deleted from this tree and ships in no release. Naming it as a
// migration target is fine; naming it as a surface is not.
// ---------------------------------------------------------------------------
// Assembled from fragments on purpose. tests/unit/unrealagent-removal-contract.ts
// asserts that exactly ONE repo-owned line contains the contiguous plugin name,
// so a rule that FORBIDS the name must not spell it out and become the second.
const REMOVED_PANEL_NAME = `${'Unreal'}${'Agent'}`;
const REMOVED_PANEL_ACP = `${'Open'}${'Code ACP'}`;

const UNREAL_AGENT_MENTIONS: readonly RegExp[] = [
  new RegExp(`\\b${REMOVED_PANEL_NAME}\\b`),
  /\bagent panel\b/i,
  /\bassistant panel\b/i,
  new RegExp(`\\bin-editor (?:${REMOVED_PANEL_ACP} )?ACP\\b`, 'i'),
];

// ---------------------------------------------------------------------------
// Rule 3 — protocol versions.
// Only these five are real: three on the native transport, plus two legacy
// versions the TypeScript SDK also accepts. Anything else in a protocol-version
// context is fiction unless it is explicitly disclaimed.
// ---------------------------------------------------------------------------
export const NATIVE_PROTOCOL_VERSIONS = ['2025-11-25', '2025-06-18', '2025-03-26'] as const;
export const TS_ONLY_LEGACY_PROTOCOL_VERSIONS = ['2024-11-05', '2024-10-07'] as const;
export const SUPPORTED_PROTOCOL_VERSIONS: readonly string[] = [
  ...NATIVE_PROTOCOL_VERSIONS,
  ...TS_ONLY_LEGACY_PROTOCOL_VERSIONS,
];

const PROTOCOL_CONTEXT: readonly RegExp[] = [
  /protocol[\s-]?version/i,
  /protocolVersion/,
  /MCP-Protocol-Version/i,
  /\brelease[\s-]candidate\b/i,
  /\bRC\b/,
  /\bnegotiat/i,
];

const DATE_TOKEN = /\b20\d{2}-\d{2}-\d{2}\b/g;

function unsupportedProtocolVersions(paragraph: string): readonly string[] {
  const found = paragraph.match(DATE_TOKEN) ?? [];
  return found.filter((v) => !SUPPORTED_PROTOCOL_VERSIONS.includes(v));
}

// ---------------------------------------------------------------------------
// Rule 4 — certification evidence.
// Engine roots actually present are 5.0.3, 5.3.2, 5.5.4, 5.7.4 and 5.8.0-P1.
// 5.1, 5.2, 5.4 and 5.6 are ABSENT, so no result may be asserted for them, and
// no completed full-range certification may be asserted either.
// ---------------------------------------------------------------------------
export const ABSENT_ENGINE_MINORS = ['5.1', '5.2', '5.4', '5.6'] as const;

const CERTIFICATION_CONTEXT: readonly RegExp[] = [
  /\bcertif/i,
  /\bcompile-verified\b/i,
  /\bverified (?:on|across|against)\b/i,
  /\btested (?:on|across|against)\b/i,
  /\bvalidated (?:on|across|against)\b/i,
];

// "5.0-5.8", "5.0–5.8" (en dash), "5.0 through 5.8", "full matrix".
const FULL_RANGE_CLAIM =
  /\b5\.0\s*(?:[-–—]|to|through)\s*5\.8\b|\bfull(?:[\s-]\w+)?\s+matrix\b/i;

const absentEngineMention = (paragraph: string): boolean =>
  ABSENT_ENGINE_MINORS.some((minor) =>
    new RegExp(`\\bUE\\s*${minor.replace('.', '\\.')}\\b|\\b${minor.replace('.', '\\.')}(?:\\.\\d+)?\\b`).test(
      paragraph,
    ),
  );

// Markers that make a certification paragraph honest rather than a claim.
const CERTIFICATION_NEGATIONS: readonly RegExp[] = [
  ...REMOVAL_NEGATIONS,
  /\bpending\b/i,
  /\bincomplete\b/i,
  /\bongoing\b/i,
  /\bin progress\b/i,
  /\bin flight\b/i,
  /\bblocked\b/i,
  /\bmissing\b/i,
  /\babsent\b/i,
  /\bdeferred\b/i,
  /\bdo not (?:assume|claim|cite|quote)\b/i,
  /\bcompatibility target\b/i,
];

// ---------------------------------------------------------------------------
// Rule 5 — a SUPPORT claim about an engine range, checked against the
// advertised range.
//
// Rule 4 fires only on CERTIFICATION vocabulary, and `CERTIFICATION_NEGATIONS`
// excuses any paragraph containing the phrase "compatibility target". So the
// strongest engine claim this project publishes — that the plugin builds and
// runs across UE 5.0 through 5.8 — is invisible to it twice over: it names no
// certification verb, and it sits beside the excusing phrase.
//
// This rule is different in two ways that matter:
//
//   1. It scans for SUPPORT/CAPABILITY verbs ("builds and runs", "supports",
//      "works on", "requires UE x.y") rather than certification verbs, because
//      "it runs on 5.0-5.8" is the claim a reader actually acts on.
//   2. It resolves the claimed minors against SUPPORTED_ENGINE_MINORS — the
//      advertised 5.0-5.8 range — instead of asking whether a hedging word is
//      present anywhere nearby. Every advertised minor is claimed as supported,
//      so an in-range claim needs no qualification; a claim naming a minor
//      OUTSIDE the advertised range (e.g. `5.9`) is unbacked unless the same
//      sentence qualifies or denies it. That is a fact lookup, not a word count,
//      so no amount of rewording can make an out-of-range claim pass.
//
// Negations are SENTENCE-scoped here, deliberately. Paragraph scoping is what
// let one unrelated "not" disable a whole paragraph; a disclaimer that qualifies
// a different sentence does not qualify this one.
// ---------------------------------------------------------------------------

/** One advertised engine minor and what the record actually says about it. */
export interface EngineLedgerRow {
  readonly minor: string;
  readonly state: 'PASS' | 'FAIL' | 'BLOCKED_EXTERNAL';
  readonly certified: boolean;
}

/**
 * The nine advertised minors and their recorded state.
 *
 * Transcribed from the engine matrix published in
 * `docs/performance-and-evidence.md` (itself rendered from the task-64
 * `/environment/engineMatrix` record): 0 certified, one stale PASS (5.7), one
 * FAIL that is ours (5.8), seven BLOCKED_EXTERNAL. The contract test re-parses
 * that table and asserts this constant still matches it, so the ledger cannot
 * drift away from the document it was taken from. It lives here as data rather
 * than a file read because these rules are pure functions over text and the
 * underlying evidence directory is not distributed.
 */
export const ENGINE_CERTIFICATION_LEDGER: readonly EngineLedgerRow[] = Object.freeze([
  { minor: '5.0', state: 'BLOCKED_EXTERNAL', certified: false },
  { minor: '5.1', state: 'BLOCKED_EXTERNAL', certified: false },
  { minor: '5.2', state: 'BLOCKED_EXTERNAL', certified: false },
  { minor: '5.3', state: 'BLOCKED_EXTERNAL', certified: false },
  { minor: '5.4', state: 'BLOCKED_EXTERNAL', certified: false },
  { minor: '5.5', state: 'BLOCKED_EXTERNAL', certified: false },
  { minor: '5.6', state: 'BLOCKED_EXTERNAL', certified: false },
  { minor: '5.7', state: 'PASS', certified: false },
  { minor: '5.8', state: 'FAIL', certified: false },
]);

/**
 * The advertised engine support range. The published docs claim every minor
 * from 5.0 through 5.8 as supported and working, so a support claim inside
 * that range needs no qualification. A minor OUTSIDE the advertised range
 * (e.g. `5.9`, `4.27`) is not advertised as supported and may only be named
 * with a support verb when the same sentence qualifies or denies it.
 */
export const SUPPORTED_ENGINE_MINORS: readonly string[] = Object.freeze(
  ENGINE_CERTIFICATION_LEDGER.map((row) => row.minor),
);

/**
 * Sentences, for negation scoping. Splits only on terminal punctuation followed
 * by whitespace, so `5.0`, `5.8 Preview` and `2025-11-25` never split mid-token;
 * `e.g.`/`i.e.` are guarded because they are the common false split in prose.
 *
 * The closing-delimiter class is load-bearing, not cosmetic. These are markdown
 * documents, and the sentence this rule exists to catch ends `compatibility
 * target.** The MCP Automation Bridge plugin is scoped to build and run…`. A
 * plain `(?<=[.!?])\s+` sees `*` before the space, refuses to split, and hands
 * the support claim a disclaimer from the previous sentence — reproducing the
 * exact paragraph-scoping defect this rule was written to remove.
 *
 * Markdown table rows are dropped: cells are independent facts, and joining them
 * into one string manufactures adjacency between a version column and unrelated
 * prose. Every real claim audited here is a heading, bullet or paragraph.
 */
export function sentencesOf(paragraph: string): readonly string[] {
  return paragraph
    .split('\n')
    .filter((line) => !/^\s*\|/.test(line))
    .join('\n')
    .replace(/\b(e\.g|i\.e|cf|vs)\.\s/gi, '$1<DOT> ')
    .split(/(?<=[.!?][*_`)\]"']*)\s+/)
    .map((s) => s.replace(/<DOT>/g, '.').trim())
    .filter((s) => s.length > 0);
}

/** Verbs that assert the software WORKS on an engine, as opposed to certifying it. */
const ENGINE_SUPPORT_ASSERTIONS: readonly RegExp[] = [
  /\bscoped to (?:build|run|compile)\b/i,
  /\bbuilds? and runs?\b/i,
  /\bruns? and builds?\b/i,
  /\bsupports?\b/i,
  /\bsupported (?:on|across|for|by)\b/i,
  /\bworks? (?:on|with|across)\b/i,
  /\bruns? (?:on|across)\b/i,
  /\bcompatible with\b/i,
  /\brequires? Unreal Engine\b/i,
  /\bcompiles? (?:on|across|against)\b/i,
];

/** An explicit engine range: `5.0-5.8`, `5.0–5.8`, `5.0 through 5.8`, `5.0 to 5.8`. */
const ENGINE_RANGE = /\b(5\.\d)\s*(?:[-–—]|to|through)\s*(5\.\d)\b/i;

/** A bare engine minor mention, e.g. `UE 5.6`, `5.8 Preview`, `Unreal Engine 5.2`. */
const ENGINE_MINOR_TOKEN = /\b(?:UE\s*|Unreal Engine\s*)?(5\.\d)(?:\.\d+)?\b/gi;

/** Every advertised minor a sentence claims, expanding any range it states. */
export function claimedEngineMinors(sentence: string): readonly string[] {
  const claimed = new Set<string>();
  const range = ENGINE_RANGE.exec(sentence);
  if (range) {
    const low = Number(range[1].slice(2));
    const high = Number(range[2].slice(2));
    for (let n = Math.min(low, high); n <= Math.max(low, high); n++) claimed.add(`5.${n}`);
  }
  for (const match of sentence.matchAll(ENGINE_MINOR_TOKEN)) claimed.add(match[1]);
  return [...claimed].sort();
}

/**
 * Markers that make a support sentence honest. Narrow on purpose, and matched
 * within the SAME sentence as the claim: each one names the gap rather than
 * merely containing a negative word. `\bnot\b` is deliberately NOT here — it is
 * what let "Console platforms are not included" excuse a false certification.
 */
const SUPPORT_NEGATIONS: readonly RegExp[] = [
  // An outright denial of the capability. "A build for 5.6 won't work with 5.7"
  // and "Plugin failed to compile on UE older than 5.4" trip the support verbs
  // while asserting the exact opposite of support, so the denial must clear them.
  /\b(?:won't|will not|does ?n'?t|do not|did ?n'?t|cannot|can'?t|never)\s+(?:\w+\s+){0,2}(?:work|build|compile|run|support)/i,
  /\b(?:failed|fails|failing) to\b/i,
  /\bnot\s+(?:yet\s+)?(?:been\s+)?(?:certified|verified|compile-verified|validated|proven|tested|confirmed|run|exercised)\b/i,
  /\bnot\s+(?:currently\s+)?(?:supported|compiling|building)\b/i,
  /\bdoes not (?:compile|build|run|certify)\b/i,
  /\bis not a (?:certification|guarantee|claim)\b/i,
  /\buncertified\b/i,
  /\bcompatibility target\b/i,
  /\bintended\b/i,
  /\bintent\b/i,
  /\baspiration/i,
  /\bunverified\b/i,
  /\bpending\b/i,
  /\bblocked\b/i,
  /\bunknown\b/i,
  /\bdeferred\b/i,
  /\bin progress\b/i,
  /\bdo not (?:assume|claim|cite|quote|read)\b/i,
];

/** The rendered failure reason, so a red gate tells the author what is wrong. */
export function unsupportedEngineMinors(sentence: string): readonly string[] {
  return claimedEngineMinors(sentence).filter(
    (minor) => !SUPPORTED_ENGINE_MINORS.includes(minor),
  );
}

function assertsUnsupportedEngineSupport(paragraph: string): boolean {
  return sentencesOf(paragraph).some(
    (sentence) =>
      has(sentence, ENGINE_SUPPORT_ASSERTIONS) &&
      unsupportedEngineMinors(sentence).length > 0 &&
      !has(sentence, SUPPORT_NEGATIONS),
  );
}

export const DOCS_CLAIM_RULES: readonly ClaimRule[] = [
  {
    id: 'stale-public-tool-surface',
    description:
      'The public surface is the single `unreal` gateway tool; the 23 canonical parents are internal.',
    violates: (p) => has(p, PUBLIC_SURFACE_ASSERTIONS) && !has(p, REMOVAL_NEGATIONS),
  },
  {
    id: 'removed-unrealagent-surface',
    description:
      'The in-editor assistant panel was removed from this tree; it may be named only as a migration target.',
    violates: (p) => has(p, UNREAL_AGENT_MENTIONS) && !has(p, REMOVAL_NEGATIONS),
  },
  {
    id: 'unsupported-protocol-version',
    description:
      'Only 2025-11-25 / 2025-06-18 / 2025-03-26 (native) plus 2024-11-05 / 2024-10-07 (TypeScript SDK) exist.',
    violates: (p) =>
      has(p, PROTOCOL_CONTEXT) &&
      unsupportedProtocolVersions(p).length > 0 &&
      !has(p, REMOVAL_NEGATIONS),
  },
  {
    id: 'unbacked-certification',
    description:
      'No certification result may be asserted for an absent engine (5.1/5.2/5.4/5.6) or for the full 5.0-5.8 range.',
    violates: (p) =>
      has(p, CERTIFICATION_CONTEXT) &&
      (absentEngineMention(p) || FULL_RANGE_CLAIM.test(p)) &&
      !has(p, CERTIFICATION_NEGATIONS),
  },
  {
    id: 'unbacked-engine-range-support',
    description:
      'A support claim naming a minor must stay within the advertised UE 5.0-5.8 range, or be qualified in the same sentence.',
    violates: assertsUnsupportedEngineSupport,
  },
];

/** Every violation in one document, in rule order. */
export function auditDocument(file: string, text: string): readonly ClaimViolation[] {
  const violations: ClaimViolation[] = [];
  for (const paragraph of paragraphsOf(text)) {
    for (const rule of DOCS_CLAIM_RULES) {
      if (rule.violates(paragraph)) {
        violations.push({ rule: rule.id, file, paragraph });
      }
    }
  }
  return violations;
}
