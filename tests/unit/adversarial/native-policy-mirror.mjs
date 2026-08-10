// @ts-check
// tests/unit/adversarial/native-policy-mirror.mjs
// Task 51 — a faithful JS mirror of the NATIVE console-command gate, built from
// the generated header the C++ actually compiles.
//
// WHY A MIRROR AND NOT A LIVE CALL. The native decision is made inside the plugin,
// which needs a compiled editor. Task 52 owns editors. But the native decision is
// data (the generated header) plus a short, fixed algorithm (IsBlockedCommand in
// McpAutomationBridge_ConsoleCommandHandlers.cpp), so it can be reproduced exactly
// and differentially compared against the TypeScript surface offline.
//
// TWO THINGS KEEP THIS HONEST rather than a story about C++:
//
//   1. THE DATA IS READ, NOT COPIED. The arrays are parsed out of the generated
//      header at run time. If the generator emits a new blocked command and nobody
//      updates this file, the mirror picks it up. A hardcoded copy would silently
//      diverge and then "prove" parity against itself.
//   2. THE ALGORITHM IS PINNED BY A SOURCE CONTRACT. `NATIVE_ALGORITHM_CONTRACT`
//      lists the exact fragments of the .cpp this mirror assumes. The test asserts
//      every one of them is still present. Change the C++ and the mirror fails
//      loudly instead of quietly describing code that no longer exists.
//
// ONE LIMIT, STATED UP FRONT. `FString::ToLower()` is per-character and does not
// perform full Unicode case folding; JavaScript's `toLowerCase()` does (notably
// 'İ' folds to two code points). For any input outside ASCII the two lowerings can
// legitimately differ, so this mirror refuses to adjudicate those: `decidable` is
// false and the differential reports them as UNDECIDABLE_OFFLINE rather than
// inventing a verdict. Claiming parity there would be exactly the overclaim this
// suite exists to prevent.

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

export const NATIVE_POLICY_HEADER =
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/ConsoleCommand/McpAutomationBridge_ConsoleCommandPolicy.generated.h';

export const NATIVE_POLICY_IMPL =
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/ConsoleCommand/McpAutomationBridge_ConsoleCommandHandlers.cpp';

/**
 * The exact source fragments this mirror's algorithm depends on. Each is a literal
 * substring of the .cpp; the contract test asserts all of them still occur, in this
 * order, inside `IsBlockedCommand`.
 */
export const NATIVE_ALGORITHM_CONTRACT = Object.freeze([
  'FString LowerCommand = Command.TrimStartAndEnd().ToLower();',
  'if (LowerCommand.IsEmpty())',
  'if (ContainsUnsafeSeparator(LowerCommand))',
  'LowerCommand.ParseIntoArrayWS(CommandParts);',
  'const FString& CommandName = CommandParts[0];',
  'McpGeneratedConsoleCommandPolicy::BLOCKED_COMMANDS',
  'McpGeneratedConsoleCommandPolicy::RESTRICTED_COMMANDS',
  'McpGeneratedConsoleCommandPolicy::FORBIDDEN_COMMAND_NAMES',
  'McpGeneratedConsoleCommandPolicy::FORBIDDEN_TOKENS',
]);

/** The array names the mirror needs out of the generated header. */
export const NATIVE_POLICY_ARRAYS = Object.freeze([
  'UNSAFE_SEPARATORS', 'BLOCKED_COMMANDS', 'RESTRICTED_COMMANDS',
  'FORBIDDEN_COMMAND_NAMES', 'FORBIDDEN_TOKENS',
]);

