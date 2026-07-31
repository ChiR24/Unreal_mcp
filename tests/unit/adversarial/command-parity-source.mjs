// @ts-check
// tests/unit/adversarial/command-parity-source.mjs
// Task 51 — the src-bound bindings for the console-command differential.
//
// Split out of command-parity.mjs for one reason: module resolution. The vitest
// suites can import TypeScript from `src/` directly; an offline CLI running under
// plain node cannot, and must reach the same functions in `dist/`. Keeping the
// asymmetry-class data and the attribution logic free of `src/` imports lets both
// callers share ONE implementation instead of growing a second one.
//
// Nothing is reimplemented here. Every export is a binding over the real generated
// policy the runtime validator delegates to.

import {
  CONSOLE_COMMAND_POLICY_BLOCK_REASONS,
  CONSOLE_COMMAND_POLICY_RULES,
} from '../../../src/utils/commands/console-command-policy-rules.js';
import { evaluateGeneratedConsoleCommandPolicy } from '../../../src/utils/commands/console-command-policy-generated.js';

import { bothMustRefuse as bothMustRefuseWith, typescriptSideFrom } from './command-parity.mjs';

/** Every rule id the TypeScript surface can attribute a block to. */
export const KNOWN_RULE_IDS = Object.freeze(CONSOLE_COMMAND_POLICY_RULES.map((rule) => rule.id));

/** Reason codes the TypeScript evaluator can emit, for allowlist validation. */
export const TS_REASONS = Object.freeze([...CONSOLE_COMMAND_POLICY_BLOCK_REASONS, 'ALLOWED']);

/** The default TypeScript side, over the evaluator imported from source. */
export const typescriptSide = typescriptSideFrom(evaluateGeneratedConsoleCommandPolicy);

/**
 * The invariant no asymmetry class may excuse, bound to the source evaluator.
 * @param {string} command
 * @param {ReturnType<typeof import('./native-policy-mirror.mjs').loadNativePolicy>} policy
 */
export function bothMustRefuse(command, policy) {
  return bothMustRefuseWith(command, policy, typescriptSide);
}
