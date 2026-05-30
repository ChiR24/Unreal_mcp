import { isRecord } from './type-guards.js';

export const REDACTED_IMAGE_PAYLOAD = '<omitted; see image content>';

export function redactImagePayloadForLog(value: unknown): unknown {
  if (Array.isArray(value)) {
    return value.map(item => redactImagePayloadForLog(item));
  }

  if (!isRecord(value)) {
    return value;
  }

  const redacted: Record<string, unknown> = {};
  const isImageContent = value.type === 'image';

  for (const [key, child] of Object.entries(value)) {
    if (key === 'imageBase64' || (isImageContent && key === 'data' && typeof child === 'string')) {
      redacted[key] = REDACTED_IMAGE_PAYLOAD;
    } else {
      redacted[key] = redactImagePayloadForLog(child);
    }
  }

  return redacted;
}

export function redactImagePayloadTextForLog(text: string): string {
  return text
    .replace(/("imageBase64"\s*:\s*")([^"]*)(")/g, `$1${REDACTED_IMAGE_PAYLOAD}$3`)
    .replace(/("data"\s*:\s*")([^"]*)(")/g, `$1${REDACTED_IMAGE_PAYLOAD}$3`);
}