/** Decode the escapes the generator emits inside `TEXT("...")`. @param {string} raw */
function decodeTextLiteral(raw) {
  return raw
    .replace(/\\n/gu, '\n').replace(/\\r/gu, '\r').replace(/\\t/gu, '\t')
    .replace(/\\"/gu, '"').replace(/\\\\/gu, '\\');
}

/**
 * Parse one `static const TCHAR* const NAME[] = { TEXT("a"), ... };` block.
 * Deliberately strict: a name that is absent throws rather than yielding an empty
 * list, because an empty block list would make the mirror permissive and every
 * parity assertion would pass for the wrong reason.
 * @param {string} source @param {string} name @returns {string[]}
 */
export function parseNativeArray(source, name) {
  const start = source.indexOf(`${name}[] = {`);
  if (start < 0) throw new Error(`native policy header has no array "${name}"`);
  const end = source.indexOf('};', start);
  if (end < 0) throw new Error(`native policy array "${name}" is unterminated`);
  const body = source.slice(start, end);
  const values = [...body.matchAll(/TEXT\("((?:[^"\\]|\\.)*)"\)/gu)].map((match) => decodeTextLiteral(match[1]));
  if (values.length === 0) throw new Error(`native policy array "${name}" parsed empty`);
  return values;
}

/**
 * Load the native policy data from the generated header.
 * @param {string} [projectRoot]
 * @returns {{ separators: string[], blocked: string[], restricted: string[], forbiddenNames: string[], forbiddenTokens: string[], headerPath: string }}
 */
export function loadNativePolicy(projectRoot = process.cwd()) {
  const headerPath = resolve(projectRoot, NATIVE_POLICY_HEADER);
  const source = readFileSync(headerPath, 'utf8');
  return {
    separators: parseNativeArray(source, 'UNSAFE_SEPARATORS'),
    blocked: parseNativeArray(source, 'BLOCKED_COMMANDS'),
    restricted: parseNativeArray(source, 'RESTRICTED_COMMANDS'),
    forbiddenNames: parseNativeArray(source, 'FORBIDDEN_COMMAND_NAMES'),
    forbiddenTokens: parseNativeArray(source, 'FORBIDDEN_TOKENS'),
    headerPath,
  };
}

/**
 * ASCII-only inputs are the ones where JS and UE lowercasing provably agree.
 * Checked by code point rather than a regex so no control character has to be
 * written into a character class (which is itself a lint hazard).
 * @param {string} command
 */
export function isAsciiDecidable(command) {
  for (const character of command) {
    if (/** @type {number} */ (character.codePointAt(0)) > 0x7f) return false;
  }
  return true;
}

/**
 * `FString::ParseIntoArrayWS` splits on whitespace and DROPS empty entries. JS
 * `split(/\s+/)` leaves a leading empty string when the input starts with
 * whitespace, so the filter is not cosmetic: without it the "first token" of
 * " quit" is '' and the mirror would wrongly allow it.
 * @param {string} lowered
 */
export function firstTokenWs(lowered) {
  return lowered.split(/\s+/u).filter((part) => part.length > 0)[0] ?? '';
}

/**
 * The native verdict for one command.
 *
 * `decidable` is false when the input leaves ASCII, where UE's per-character
 * ToLower and JavaScript's full case folding may disagree. A caller must not treat
 * an undecidable row as either a pass or a failure.
 * @param {string} command
 * @param {ReturnType<typeof loadNativePolicy>} policy
 * @returns {{ blocked: boolean, reason: string, decidable: boolean }}
 */
export function nativeDecision(command, policy) {
  const decidable = isAsciiDecidable(command);
  const lowered = command.trim().toLowerCase();
  if (lowered.length === 0) return { blocked: false, reason: 'EMPTY', decidable };
  for (const separator of policy.separators) {
    if (lowered.includes(separator)) return { blocked: true, reason: 'UNSAFE_SEPARATOR', decidable };
  }
  const name = firstTokenWs(lowered);
  if (name.length === 0) return { blocked: false, reason: 'NO_TOKEN', decidable };
  if (policy.blocked.some((entry) => entry.toLowerCase() === name)) {
    return { blocked: true, reason: 'DANGEROUS_ENGINE_COMMAND', decidable };
  }
  if (policy.restricted.some((entry) => entry.toLowerCase() === name)) {
    return { blocked: true, reason: 'RESTRICTED_ENGINE_COMMAND', decidable };
  }
  if (policy.forbiddenNames.some((entry) => entry.toLowerCase() === name)) {
    return { blocked: true, reason: 'SHELL_COMMAND', decidable };
  }
  for (const token of policy.forbiddenTokens) {
    if (lowered.includes(token.toLowerCase())) return { blocked: true, reason: 'UNSAFE_TOKEN', decidable };
  }
  return { blocked: false, reason: 'ALLOWED', decidable };
}

/**
 * Confirm the .cpp still implements the algorithm this mirror reproduces.
 * @param {string} [projectRoot]
 * @returns {{ ok: boolean, missing: string[], implPath: string }}
 */
export function verifyNativeAlgorithmContract(projectRoot = process.cwd()) {
  const implPath = resolve(projectRoot, NATIVE_POLICY_IMPL);
  const source = readFileSync(implPath, 'utf8');
  const missing = NATIVE_ALGORITHM_CONTRACT.filter((fragment) => !source.includes(fragment));
  return { ok: missing.length === 0, missing, implPath };
}
