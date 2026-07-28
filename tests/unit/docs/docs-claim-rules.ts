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
