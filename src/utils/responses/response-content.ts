import { isRecord } from '../validation/type-guards.js';

const SUMMARY_SKIP_KEYS = new Set(['requestId', 'type', 'data', 'result', 'warnings', 'imageBase64']);

function normalizeText(text: string): string {
  return text.replace(/\s+/g, ' ').trim();
}

export function hasExplicitFailurePayload(payload: unknown): boolean {
  if (!isRecord(payload)) return false;
  return (typeof payload.success === 'boolean' && payload.success === false) ||
    (typeof payload.error === 'string' && payload.error.length > 0);
}

function scalarToText(value: unknown): string | undefined {
  if (typeof value === 'string') return value;
  if (typeof value === 'number' || typeof value === 'boolean' || typeof value === 'bigint') return String(value);
  return undefined;
}

function formatNestedValue(value: unknown): string {
  const scalar = scalarToText(value);
  if (scalar !== undefined) return scalar;
  if (Array.isArray(value)) return `${value.length} item${value.length === 1 ? '' : 's'}`;
  if (isRecord(value)) return '{...}';
  if (value === null) return 'null';
  return String(value);
}

function formatRecordListItem(record: Record<string, unknown>): string {
  const pinName = scalarToText(record.pinName);
  if (pinName !== undefined) {
    const pinParts = [`pinName=${pinName}`];
    for (const key of ['direction', 'pinType', 'defaultValue']) {
      const value = scalarToText(record[key]);
      if (value !== undefined) pinParts.push(`${key}=${value}`);
    }
    if (Array.isArray(record.linkedTo)) {
      const links = record.linkedTo;
      if (links.length === 0) {
        pinParts.push('linkedTo=0');
      } else {
        // Render the link TARGETS, not just a count: the target nodeId is the argument every follow-up
        // graph call takes (connect/delete/get_node_details), and a bare count forces clients that only
        // read the text channel to re-query per node. 8-hex GUID prefixes are directly consumable —
        // FindNode resolves unique prefixes of >=8 hex chars (BlueprintGraphHandlersContextEditor.cpp).
        const shown = links.slice(0, 5).map(link => {
          if (isRecord(link)) {
            const nodeId = typeof link.nodeId === 'string' ? link.nodeId.slice(0, 8) : '';
            const pin = scalarToText(link.pinName);
            if (nodeId !== '') return pin !== undefined && pin !== '' ? `${nodeId}.${pin}` : nodeId;
          }
          return '?';
        });
        const spill = links.length > 5 ? ', ...' : '';
        pinParts.push(`linkedTo=[${shown.join(', ')}${spill}] (${links.length})`);
      }
    }
    return `{ ${pinParts.join(', ')} }`;
  }

  for (const key of ['name', 'path', 'id', 'nodeId', 'nodeName', 'className', 'displayName', 'type', 'assetPath', 'objectPath']) {
    const value = scalarToText(record[key]);
    if (value !== undefined && value.trim() !== '') return value;
  }

  const entries = Object.entries(record).filter(([, value]) => value !== undefined && value !== null).slice(0, 4);
  if (entries.length === 0) return '{}';
  const suffix = Object.keys(record).length > entries.length ? ' ...' : '';
  return `{ ${entries.map(([key, value]) => `${key}=${formatNestedValue(value)}`).join(', ')}${suffix} }`;
}

/**
 * Program output (`output` from execute_python / console commands) gets a larger summary budget than other
 * string fields: it is the payload the caller explicitly asked the tool to produce, and clients that render
 * only the text content (many MCP clients never surface structuredContent to the model) would otherwise see
 * an unusable 150-char head and re-query in a loop. Truncation above this budget is announced explicitly —
 * with the shown/total sizes and a pointer to structuredContent — never a bare '...'.
 */
const OUTPUT_SUMMARY_LIMIT = 2000;

function formatOutputValue(val: unknown): string {
  if (typeof val !== 'string') return formatValue(val);
  if (val.length <= OUTPUT_SUMMARY_LIMIT) return val;
  return `${val.slice(0, OUTPUT_SUMMARY_LIMIT)}... [output truncated: showing ${OUTPUT_SUMMARY_LIMIT} of ${val.length} chars — full text in structuredContent]`;
}

