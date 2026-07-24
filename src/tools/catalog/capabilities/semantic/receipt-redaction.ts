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

export function maskSecrets(value: string): string {
  return value
    .replace(SECRET_ASSIGNMENT, (_m, prefix: string) => `${prefix}[REDACTED]`)
    .replace(BEARER, (_m, prefix: string) => `${prefix}[REDACTED]`);
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

// Deep secret masking for the outer legacy envelope. Masks secret-looking string
// leaves in place WITHOUT bounding arrays or truncating text, so a token echoed
// by a handler is scrubbed while legitimate (possibly large) result data is
// preserved verbatim - the RESULT_TOO_LARGE gate bounds total size separately.
// This is what stops the outer envelope from re-emitting an unredacted copy of
// content already sanitized inside the receipt.
export function maskSecretsDeep(value: unknown): unknown {
  if (typeof value === 'string') return maskSecrets(value);
  if (Array.isArray(value)) return value.map(maskSecretsDeep);
  if (value !== null && typeof value === 'object') {
    const out: Record<string, unknown> = {};
    for (const [key, entry] of Object.entries(value)) out[key] = maskSecretsDeep(entry);
    return out;
  }
  return value;
}
