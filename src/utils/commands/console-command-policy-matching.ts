// src/utils/commands/console-command-policy-matching.ts
// The rule-matching primitives shared by the policy MODEL (the authored rule
// set) and the policy GENERATOR/evaluator (which emits and applies the
// generated artifact).
//
// These three were written out identically in both modules. They decide whether
// a console command is blocked, so two copies meant a matching bug — a missed
// regex escape, a different token split — could be fixed on one surface and
// left open on the other while the parity test still compared rule DATA and
// passed. One implementation makes that class of divergence impossible.

import type { ConsoleCommandRuleMatcher } from './console-command-policy-rules.js';

/** Escape every regex metacharacter so a rule value matches literally. */
export function escapeRegExp(value: string): string {
  return value.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
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
        new RegExp(`(?:^|\\s)${escapeRegExp(value)}(?:\\s|$)`, 'i').test(command),
      );
    case 'pattern':
      return new RegExp(matcher.source, matcher.flags).test(command);
    default: {
      const never: never = matcher;
      throw new Error(`Unhandled console-command matcher: ${String(never)}`);
    }
  }
}
