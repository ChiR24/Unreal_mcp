import { z } from 'zod';

import {
  CONSOLE_COMMAND_POLICY_RULES,
  type ConsoleCommandPolicyBlockReason,
  type ConsoleCommandPolicyRule,
  type ConsoleCommandPolicySurface,
} from './console-command-policy-rules.js';
import { matchesRule } from './console-command-policy-matching.js';

export type ConsoleCommandPolicyDecision =
  | {
      readonly kind: 'allowed';
      readonly blocked: false;
      readonly reasonCode: 'ALLOWED';
    }
  | {
      readonly kind: 'blocked';
      readonly blocked: true;
      readonly reasonCode: ConsoleCommandPolicyBlockReason;
      readonly ruleId: string;
    };

export type IntendedConsoleCommandPolicyDecision =
  | {
      readonly kind: 'allowed';
      readonly blocked: false;
      readonly reasonCodes: readonly ['ALLOWED'];
      readonly sourceSurfaces: readonly [];
    }
  | {
      readonly kind: 'blocked';
      readonly blocked: true;
      readonly reasonCodes: readonly ConsoleCommandPolicyBlockReason[];
      readonly sourceSurfaces: readonly ConsoleCommandPolicySurface[];
    };

export type ConsoleCommandPolicyInputResult =
  | { readonly kind: 'valid'; readonly command: string }
  | { readonly kind: 'invalid'; readonly reasonCode: 'MALFORMED_INPUT' };

type SurfaceNormalization = {
  readonly edgeWhitespace: 'trim';
  readonly caseFolding: 'unicode-lowercase';
  readonly internalWhitespace: 'preserve';
  readonly firstToken: 'split-whitespace-runs';
  readonly unicodeCompatibility: 'preserve';
};

const surfaceNormalization = {
  edgeWhitespace: 'trim',
  caseFolding: 'unicode-lowercase',
  internalWhitespace: 'preserve',
  firstToken: 'split-whitespace-runs',
  unicodeCompatibility: 'preserve',
} as const satisfies SurfaceNormalization;

export const CONSOLE_COMMAND_POLICY_NORMALIZATION = {
  typescript: surfaceNormalization,
  native: surfaceNormalization,
  intended: {
    strategy: 'fail-closed-union',
    whitespaceSemantics: 'preserve-per-rule',
    unicodeCompatibility: 'preserve',
  },
} as const;

export const CONSOLE_COMMAND_POLICY_GENERATOR_REQUIREMENTS = {
  sourceOrdering: 'declared-rule-order',
  targetSurfaces: ['typescript', 'native'],
  intendedPolicy: 'fail-closed-union',
  normalizationSource: 'typed-rule-data',
  reasonCodes: 'stable-and-surface-independent',
  serialization: 'stable-json',
  runtimeWiring: 'deferred-to-task-22',
} as const;

const ConsoleCommandInputSchema = z.string();

function assertNever(value: never): never {
  throw new Error(`Unhandled console-command policy variant: ${String(value)}`);
}

function appliesToSurface(
  rule: ConsoleCommandPolicyRule,
  surface: ConsoleCommandPolicySurface,
): boolean {
  switch (rule.appliesTo) {
    case 'both':
      return true;
    case 'typescript':
      return surface === 'typescript';
    case 'native':
      return surface === 'native';
    default:
      return assertNever(rule.appliesTo);
  }
}

export function parseConsoleCommandPolicyInput(input: unknown): ConsoleCommandPolicyInputResult {
  const result = ConsoleCommandInputSchema.safeParse(input);
  return result.success
    ? { kind: 'valid', command: result.data }
    : { kind: 'invalid', reasonCode: 'MALFORMED_INPUT' };
}

export function evaluateCurrentConsoleCommandPolicy(
  command: string,
  surface: ConsoleCommandPolicySurface,
): ConsoleCommandPolicyDecision {
  const normalized = command.trim().toLowerCase();
  for (const rule of CONSOLE_COMMAND_POLICY_RULES) {
    if (appliesToSurface(rule, surface) && matchesRule(normalized, rule.matcher)) {
      return {
        kind: 'blocked',
        blocked: true,
        reasonCode: rule.reasonCode,
        ruleId: rule.id,
      };
    }
  }
  return { kind: 'allowed', blocked: false, reasonCode: 'ALLOWED' };
}

export function evaluateConsoleCommandPolicyInput(
  input: unknown,
  surface: ConsoleCommandPolicySurface,
): ConsoleCommandPolicyDecision {
  const parsed = parseConsoleCommandPolicyInput(input);
  if (parsed.kind === 'invalid') {
    return {
      kind: 'blocked',
      blocked: true,
      reasonCode: parsed.reasonCode,
      ruleId: 'shared.malformed-input',
    };
  }
  return evaluateCurrentConsoleCommandPolicy(parsed.command, surface);
}

export function evaluateIntendedConsoleCommandPolicy(
  command: string,
): IntendedConsoleCommandPolicyDecision {
  const typescript = evaluateCurrentConsoleCommandPolicy(command, 'typescript');
  const native = evaluateCurrentConsoleCommandPolicy(command, 'native');
  const reasonCodes: ConsoleCommandPolicyBlockReason[] = [];
  const sourceSurfaces: ConsoleCommandPolicySurface[] = [];

  if (typescript.kind === 'blocked') {
    reasonCodes.push(typescript.reasonCode);
    sourceSurfaces.push('typescript');
  }
  if (native.kind === 'blocked') {
    if (!reasonCodes.includes(native.reasonCode)) reasonCodes.push(native.reasonCode);
    sourceSurfaces.push('native');
  }
  return reasonCodes.length === 0
    ? { kind: 'allowed', blocked: false, reasonCodes: ['ALLOWED'], sourceSurfaces: [] }
    : { kind: 'blocked', blocked: true, reasonCodes, sourceSurfaces };
}

export function serializeConsoleCommandPolicyModel(): string {
  return JSON.stringify({
    schema: 'unreal.console-command-policy-model.v1',
    normalization: CONSOLE_COMMAND_POLICY_NORMALIZATION,
    generatorRequirements: CONSOLE_COMMAND_POLICY_GENERATOR_REQUIREMENTS,
    rules: CONSOLE_COMMAND_POLICY_RULES,
  });
}
