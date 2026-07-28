// @ts-check
// tests/unit/task-51/command-parity.mjs
// Task 51 — the console-command instantiation of the differential, and the five
// asymmetry classes the two surfaces are DESIGNED to have.
//
// TypeScript and the plugin do not share one matcher; they share one generated rule
// SET, from which each surface takes the rules addressed to it. That is deliberate
// (`appliesTo: 'typescript' | 'native' | 'both'` in console-command-policy-rules.ts),
// so a naive "both surfaces must agree on every string" differential would report
// dozens of by-design differences and teach everyone to ignore it.
//
// The classes below are derived by READING THE RULES, before running anything. Each
// names the rule ids that produce it and argues why that direction is safe. Two
// facts drive every argument:
//
//   * THE UE CONSOLE EXECUTES THE FIRST TOKEN. Later tokens are arguments to that
//     command; there is no shell, so `stat fps rm` does not delete anything. The
//     native gate is therefore precise where TypeScript is conservative.
//   * THE PLUGIN IS THE SOLE AUTHORIZATION AUTHORITY and re-enforces every request
//     before it reaches the editor queue. TypeScript being stricter is redundancy.
//     TypeScript being laxer costs a wasted round trip, not a bypass — but it is
//     still recorded, marked defenceInDepthOnly, and must be argued, because a
//     native-transport client never traverses TypeScript at all.
//
// What is NOT here, deliberately: any entry keyed on a seed, an input, or a count.
//
// This module is kept FREE of `src/` imports so it can be loaded by plain node as
// well as by vitest. The src-bound bindings (the rule inventory and the default
// TypeScript side) live in command-parity-source.mjs; the offline CLI supplies the
// same functions from `dist/`. Both paths reach the one generated evaluator — the
// split is about module resolution, never about having a second implementation.

import { nativeDecision } from './native-policy-mirror.mjs';

/** Every reason string the native mirror can emit. */
export const NATIVE_REASONS = Object.freeze([
  'EMPTY', 'NO_TOKEN', 'ALLOWED', 'UNSAFE_SEPARATOR', 'DANGEROUS_ENGINE_COMMAND',
  'RESTRICTED_ENGINE_COMMAND', 'SHELL_COMMAND', 'UNSAFE_TOKEN',
]);

/** TS rules that match a dangerous word ANYWHERE, where native inspects only the first token. */
const TS_ANYWHERE_RULES = Object.freeze([
  'typescript.dangerous-whitespace-bounded',
  'typescript.shell-name-anywhere',
  'typescript.start-quoted',
]);

/**
 * TS rules that match python spellings more flexibly than native's fixed substrings.
 *
 * Deliberately narrowed to the three that CAN produce a divergence. The other three
 * python rules in the policy — `dunder-import`, `subprocess-member` and `os-system` —
 * match spellings the plugin also carries verbatim in FORBIDDEN_TOKENS, so both
 * surfaces refuse them and no excuse is needed. Listing them here anyway would give
 * the allowlist reach over a divergence that today cannot happen, which is exactly
 * how an allowlist quietly grows to cover a future regression.
 */
const TS_FLEXIBLE_PYTHON_RULES = Object.freeze([
  'typescript.flexible-import',
  'typescript.flexible-from-import',
  'typescript.flexible-call',
]);

/** @type {readonly import('./differential-engine.mjs').AsymmetryClass[]} */
export const COMMAND_ASYMMETRY_CLASSES = Object.freeze([
  {
    id: 'TS_ANYWHERE_VS_NATIVE_FIRST_TOKEN',
    leftRules: TS_ANYWHERE_RULES,
    rightReasons: ['ALLOWED'],
    direction: 'left-stricter',
    rationale:
      'TypeScript matches a dangerous word wherever it appears in the string; the plugin compares only the first whitespace-delimited token, which is the position the Unreal console actually executes.',
    securityArgument:
      'The UE console has no shell and no command chaining once separators are rejected (which both surfaces do, via the shared rule). Everything after the first token is an ARGUMENT to that first command, so "stat fps rm" runs "stat" and cannot delete anything. The plugin therefore refuses every spelling that can execute, and the extra TypeScript strictness only removes inert argument text.',
  },
  {
    id: 'TS_SUBSTRING_VS_NATIVE_TOKEN',
    leftRules: ['typescript.forbidden-substring'],
    rightReasons: ['ALLOWED'],
    direction: 'left-stricter',
    rationale:
      'TypeScript blocks shutdown/reboot/rmdir/mklink as bare substrings; the plugin blocks shutdown only as a first token and does not list reboot, rmdir or mklink at all.',
    securityArgument:
      'reboot, rmdir and mklink are operating-system commands, not Unreal console commands: submitting them to the console produces "Command not recognized", so the plugin has nothing to defend against there. shutdown IS an engine command and the plugin blocks it in the only position where it executes. TypeScript keeps the wider substring form because the same validator also guards batch and python-adjacent inputs where the string is not necessarily the first token.',
  },
  {
    id: 'TS_FLEXIBLE_PYTHON_PATTERNS',
    leftRules: TS_FLEXIBLE_PYTHON_RULES,
    rightReasons: ['ALLOWED'],
    direction: 'left-stricter',
    rationale:
      'TypeScript matches python spellings with variable whitespace ("import   os", "exec (") and a wider module set (importlib, shutil) through regexes; the plugin matches fixed substrings only, so the spaced spelling reaches it unblocked.',
    securityArgument:
      'Both surfaces block py and python as a first token through the SHARED rule, so python source cannot be submitted to the plugin console path in the first place. The flexible patterns defend the TypeScript path, where python text can also arrive as an execute_python parameter rather than as a console command. No spelling that reaches the plugin console gains execution from this difference.',
  },
  {
    id: 'NATIVE_ONLY_ENGINE_FIRST_TOKENS',
    leftRules: [],
    rightReasons: ['DANGEROUS_ENGINE_COMMAND', 'RESTRICTED_ENGINE_COMMAND'],
    direction: 'right-stricter',
    defenceInDepthOnly: true,
    rationale:
      'The plugin blocks debugbreak, recompileglobalshaders, deriveddatacache, unrealbuildtool and ubt as first tokens; the TypeScript rule set carries no matching entry, so TypeScript forwards them.',
    securityArgument:
      'The plugin is the sole authorization authority and re-enforces every request before it reaches the editor queue, so a command TypeScript forwards is still refused there: the cost is one wasted round trip, not execution. This is recorded rather than excused silently because a native-transport client never traverses TypeScript at all, so the plugin rule is the only one protecting that surface — and it is present.',
  },
  {
    id: 'NATIVE_SUBSTRING_VS_TS_WHITESPACE_BOUNDED',
    leftRules: [],
    rightReasons: ['UNSAFE_TOKEN'],
    direction: 'right-stricter',
    defenceInDepthOnly: true,
    rationale:
      'For tokens such as "obj list", "memreport" and "check(false)" the plugin matches a bare substring while TypeScript requires whitespace boundaries, so a glued spelling ("xobj list") is refused by the plugin and forwarded by TypeScript.',
    securityArgument:
      'The whitespace boundary exists so a legitimate identifier that merely contains these letters is not refused; the glued spelling is not an executable console command, since the console would look up the whole glued first token and fail to resolve it. The plugin refuses it anyway and, being the enforcing authority, its decision is the one that governs. Neither surface permits the executable spelling.',
  },
]);

