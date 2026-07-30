// Bounds and secret redaction for receipt content. High-cardinality arrays are
// capped and free-text fields are truncated so an untrusted or runaway handler
// result can never balloon a receipt, and secret-looking `token=`/`secret:`/
// `Bearer x` assignments are masked so a credential echoed into a warning or a
// change entry never survives serialization.

export const MAX_RECEIPT_ARRAY = 200;
export const MAX_RECEIPT_TEXT = 2048;

// Matches a secret keyword, its `=`/`:` separator, an optional surrounding quote
// (so JSON-like `"token":"x"` is caught) and an optional `Bearer ` auth scheme
// (so `Authorization: Bearer <token>` masks the TOKEN, not the scheme word), then
// captures the value run up to the next quote/space. Prefix is preserved, value
// masked. Kept narrow (value stops at a quote) so ordinary prose is not mangled.
const SECRET_ASSIGNMENT =
  /((?:token|secret|password|passwd|pwd|api[_-]?key|apikey|authorization)["']?\s*[:=]\s*["']?(?:Bearer\s+)?)([^\s"']+)/gi;
const BEARER = /(\bBearer\s+)([^\s"']+)/gi;

export const REDACTED = '[REDACTED]';

export function maskSecrets(value: string): string {
  return value
    .replace(SECRET_ASSIGNMENT, (_m, prefix: string) => `${prefix}${REDACTED}`)
    .replace(BEARER, (_m, prefix: string) => `${prefix}${REDACTED}`);
}

export function redactText(value: string): string {
  const masked = maskSecrets(value);
  return masked.length > MAX_RECEIPT_TEXT ? `${masked.slice(0, MAX_RECEIPT_TEXT - 1)}\u2026` : masked;
}

export function boundStrings(values: readonly string[]): string[] {
  return values.slice(0, MAX_RECEIPT_ARRAY).map(redactText);
}

export function boundArray<T>(values: readonly T[]): T[] {
  return values.slice(0, MAX_RECEIPT_ARRAY);
}

// The same keyword vocabulary as SECRET_ASSIGNMENT, matched against an object
// KEY rather than inside a string. `maskSecrets` needs a `keyword<sep>value` run
// within one string, so a credential that arrives as a real JSON value has no
// keyword context left to match; the key it hangs under is the only signal, and
// the invariant (a secret never reaches a receipt) makes that signal sufficient.
const SECRET_KEY_WORDS = new Set([
  'token',
  'secret',
  'password',
  'passwd',
  'pwd',
  'apikey',
  'authorization',
  'credential',
  'privatekey',
  'accesskey',
  'signingkey',
]);

// Heads that carry no meaning of their own, so the real head sits one word to
// their left: `secretValue` is still a secret, `tokenString` is still a token.
const TRANSPARENT_HEADS = new Set(['value', 'data', 'string', 'text', 'raw', 'plain']);

// Heads that make a compound a MEASUREMENT or a STATUS rather than a credential:
// `tokenCount` is a count, `secretsFound` is a finding. This list is the ONLY
// thing that suppresses masking once a secret word is present, so the default is
// to mask - `secretKey`, `passwordHash` and `authorizationHeader` all name real
// credentials, and an unrecognised head must never be assumed harmless.
const MEASUREMENT_HEADS = new Set([
  'count', 'budget', 'length', 'size', 'limit', 'total', 'max', 'min',
  'index', 'order', 'position', 'offset', 'depth', 'age', 'ttl',
  'found', 'required', 'enabled', 'disabled', 'expired', 'valid', 'present',
  'missing', 'used', 'remaining', 'supported', 'allowed',
  'name', 'id', 'type', 'kind', 'label', 'status', 'state', 'mode', 'policy',
  'rule', 'scheme', 'algorithm', 'format', 'source', 'reason', 'message',
  'error', 'version', 'timestamp', 'time', 'date', 'duration', 'at',
]);

// Split on camelCase and any non-alphanumeric run. Matching WHOLE words (not
// substrings) is what keeps `tokenizer`, `passwordless` and `unauthorized` out
// of the secret set.
function keyWords(key: string): readonly string[] {
  return key
    .replace(/([a-z0-9])([A-Z])/g, '$1 $2')
    .split(/[^A-Za-z0-9]+/)
    .filter((word) => word.length > 0)
    .map((word) => word.toLowerCase());
}

// Depluralisation is tried as an ALTERNATIVE rather than applied up front,
// because stripping unconditionally mangles words that merely end in `s`:
// `status` became `statu` and stopped matching, which silently masked
// `secretStatus`.
const singular = (word: string): string =>
  word.length > 1 && word.endsWith('s') ? word.slice(0, -1) : word;

const inSet = (set: ReadonlySet<string>, word: string): boolean =>
  set.has(word) || set.has(singular(word));

function namesCredential(words: readonly string[]): boolean {
  return words.some((word, index) => {
    if (inSet(SECRET_KEY_WORDS, word)) return true;
    const next = words[index + 1];
    if (next === undefined) return false;
    return (
      inSet(SECRET_KEY_WORDS, `${word}${next}`) ||
      inSet(SECRET_KEY_WORDS, `${singular(word)}${singular(next)}`)
    );
  });
}

// Masking is FAIL-CLOSED: once any whole word names a credential, the value is
// masked unless the head word proves the compound measures or describes it.
// `secretKey`, `passwordHash`, `authorizationHeader` and `credentialBytes` all
// carry the credential itself, so an unrecognised head must never suppress the
// mask; only MEASUREMENT_HEADS may, which is what spares `tokenCount`.
function isSecretKey(key: string): boolean {
  const words = keyWords(key);
  if (!namesCredential(words)) return false;
  let end = words.length;
  while (end > 1 && inSet(TRANSPARENT_HEADS, words[end - 1] ?? '')) end -= 1;
  return !inSet(MEASUREMENT_HEADS, words[end - 1] ?? '');
}

// Deep secret masking for the outer legacy envelope. Masks secret-looking string
// leaves in place WITHOUT bounding arrays or truncating text, so a token echoed
// by a handler is scrubbed while legitimate (possibly large) result data is
// preserved verbatim - the RESULT_TOO_LARGE gate bounds total size separately.
// This is what stops the outer envelope from re-emitting an unredacted copy of
// content already sanitized inside the receipt.
//
// A secret-named KEY masks its entire value whatever the value's shape: an
// object, an array and a number are all just as capable of carrying a credential
// as a string, and recursing into them would only find leaves that no longer
// carry the keyword context the string masker needs.
// The size gate cannot substitute for a depth gate: 5,000 levels of nesting is
// only ~10,000 chars, well inside MAX_EXECUTION_RESULT_CHARS, so the stack
// overflows before any size check runs. Only CONTAINERS are capped, and the cap
// fails closed - a subtree too deep to inspect is masked, since returning it
// unread would emit exactly the content this function exists to scrub.
export const MAX_RECEIPT_DEPTH = 100;

export function maskSecretsDeep(value: unknown, depth = 0): unknown {
  if (typeof value === 'string') return maskSecrets(value);
  if (Array.isArray(value)) {
    if (depth >= MAX_RECEIPT_DEPTH) return REDACTED;
    return value.map((entry) => maskSecretsDeep(entry, depth + 1));
  }
  if (value !== null && typeof value === 'object') {
    if (depth >= MAX_RECEIPT_DEPTH) return REDACTED;
    const out: Record<string, unknown> = {};
    for (const [key, entry] of Object.entries(value)) {
      out[key] = isSecretKey(key) ? REDACTED : maskSecretsDeep(entry, depth + 1);
    }
    return out;
  }
  return value;
}
