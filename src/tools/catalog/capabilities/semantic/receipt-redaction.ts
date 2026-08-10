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

// Qualifiers that precede a credential noun. They name no secret alone, so they
// only matter when reuniting a compound that carried no separator.
const SECRET_QUALIFIERS = new Set([
  'api', 'access', 'auth', 'private', 'public', 'client', 'server', 'session',
  'user', 'admin', 'root', 'master', 'signing', 'refresh', 'bearer', 'oauth',
  'app', 'service', 'encryption', 'shared', 'secret',
]);

// Credential nouns that can close a compound. `secret` + `key` is a credential
// even though `key` alone is far too generic to live in SECRET_KEY_WORDS.
const CREDENTIAL_TAILS = new Set([
  'key', 'token', 'secret', 'password', 'passwd', 'pwd', 'credential',
  'authorization', 'signature', 'cert', 'certificate',
  'hash', 'bytes', 'digest', 'header', 'blob',
]);

// Every word the splitter is allowed to recognise. Restricting the halves to a
// closed vocabulary is what stops `tokenizer` becoming `token` + `izer`.
const SPLITTABLE_WORDS: ReadonlySet<string> = new Set([
  ...SECRET_KEY_WORDS, ...SECRET_QUALIFIERS, ...CREDENTIAL_TAILS,
  ...MEASUREMENT_HEADS, ...TRANSPARENT_HEADS,
]);

// `SECRETKEY` and `ACCESSTOKEN` carry no camelCase boundary and no separator, so
// the splitter above sees one unrecognised word and the key escapes masking
// entirely - while `SECRET_KEY` and `secretKey` are both masked. Recover the
// boundary by splitting a single run into known words, at least one of which
// names a credential. EVERY part must be in the closed vocabulary, so
// `tokenizer` and `passwordless` still refuse to split and stay unmasked.
// Returning the WORDS (rather than a boolean) keeps the head logic below intact:
// `TOKENCOUNT` decomposes to token + count and is spared as a measurement,
// exactly as `tokenCount` already was.
//
// Two parts cannot reach `APIACCESSTOKEN`, so the search runs to three - fewest
// parts first, since a coarser split is the likelier reading of a real key.
const MAX_COMPOUND_PARTS = 3;

// Leftmost cut first at every level, so one word always yields the same split.
function segmentations(word: string, parts: number): readonly (readonly string[])[] {
  if (parts === 1) return SPLITTABLE_WORDS.has(word) ? [[word]] : [];
  const found: (readonly string[])[] = [];
  for (let cut = 2; cut <= word.length - 2; cut += 1) {
    const head = word.slice(0, cut);
    if (!SPLITTABLE_WORDS.has(head)) continue;
    for (const rest of segmentations(word.slice(cut), parts - 1)) found.push([head, ...rest]);
  }
  return found;
}

const splitNamesCredential = (parts: readonly string[]): boolean =>
  parts.some((part) => SECRET_KEY_WORDS.has(part)) ||
  CREDENTIAL_TAILS.has(parts[parts.length - 1] ?? '');

function splitCompound(word: string): readonly string[] {
  if (word.length < 6 || SPLITTABLE_WORDS.has(word)) return [word];
  for (let parts = 2; parts <= MAX_COMPOUND_PARTS; parts += 1) {
    const split = segmentations(word, parts).find(splitNamesCredential);
    if (split !== undefined) return split;
  }
  return [word];
}

// Split on camelCase and any non-alphanumeric run. Matching WHOLE words (not
// substrings) is what keeps `tokenizer`, `passwordless` and `unauthorized` out
// of the secret set.
function keyWords(key: string): readonly string[] {
  return key
    .replace(/([a-z0-9])([A-Z])/g, '$1 $2')
    .split(/[^A-Za-z0-9]+/)
    .filter((word) => word.length > 0)
    .flatMap((word) => splitCompound(word.toLowerCase()));
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

// `JSON.parse` creates `__proto__` as a REAL own property, so any rebuild that
// does `out[key] = ...` on a plain object hands that key to Object.prototype's
// setter: the field is silently dropped from the rebuilt object and the object's
// prototype is replaced with whatever the bridge sent. Defining the property
// keeps the value where it belongs and leaves the prototype alone. Only
// `__proto__` needs the slow path; every other key is an ordinary assignment.
export function assignJsonKey(
  target: Record<string, unknown>,
  key: string,
  value: unknown
): void {
  if (key === '__proto__') {
    Object.defineProperty(target, key, {
      value,
      enumerable: true,
      writable: true,
      configurable: true,
    });
    return;
  }
  target[key] = value;
}

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
      assignJsonKey(out, key, isSecretKey(key) ? REDACTED : maskSecretsDeep(entry, depth + 1));
    }
    return out;
  }
  return value;
}
