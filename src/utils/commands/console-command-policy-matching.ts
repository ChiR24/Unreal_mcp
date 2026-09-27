// src/utils/commands/console-command-policy-matching.ts
// The console-command policy evaluator. The TypeScript validator applies EVERY
// rule in console-command-policy-rules.ts, the union of both surfaces: a rule
// marked native-only is enforced by the plugin on every command anyway, so
// refusing it here too only fails earlier and never admits more. The native
// header (scripts/generate-native-headers.ts) carries the rules the
// C++ validator can express.

import { CONSOLE_COMMAND_POLICY_RULES, type ConsoleCommandRuleMatcher } from './console-command-policy-rules.js';

/** Escape every regex metacharacter so a rule value matches literally. */
export function escapeRegExp(value: string): string {
  return value.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
}

/**
 * Compiled-once regexes for the rule set.
 *
 * Every matcher used to build a fresh `RegExp` per call, so evaluating one
 * console command against the policy compiled 28 of them — the pattern rules
 * plus one per value of the whitespace-bounded rule. The keys come only from
 * rule DATA (a matcher's source/flags, or an escaped rule value), never from
 * the command under test, so this cache is bounded by the rule set. No matcher
 * uses the `g` flag, so a shared instance carries no `lastIndex` state.
 */
const compiledMatchers = new Map<string, RegExp>();

function cachedRegExp(source: string, flags: string): RegExp {
  // NUL separates the two halves so no (flags, source) pair can collide.
  // Written as an ESCAPE: it used to be a raw 0x00 byte in this file, which
  // made git treat a security-relevant source as binary and refuse to diff
  // it.
  const key = `${flags}\u0000${source}`;
  let compiled = compiledMatchers.get(key);
  if (compiled === undefined) {
    compiled = new RegExp(source, flags);
    compiledMatchers.set(key, compiled);
  }
  return compiled;
}

/** The first whitespace-delimited token, or '' when the command is blank. */
export function firstToken(command: string): string {
  return command.split(/\s+/u).filter(Boolean)[0] ?? '';
}

/** True when the command satisfies the matcher. */
export function matchesRule(command: string, matcher: ConsoleCommandRuleMatcher): boolean {
  switch (matcher.kind) {
    case 'contains-any':
      return matcher.values.some((value) => command.includes(value));
    case 'first-token':
      return matcher.values.includes(firstToken(command));
    case 'whitespace-bounded-anywhere':
      return matcher.values.some((value) =>
        cachedRegExp(`(?:^|\\s)${escapeRegExp(value)}(?:\\s|$)`, 'i').test(command),
      );
    case 'pattern':
      return cachedRegExp(matcher.source, matcher.flags).test(command);
    default: {
      const never: never = matcher;
      throw new Error(`Unhandled console-command matcher: ${String(never)}`);
    }
  }
}

/** True when any policy rule blocks the command (compared trimmed and lower-cased). */
export function isConsoleCommandBlocked(command: string): boolean {
  const normalized = command.trim().toLowerCase();
  return CONSOLE_COMMAND_POLICY_RULES.some((rule) => matchesRule(normalized, rule.matcher));
}
