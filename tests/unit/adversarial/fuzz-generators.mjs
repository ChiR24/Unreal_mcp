// @ts-check
// tests/unit/adversarial/fuzz-generators.mjs
// Task 51 — the VALUE generators: text, numbers, paths, console commands.
//
// These produce the inputs that cross a trust boundary. Every one of them is a
// pure function of an Rng, so a case is fully described by (seed, stream, index)
// and an evidence entry can be replayed with three numbers.
//
// The corpus is built from CLASSES OF KNOWN BREAKAGE rather than random bytes,
// because random bytes overwhelmingly produce inputs that are rejected for boring
// reasons and never reach the interesting comparison. Each class below is a shape
// that has broken a real validator somewhere:
//
//   * lone surrogates          — survive JSON.stringify, die at UTF-8 encode
//   * NFC/NFD pairs            — two spellings of one path; one may pass a prefix
//                                check and the other not
//   * bidi overrides           — a name that renders as something else entirely
//   * zero-width joiners       — invisible in a log line, significant to a compare
//   * dotted-I casing          — 'İ'.toLowerCase() is two code points, so a
//                                lowercase-then-compare gate can be walked past
//   * numeric edges            — -0, 2**53, 1e309, "0x10", " 1 ", "1_0"
//   * traversal spellings      — '..', '%2e%2e', '....//', backslash, NUL
//
// Nothing here knows what the system under test does with a value. Judgement lives
// in the property files; a generator that also asserted would hide the cases it
// chose not to emit.

/** Text fragments that have historically defeated normalize-then-compare gates. */
export const ADVERSARIAL_TEXT = Object.freeze([
  '\uD800',                    // lone high surrogate
  '\uDC00',                    // lone low surrogate
  '\uFEFF',                    // BOM as content
  '\u200B', '\u200D', '\u2060', // zero-width space / joiner / word-joiner
  '\u202E', '\u202D',          // RTL / LTR override
  '\u0130', '\u0131',          // dotted capital I, dotless small i
  '\u00DF', '\u1E9E',          // sharp s and its capital
  'A\u030A', '\u00C5', '\u212B', // NFD 'Å', NFC 'Å', ANGSTROM SIGN
  '\u0000', '\u0001', '\u001F', // control characters
  '\u007F',                    // DEL
  '\uFFFD',                    // replacement char
  '\u3000',                    // ideographic space
  '\t', '\n', '\r',
  '\uD83D\uDCA9',              // astral plane pair
  '\u0041\u0301',              // combining acute
]);

/** Numeric spellings that a naive Number()/parseInt() gate accepts or mangles. */
export const ADVERSARIAL_NUMBERS = Object.freeze([
  -0, 0, 1, -1,
  Number.MAX_SAFE_INTEGER, Number.MAX_SAFE_INTEGER + 1, -Number.MAX_SAFE_INTEGER - 1,
  Number.MAX_VALUE, Number.MIN_VALUE, Number.EPSILON,
  1e308, -1e308, 5e-324,
  Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY, Number.NaN,
]);

/** The same, as STRINGS — a distinct attack surface from the numbers themselves. */
export const ADVERSARIAL_NUMERIC_STRINGS = Object.freeze([
  '0', '-0', '+1', ' 1 ', '1_000', '0x10', '0o17', '0b101', '1e999', '-1e999',
  'Infinity', '-Infinity', 'NaN', '1.', '.1', '1e', '', '   ',
  '9007199254740993',                       // 2**53 + 1, unrepresentable
  '340282366920938463463374607431768211456', // 2**128
  '1\u06601',                                // arabic-indic digit in the middle
  '\uFF11',                                  // fullwidth 1
]);

/** Path traversal and containment-escape spellings. */
export const TRAVERSAL_FRAGMENTS = Object.freeze([
  '..', '../', '..\\', '....//', '.../.../', '%2e%2e', '%2E%2E%2F',
  '..%00/', '.%2e/', '\\..\\', '/./', '//', '\\\\', '\u0000',
]);

/** Roots the system is supposed to accept, plus near-misses that must not pass. */
export const PATH_ROOTS = Object.freeze([
  '/Game', '/Engine', '/Script', '/Temp', '/Niagara',
  '/game', '/GAME', '/Gam', '/Games', '/Game2', '/GameX',
  '/Content', '//Game', ' /Game', '/Game ', 'Game', '',
  '/\u0130Game', '/Ga\u200Bme',
]);

