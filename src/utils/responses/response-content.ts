import { isRecord } from '../validation/type-guards.js';

export function hasExplicitFailurePayload(payload: unknown): boolean {
  if (!isRecord(payload)) return false;
  return (typeof payload.success === 'boolean' && payload.success === false) ||
    (typeof payload.error === 'string' && payload.error.length > 0);
}

/** Collapses `data`/`result` wrappers onto the root. Inner keys win; input is not mutated. */
export function flattenPayloadWrappers(payload: Record<string, unknown>): Record<string, unknown> {
  const effectivePayload = { ...payload };

  const flattenWrappers = (obj: Record<string, unknown>, depth = 0): void => {
    if (depth > 5) return;
    if (isRecord(obj.data)) {
      Object.assign(obj, obj.data);
      delete obj.data;
      flattenWrappers(obj, depth + 1);
    }
    if (isRecord(obj.result)) {
      Object.assign(obj, obj.result);
      delete obj.result;
      flattenWrappers(obj, depth + 1);
    }
  };

  flattenWrappers(effectivePayload);
  return effectivePayload;
}

function findImagePayload(payload: unknown, depth = 0): Record<string, unknown> | undefined {
  if (!isRecord(payload) || depth > 5) return undefined;

  if (typeof payload.imageBase64 === 'string' && payload.imageBase64.trim() !== '') {
    return payload;
  }

  const resultPayload = findImagePayload(payload.result, depth + 1);
  if (resultPayload) return resultPayload;

  return findImagePayload(payload.data, depth + 1);
}

export function buildImageContent(payload: unknown): Record<string, string> | undefined {
  const imagePayload = findImagePayload(payload);
  if (!imagePayload) return undefined;

  const imageBase64 = imagePayload.imageBase64;
  if (typeof imageBase64 !== 'string' || imageBase64.trim() === '') return undefined;

  const mimeType = typeof imagePayload.mimeType === 'string' && imagePayload.mimeType.trim() !== ''
    ? imagePayload.mimeType.trim()
    : 'image/png';

  return {
    type: 'image',
    data: imageBase64,
    mimeType
  };
}