function formatValue(val: unknown): string {
  if (val === null || val === undefined) return '';
  if (typeof val === 'string') return val.length > 150 ? val.slice(0, 150) + '...' : val;
  if (typeof val === 'number' || typeof val === 'boolean') return String(val);

  if (Array.isArray(val)) {
    if (val.length === 0) return '[] (0)';
    const items = val.slice(0, 30).map(v => isRecord(v) ? formatRecordListItem(v) : String(v));
    const suffix = val.length > 30 ? `, ... (+${val.length - 30} more)` : '';
    return `[${items.join(', ')}${suffix}] (${val.length})`;
  }

  if (isRecord(val)) {
    const keys = Object.keys(val);
    if (keys.some(k => ['x', 'y', 'z', 'pitch', 'yaw', 'roll'].includes(k))) {
      const x = val.x ?? val.pitch ?? 0;
      const y = val.y ?? val.yaw ?? 0;
      const z = val.z ?? val.roll ?? 0;
      return `[${x}, ${y}, ${z}]`;
    }
    const entries = Object.entries(val).slice(0, 8);
    const formatted = entries.map(([k, v]) => `${k}=${formatNestedValue(v)}`);
    return `{ ${formatted.join(', ')}${keys.length > 8 ? ' ...' : ''} }`;
  }

  return String(val);
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

export function buildSummaryText(toolName: string, payload: unknown): string {
  if (typeof payload === 'string') {
    const normalized = payload.trim();
    return normalized || `${toolName} responded`;
  }

  if (typeof payload === 'number' || typeof payload === 'bigint' || typeof payload === 'boolean') {
    return `${toolName} responded: ${payload}`;
  }

  if (!isRecord(payload)) {
    return `${toolName} responded`;
  }

  const effectivePayload = flattenPayloadWrappers(payload);
  const parts: string[] = [];
  const addedKeys = new Set<string>();

  for (const key of ['success', 'error']) {
    if (effectivePayload[key] !== undefined && !addedKeys.has(key)) {
      const formatted = formatValue(effectivePayload[key]);
      if (formatted) {
        parts.push(`${key}: ${formatted}`);
        addedKeys.add(key);
      }
    }
  }

  let hasArrays = false;
  for (const [key, val] of Object.entries(effectivePayload)) {
    if (addedKeys.has(key)) continue;
    if (SUMMARY_SKIP_KEYS.has(key)) continue;
    if (val === undefined || val === null) continue;
    if (typeof val === 'string' && val.trim() === '') continue;
    if (key === 'message') continue;
    if (Array.isArray(val) && val.length > 0) hasArrays = true;
    if ((key === 'count' || key === 'totalCount') && hasArrays) continue;

    const formatted = key === 'output' ? formatOutputValue(val) : formatValue(val);
    if (formatted) {
      parts.push(`${key}: ${formatted}`);
      addedKeys.add(key);
    }
  }

  const message = typeof effectivePayload.message === 'string' ? normalizeText(effectivePayload.message) : '';
  if (message && message.toLowerCase() !== 'success') {
    const isDuplicateInfo = /^(found|listed|retrieved|got|loaded|created|deleted|saved|spawned)\s+\d+/i.test(message) ||
      /Folders:\s*\[/.test(message) ||
      /\d+\s+(assets?|folders?|items?|actors?|components?)\s+(and|in|at)/i.test(message);
    const messageInParts = parts.some(p => p.toLowerCase().includes(message.toLowerCase().slice(0, 30)));

    if (!isDuplicateInfo && !messageInParts) {
      parts.push(message);
    }
  }

  const warnings = Array.isArray(effectivePayload.warnings) ? effectivePayload.warnings : [];
  if (warnings.length > 0) {
    parts.push(`Warnings: ${warnings.map((w: unknown) => typeof w === 'string' ? w : JSON.stringify(w)).join('; ')}`);
  }

  return parts.length > 0 ? parts.join(' | ') : `${toolName} responded`;
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