/**
 * The TypeScript side, built over a supplied evaluator.
 *
 * Injectable for one reason only: the vitest suites import the evaluator straight
 * from `src/` (TypeScript), while an offline CLI has to reach the SAME function in
 * `dist/`. Both call the real generated evaluator the runtime validator delegates
 * to; neither reimplements it, which is what would make the differential compare a
 * copy against itself.
 * @param {(command: string, surface: 'typescript'|'native') => { blocked: boolean, reasonCode: string, ruleId: string|null }} evaluate
 */
export function typescriptSideFrom(evaluate) {
  /** @param {string} command @returns {import('./differential-engine.mjs').SideResult} */
  return (command) => {
    const decision = evaluate(command, 'typescript');
    return {
      verdict: decision.blocked ? 'reject' : 'accept',
      reason: decision.reasonCode,
      rule: decision.ruleId,
    };
  };
}



/**
 * The native side: the mirror, which reports `undecidable` outside ASCII rather
 * than guessing how UE's per-character ToLower would have folded the input.
 * @param {ReturnType<typeof import('./native-policy-mirror.mjs').loadNativePolicy>} policy
 */
export function nativeSideFactory(policy) {
  /** @param {string} command @returns {import('./differential-engine.mjs').SideResult} */
  return (command) => {
    const decision = nativeDecision(command, policy);
    if (!decision.decidable) {
      return { verdict: 'undecidable', reason: 'NON_ASCII_CASE_FOLDING', rule: null };
    }
    return { verdict: decision.blocked ? 'reject' : 'accept', reason: decision.reason, rule: null };
  };
}

/**
 * Attribute a divergence to a declared class by looking at WHICH RULES FIRED.
 * Returns null when no declared rule pair explains it — which the engine turns into
 * a finding, by design.
 * @param {string} _command
 * @param {import('./differential-engine.mjs').SideResult} left
 * @param {import('./differential-engine.mjs').SideResult} right
 * @returns {string|null}
 */
export function explainCommandDivergence(_command, left, right) {
  if (left.verdict === 'reject' && right.verdict === 'accept') {
    if (left.rule !== null && left.rule !== undefined && TS_ANYWHERE_RULES.includes(left.rule)) {
      return 'TS_ANYWHERE_VS_NATIVE_FIRST_TOKEN';
    }
    if (left.rule === 'typescript.forbidden-substring') return 'TS_SUBSTRING_VS_NATIVE_TOKEN';
    if (left.rule !== null && left.rule !== undefined && TS_FLEXIBLE_PYTHON_RULES.includes(left.rule)) {
      return 'TS_FLEXIBLE_PYTHON_PATTERNS';
    }
    return null;
  }
  if (left.verdict === 'accept' && right.verdict === 'reject') {
    if (right.reason === 'DANGEROUS_ENGINE_COMMAND' || right.reason === 'RESTRICTED_ENGINE_COMMAND') {
      return 'NATIVE_ONLY_ENGINE_FIRST_TOKENS';
    }
    if (right.reason === 'UNSAFE_TOKEN') return 'NATIVE_SUBSTRING_VS_TS_WHITESPACE_BOUNDED';
    return null;
  }
  return null;
}

/**
 * The invariant that no asymmetry class may ever excuse: a command whose FIRST
 * TOKEN is an executable dangerous name, or which carries a chaining separator,
 * must be refused by BOTH surfaces. This is checked separately from the
 * differential so that no allowlist entry can reach it.
 * @param {string} command
 * @param {ReturnType<typeof import('./native-policy-mirror.mjs').loadNativePolicy>} policy
 * @param {(command: string) => import('./differential-engine.mjs').SideResult} side
 * @returns {{ ok: boolean, reason: string }}
 */
export function bothMustRefuse(command, policy, side) {
  const ts = side(command);
  const native = nativeDecision(command, policy);
  if (ts.verdict !== 'reject') return { ok: false, reason: `TypeScript accepted "${command}"` };
  if (!native.blocked) return { ok: false, reason: `the plugin accepted "${command}"` };
  return { ok: true, reason: `${ts.reason} / ${native.reason}` };
}
