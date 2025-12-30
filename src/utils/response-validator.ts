import Ajv, { ValidateFunction } from 'ajv';
import { Logger } from './logger.js';
import { cleanObject } from './safe-json.js';
import { wasmIntegration } from '../wasm/index.js';

const log = new Logger('ResponseValidator');

function isRecord(value: unknown): value is Record<string, unknown> {
  return !!value && typeof value === 'object' && !Array.isArray(value);
}

function normalizeText(text: string): string {
  return text.replace(/\s+/g, ' ').trim();
}

function buildSummaryText(toolName: string, payload: unknown): string {
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

  // Recursively flatten data/result wrappers into effective payload
  const effectivePayload: Record<string, unknown> = { ...(payload as object) };

  const flattenWrappers = (obj: Record<string, unknown>, depth = 0): void => {
    if (depth > 5) return; // Prevent infinite loops
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

  const parts: string[] = [];
  const addedKeys = new Set<string>();

  // Keys to skip (internal/redundant)
  const skipKeys = new Set(['requestId', 'type', 'data', 'result', 'warnings']);

  // Helper to format a value for display
  const formatValue = (val: unknown): string => {
    if (val === null || val === undefined) return '';
    if (typeof val === 'string') return val.length > 150 ? val.slice(0, 150) + '...' : val;
    if (typeof val === 'number' || typeof val === 'boolean') return String(val);

    // Handle arrays - show items with names/paths
    if (Array.isArray(val)) {
      if (val.length === 0) return '[] (0)';
      const items = val.slice(0, 30).map(v => {
        if (isRecord(v)) {
          // Try common identifier fields
          return v.name || v.path || v.id || v.nodeId || v.nodeName || v.className ||
            v.displayName || v.type || v.assetPath || v.objectPath ||
            JSON.stringify(v).slice(0, 50);
        }
        return String(v);
      });
      const suffix = val.length > 30 ? `, ... (+${val.length - 30} more)` : '';
      return `[${items.join(', ')}${suffix}] (${val.length})`;
    }

    // Handle transform-like objects (location/rotation/scale)
    if (isRecord(val)) {
      const keys = Object.keys(val);
      // Check if it looks like a 3D vector/transform
      if (keys.some(k => ['x', 'y', 'z', 'pitch', 'yaw', 'roll'].includes(k))) {
        const x = val.x ?? val.pitch ?? 0;
        const y = val.y ?? val.yaw ?? 0;
        const z = val.z ?? val.roll ?? 0;
        return `[${x}, ${y}, ${z}]`;
      }
      // Generic object - show key=value pairs
      const entries = Object.entries(val).slice(0, 8);
      const formatted = entries.map(([k, v]) => {
        const vStr = typeof v === 'object' ? JSON.stringify(v).slice(0, 40) : String(v);
        return `${k}=${vStr}`;
      });
      return `{ ${formatted.join(', ')}${keys.length > 8 ? ' ...' : ''} }`;
    }

    return String(val);
  };

  // Process all keys in priority order
  // 1. First add 'success' and 'error' at the start
  for (const key of ['success', 'error']) {
    if (effectivePayload[key] !== undefined && !addedKeys.has(key)) {
      const formatted = formatValue(effectivePayload[key]);
      if (formatted) {
        parts.push(`${key}: ${formatted}`);
        addedKeys.add(key);
      }
    }
  }

  // 2. Then add ALL other keys dynamically
  let hasArrays = false;
  for (const [key, val] of Object.entries(effectivePayload)) {
    if (addedKeys.has(key)) continue;
    if (skipKeys.has(key)) continue;
    if (val === undefined || val === null) continue;
    if (typeof val === 'string' && val.trim() === '') continue;

    // Skip 'message' for now - handle later to avoid duplication
    if (key === 'message') continue;

    // Track if we have arrays (to skip duplicate count/totalCount later)
    if (Array.isArray(val) && val.length > 0) hasArrays = true;

    // Skip count/totalCount if we already have arrays showing counts
    if ((key === 'count' || key === 'totalCount') && hasArrays) continue;

    const formatted = formatValue(val);
    if (formatted) {
      parts.push(`${key}: ${formatted}`);
      addedKeys.add(key);
    }
  }

  // 3. Handle message last - but skip if it duplicates existing info
  const message = typeof effectivePayload.message === 'string' ? normalizeText(effectivePayload.message) : '';
  if (message && message.toLowerCase() !== 'success') {
    // Skip if message duplicates count info
    const isDuplicateInfo = /^(found|listed|retrieved|got|loaded|created|deleted|saved|spawned)\s+\d+/i.test(message) ||
      /Folders:\s*\[/.test(message) ||
      /\d+\s+(assets?|folders?|items?|actors?|components?)\s+(and|in|at)/i.test(message);

    // Also skip if message content is already represented in parts
    const messageInParts = parts.some(p => p.toLowerCase().includes(message.toLowerCase().slice(0, 30)));

    if (!isDuplicateInfo && !messageInParts) {
      parts.push(message);
    }
  }

  // 4. Warnings at end
  const warnings = Array.isArray(effectivePayload.warnings) ? effectivePayload.warnings : [];
  if (warnings.length > 0) {
    parts.push(`Warnings: ${warnings.map((w: unknown) => typeof w === 'string' ? w : JSON.stringify(w)).join('; ')}`);
  }

  return parts.length > 0 ? parts.join(' | ') : `${toolName} responded`;
}

/**
 * Response Validator for MCP Tool Outputs
 * Validates tool responses against their defined output schemas
 */
export class ResponseValidator {
  // Ajv instance - using Ajv.default for ESM/CJS interop
  private ajv: Ajv.default;
  private validators: Map<string, ValidateFunction> = new Map();

  constructor() {
    // Ajv exports differ between ESM and CJS - handle both patterns
    const AjvClass = (Ajv as unknown as { default: typeof Ajv.default }).default ?? Ajv.default;
    this.ajv = new AjvClass({
      allErrors: true,
      verbose: true,
      strict: true // Enforce strict schema validation
    });
  }

  /**
   * Register a tool's output schema for validation
   */
  registerSchema(toolName: string, outputSchema: Record<string, unknown>) {
    if (!outputSchema) {
      log.warn(`No output schema defined for tool: ${toolName}`);
      return;
    }

    try {
      const validator = this.ajv.compile(outputSchema);
      this.validators.set(toolName, validator);
      // Demote per-tool schema registration to debug to reduce log noise
      log.debug(`Registered output schema for tool: ${toolName}`);
    } catch (_error) {
      log.error(`Failed to compile output schema for ${toolName}:`, _error);
    }
  }

  /**
   * Validate a tool's response against its schema
   */
  async validateResponse(toolName: string, response: unknown): Promise<{
    valid: boolean;
    errors?: string[];
    structuredContent?: unknown;
  }> {
    const validator = this.validators.get(toolName);

    if (!validator) {
      log.debug(`No validator found for tool: ${toolName}`);
      return { valid: true }; // Pass through if no schema defined
    }

    // Extract structured content from response
    let structuredContent = response;
    const responseObj = response as Record<string, unknown> | null;

    // If response has MCP format with content array
    if (responseObj && responseObj.content && Array.isArray(responseObj.content)) {
      // Try to extract structured data from text content
      const textContent = responseObj.content.find((c: unknown) => {
        const cObj = c as Record<string, unknown> | null;
        return cObj?.type === 'text';
      }) as Record<string, unknown> | undefined;
      if (textContent?.text) {
        const rawText = String(textContent.text);
        const trimmed = rawText.trim();
        const looksLikeJson = trimmed.startsWith('{') || trimmed.startsWith('[');

        if (looksLikeJson) {
          try {
            // Check if text is JSON - use WASM for high-performance parsing (5-8x faster)
            structuredContent = await wasmIntegration.parseProperties(rawText);
          } catch {
            // If JSON parsing fails, fall back to using the full response without
            // emitting noisy parse errors for plain-text messages.
            structuredContent = response;
          }
        } else {
          // Plain-text summary or error message; keep the original structured
          // response object instead of attempting JSON parsing.
          structuredContent = response;
        }
      }
    }

    const valid = validator(structuredContent);

    if (!valid) {
      const errors = validator.errors?.map((err: { instancePath?: string; message?: string }) =>
        `${err.instancePath || 'root'}: ${err.message}`
      );

      log.warn(`Response validation failed for ${toolName}:`, errors);

      return {
        valid: false,
        errors,
        structuredContent
      };
    }

    return {
      valid: true,
      structuredContent
    };
  }

  /**
   * Wrap a tool response with validation and MCP-compliant content shape.
   *
   * MCP tools/call responses must contain a `content` array. Many internal
   * handlers return structured JSON objects (e.g., { success, message, ... }).
   * This wrapper serializes such objects into a single text block while keeping
   * existing `content` responses intact.
   */
  async wrapResponse(toolName: string, response: unknown): Promise<Record<string, unknown>> {
    // Ensure response is safe to serialize first
    let safeResponse = response;
    try {
      if (response && typeof response === 'object') {
        JSON.stringify(response);
      }
    } catch (_error) {
      log.error(`Response for ${toolName} contains circular references, cleaning...`);
      safeResponse = cleanObject(response);
    }

    const responseObj = safeResponse as Record<string, unknown> | null;

    // If handler already returned MCP content, keep it as-is (still validate)
    const alreadyMcpShaped = responseObj && typeof responseObj === 'object' && Array.isArray(responseObj.content);

    // Choose the payload to validate: if already MCP-shaped, validate the
    // structured content extracted from text; otherwise validate the object directly.
    const validation = await this.validateResponse(toolName, safeResponse);
    const structuredPayload = validation.structuredContent;

    if (!validation.valid) {
      log.warn(`Tool ${toolName} response validation failed:`, validation.errors);
    }

    // If it's already MCP-shaped, return as-is (optionally append validation meta)
    if (alreadyMcpShaped && responseObj) {
      if (structuredPayload !== undefined && responseObj.structuredContent === undefined) {
        try {
          responseObj.structuredContent = structuredPayload && typeof structuredPayload === 'object'
            ? cleanObject(structuredPayload)
            : structuredPayload;
        } catch { }
      }
      // Promote failure semantics to top-level isError when obvious
      try {
        const sc = (responseObj.structuredContent || structuredPayload || {}) as Record<string, unknown>;
        const hasExplicitFailure = (typeof sc.success === 'boolean' && sc.success === false) || (typeof sc.error === 'string' && (sc.error as string).length > 0);
        if (hasExplicitFailure && responseObj.isError !== true) {
          responseObj.isError = true;
        }
      } catch { }
      if (!validation.valid) {
        try {
          responseObj._validation = { valid: false, errors: validation.errors };
        } catch { }
      }
      return responseObj;
    }

    // Otherwise, wrap structured result into MCP content
    const summarySource = structuredPayload !== undefined ? structuredPayload : safeResponse;
    let text = buildSummaryText(toolName, summarySource);
    if (!text || !text.trim()) {
      text = buildSummaryText(toolName, safeResponse);
    }

    const wrapped: Record<string, unknown> = {
      content: [
        { type: 'text', text }
      ]
    };

    // Surface a top-level success flag when available so clients and test
    // harnesses do not have to infer success from the absence of isError.
    try {
      const structPayloadObj = structuredPayload as Record<string, unknown> | null;
      const safeResponseObj = safeResponse as Record<string, unknown> | null;
      if (structPayloadObj && typeof structPayloadObj.success === 'boolean') {
        wrapped.success = Boolean(structPayloadObj.success);
      } else if (safeResponseObj && typeof safeResponseObj.success === 'boolean') {
        wrapped.success = Boolean(safeResponseObj.success);
      }
    } catch { }

    if (structuredPayload !== undefined) {
      try {
        wrapped.structuredContent = structuredPayload && typeof structuredPayload === 'object'
          ? cleanObject(structuredPayload)
          : structuredPayload;
      } catch {
        wrapped.structuredContent = structuredPayload;
      }
    } else if (safeResponse && typeof safeResponse === 'object') {
      try {
        wrapped.structuredContent = cleanObject(safeResponse);
      } catch {
        wrapped.structuredContent = safeResponse;
      }
    }

    // Promote failure semantics to top-level isError when obvious
    try {
      const sc = (wrapped.structuredContent || {}) as Record<string, unknown>;
      const hasExplicitFailure = (typeof sc.success === 'boolean' && sc.success === false) || (typeof sc.error === 'string' && (sc.error as string).length > 0);
      if (hasExplicitFailure) {
        wrapped.isError = true;
      }
    } catch { }

    if (!validation.valid) {
      wrapped._validation = { valid: false, errors: validation.errors };
    }

    // Mark explicit error when success is false to avoid false positives in
    // clients that check only for the absence of isError.
    try {
      const s = wrapped.success;
      if (typeof s === 'boolean' && s === false) {
        wrapped.isError = true;
      }
    } catch { }

    return wrapped;
  }

  /**
   * Get validation statistics
   */
  getStats() {
    return {
      totalSchemas: this.validators.size,
      tools: Array.from(this.validators.keys())
    };
  }
}

// Singleton instance
export const responseValidator = new ResponseValidator();