/** Console-command atoms, split by what the two surfaces do with them. */
export const COMMAND_ATOMS = Object.freeze({
  /** Blocked by BOTH surfaces as a first token (shared python rule). */
  sharedPython: Object.freeze(['py', 'python']),
  /** Blocked by BOTH surfaces anywhere (shared separator rule). */
  sharedSeparators: Object.freeze(['\n', '\r', '&&', '||', ';', '|', '`']),
  /** Native blocks these ONLY as the first token; TypeScript also has a rule for each. */
  nativeFirstToken: Object.freeze([
    'shutdown', 'quit', 'exit', 'kill', 'crash', 'r.gpucrash', 'r.crash',
    'forcecrash', 'buildpaths', 'rebuildnavigation', 'delete', 'destroy',
    'rm', 'del', 'format', 'copy', 'move', 'start',
  ]),
  /** First tokens the PLUGIN blocks and the TypeScript rule set has no entry for.
   * Kept separate so the differential is actually asked about that direction — the
   * one where the laxer surface is the non-authoritative one. */
  nativeOnlyFirstToken: Object.freeze([
    'debugbreak', 'recompileglobalshaders', 'deriveddatacache', 'unrealbuildtool', 'ubt',
  ]),
  /** Substrings both surfaces reject wherever they appear. */
  sharedTokens: Object.freeze([
    'import os', 'import sys', 'import subprocess', 'subprocess.', 'os.system',
    'exec(', 'eval(', '__import__', 'with open', 'open(', 'write(', 'read(',
    'debug crash', 'debug break', 'assert false', 'check(false)', 'obj garbage',
    'obj list', 'memreport',
  ]),
  /**
   * Python spellings TypeScript matches by regex and the plugin does not carry as a
   * fixed substring. Without these the flexible-pattern rules are never exercised
   * and the differential silently stops asking about a whole rule family.
   */
  flexiblePython: Object.freeze([
    'import  os', 'import   sys', 'import importlib', 'import shutil',
    'from os import path', 'from  subprocess  import run',
    'exec (', 'eval (', 'open (', 'write (', 'read (', 'system(',
  ]),
  /** Commands that must stay usable; a policy that blocks these is broken too. */
  benign: Object.freeze([
    'stat fps', 'stat unit', 'show collision', 'r.screenpercentage 100',
    'showflag.bounds 1', 'slomo 1', 'fx.niagara.debug 0', 'viewmode lit',
    't.maxfps 60', 'summon staticmeshactor',
  ]),
});

/** @param {import('./fuzz-random.mjs').Rng} rng @returns {string} one adversarial text fragment */
export function adversarialFragment(rng) {
  return rng.pick(ADVERSARIAL_TEXT);
}

/**
 * A string built from benign runs and adversarial fragments. Bounded at 64 units:
 * an unbounded generator turns a shrink loop into a timeout and buries the finding.
 * @param {import('./fuzz-random.mjs').Rng} rng
 */
export function fuzzString(rng) {
  const parts = rng.list(rng.int(0, 5), (stream) => stream.weighted([
    [5, () => stream.pick(['a', 'name', 'Test', 'MCPTest', '0', 'x_1'])],
    [3, () => adversarialFragment(stream)],
    [1, () => String.fromCodePoint(stream.int(0x20, 0x10ffff - 0x800))],
  ])());
  return parts.join('').slice(0, 64);
}

/** @param {import('./fuzz-random.mjs').Rng} rng @returns {unknown} a number OR a numeric string, deliberately mixed */
export function fuzzNumeric(rng) {
  return rng.bool(0.5) ? rng.pick(ADVERSARIAL_NUMBERS) : rng.pick(ADVERSARIAL_NUMERIC_STRINGS);
}

/**
 * @typedef {{ path: string, intent: 'in-prefix'|'escape'|'malformed' }} PathCase
 */

/** @param {string} path @param {'in-prefix'|'escape'|'malformed'} intent @returns {PathCase} */
function pathCase(path, intent) {
  return { path, intent };
}

/**
 * A `/Game`-shaped asset path, sometimes legal and sometimes an escape attempt.
 * `intent` records why the generator BUILT it, never the expected verdict —
 * deciding the verdict is the property's job, and a generator that pre-judged
 * would only ever confirm itself.
 * @param {import('./fuzz-random.mjs').Rng} rng
 * @returns {PathCase}
 */
export function fuzzAssetPath(rng) {
  /** @type {readonly (readonly [number, () => PathCase])[]} */
  const table = [
    [4, () => {
      const segments = rng.list(rng.int(1, 3), (stream) => stream.pick(['MCPTest', 'Fuzz', 'A', 'Nested', 'x']));
      return pathCase(`/Game/${segments.join('/')}`, 'in-prefix');
    }],
    [3, () => pathCase(`/Game/${rng.pick(TRAVERSAL_FRAGMENTS)}/Secret`, 'escape')],
    [2, () => pathCase(`${rng.pick(PATH_ROOTS)}/Thing`, 'malformed')],
    [1, () => pathCase(`/Game/${fuzzString(rng)}`, 'malformed')],
  ];
  return rng.weighted(table)();
}

/**
 * A console command assembled from atoms, plus a declared reason it was built.
 * `class` names WHY the command was generated so a divergence can be attributed
 * to a rule family instead of a mystery string.
 * @param {import('./fuzz-random.mjs').Rng} rng
 * @returns {{ command: string, class: string }}
 */
export function fuzzConsoleCommand(rng) {
  return rng.weighted([
    [3, () => ({ command: rng.pick(COMMAND_ATOMS.benign), class: 'benign' })],
    [2, () => ({ command: `${rng.pick(COMMAND_ATOMS.sharedPython)} ${fuzzString(rng)}`, class: 'shared-python-first' })],
    [2, () => ({
      command: `${rng.pick(COMMAND_ATOMS.benign)}${rng.pick(COMMAND_ATOMS.sharedSeparators)}${rng.pick(COMMAND_ATOMS.benign)}`,
      class: 'shared-separator',
    })],
    [3, () => ({ command: `${rng.pick(COMMAND_ATOMS.nativeFirstToken)} ${fuzzString(rng)}`, class: 'dangerous-first-token' })],
    [3, () => ({ command: `${rng.pick(COMMAND_ATOMS.benign)} ${rng.pick(COMMAND_ATOMS.nativeFirstToken)}`, class: 'dangerous-later-token' })],
    [2, () => ({ command: `${rng.pick(COMMAND_ATOMS.benign)} ${rng.pick(COMMAND_ATOMS.sharedTokens)}`, class: 'shared-token' })],
    // Glued spellings: the token is present as a SUBSTRING but not whitespace-bounded.
    // This is the shape that separates a substring matcher from a bounded one, and
    // without it the two surfaces are never asked the question.
    [2, () => ({ command: `x${rng.pick(COMMAND_ATOMS.sharedTokens)}`, class: 'glued-token' })],
    [2, () => ({ command: rng.pick(COMMAND_ATOMS.flexiblePython), class: 'flexible-python-spelling' })],
    [2, () => ({ command: `${rng.pick(COMMAND_ATOMS.nativeOnlyFirstToken)} ${fuzzString(rng)}`, class: 'native-only-first-token' })],
    [2, () => ({ command: casingMutation(rng, rng.pick(COMMAND_ATOMS.nativeFirstToken)), class: 'casing-mutation' })],
    [2, () => ({ command: `${whitespaceMutation(rng)}${rng.pick(COMMAND_ATOMS.nativeFirstToken)}${whitespaceMutation(rng)}`, class: 'whitespace-mutation' })],
    [1, () => ({ command: fuzzString(rng), class: 'free-text' })],
  ])();
}

/** Case and homoglyph mutations aimed at lowercase-then-compare gates. @param {import('./fuzz-random.mjs').Rng} rng @param {string} word */
export function casingMutation(rng, word) {
  return rng.weighted([
    [3, () => word.toUpperCase()],
    [2, () => [...word].map((ch, index) => (index % 2 === 0 ? ch.toUpperCase() : ch)).join('')],
    [1, () => word.replace(/i/gu, '\u0130')],
    [1, () => word.replace(/i/gu, '\u0131')],
    [1, () => `${word}\u200B`],
  ])();
}

/** Leading/trailing whitespace variants, including the ones `trim()` does not remove. @param {import('./fuzz-random.mjs').Rng} rng */
export function whitespaceMutation(rng) {
  return rng.pick(['', ' ', '  ', '\t', '\u3000', '\u00A0', '\u2007', '\u200B']);
}
