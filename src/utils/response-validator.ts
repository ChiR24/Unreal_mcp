import Ajv from 'ajv';
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

  // 1. Check for specific "data" or "result" wrapper
  const effectivePayload: Record<string, any> = { ...(payload as object) };
  if (isRecord(effectivePayload.data)) {
    Object.assign(effectivePayload, effectivePayload.data);
  }
  if (isRecord(effectivePayload.result)) {
    Object.assign(effectivePayload, effectivePayload.result);
  }

  const parts: string[] = [];

  // 2. Identify "List" responses (Arrays) - Prioritize showing content
  const listKeys = ['actors', 'levels', 'assets', 'folders', 'blueprints', 'components', 'pawnClasses', 'foliageTypes', 'nodes', 'tracks', 'bindings', 'keys'];
  for (const key of listKeys) {
    if (Array.isArray(effectivePayload[key])) {
      const arr = effectivePayload[key] as any[];
      const names = arr.map(i => isRecord(i) ? (i.name || i.path || i.id || i.assetName || i.objectPath || i.packageName || i.nodeName || '<?>') : String(i));
      const count = arr.length;
      const preview = names.slice(0, 100).join(', '); // Show up to 100
      const suffix = count > 100 ? `, ... (+${count - 100} more)` : '';
      parts.push(`${key}: ${preview}${suffix} (Total: ${count})`);
    }
  }

  // 3. Identify "Entity" operations (Single significant object)
  if (typeof effectivePayload.actor === 'string' || isRecord(effectivePayload.actor)) {
    const a = effectivePayload.actor;
    const name = isRecord(a) ? (a.name || a.path) : a;
    const loc = isRecord(effectivePayload.location) ? ` at [${effectivePayload.location.x},${effectivePayload.location.y},${effectivePayload.location.z}]` : '';
    parts.push(`Actor: ${name}${loc}`);
  }

  if (typeof effectivePayload.asset === 'string' || isRecord(effectivePayload.asset)) {
    const a = effectivePayload.asset;
    const path = isRecord(a) ? (a.path || a.name) : a;
    parts.push(`Asset: ${path}`);
  }

  if (typeof effectivePayload.blueprint === 'string' || isRecord(effectivePayload.blueprint)) {
    const bp = effectivePayload.blueprint;
    const name = isRecord(bp) ? (bp.name || bp.path) : bp;
    parts.push(`Blueprint: ${name}`);
  }

  if (typeof effectivePayload.sequence === 'string' || isRecord(effectivePayload.sequence)) {
    const seq = effectivePayload.sequence;
    const name = isRecord(seq) ? (seq.name || seq.path) : seq;
    parts.push(`Sequence: ${name}`);
  }

  // 4. Generic Key-Value Summary (Contextual)
  // Added: sequencePath, graphName, nodeName, variableName, memberName, scriptName, etc.
  const usefulKeys = [
    'success', 'error', 'message', 'assets', 'folders', 'count', 'totalCount',
    'saved', 'valid', 'issues', 'class', 'skeleton', 'parent',
    'package', 'dependencies', 'graph', 'tags', 'metadata', 'properties'
  ];

  for (const key of usefulKeys) {
    if (effectivePayload[key] !== undefined && effectivePayload[key] !== null) {
      const val = effectivePayload[key];
      // Special handling for objects like metadata
      if (typeof val === 'object') {
        if (key === 'metadata' || key === 'properties' || key === 'tags') {
          const entries = Object.entries(val as object);
          // Format as "Key=Value", skip generic types if possible, or just show raw
          const formatted = entries.map(([k, v]) => `${k}=${v}`);
          const limit = 50; // Show more items as requested
          parts.push(`${key}: { ${formatted.slice(0, limit).join(', ')}${formatted.length > limit ? '...' : ''} }`);
          continue;
        }
        // Try to find a name if it's an object
        // Skip complex objects unless handled above
        continue;
      }

      const strVal = String(val);
      // Avoid traversing huge strings
      if (strVal.length > 100) continue;

      if (!parts.some(p => p.includes(strVal))) {
        parts.push(`${key}: ${strVal}`);
      }
    }
  }

  // 5. Add standard status messages LAST
  const success = typeof payload.success === 'boolean' ? payload.success : undefined;
  const message = typeof payload.message === 'string' ? normalizeText(payload.message) : '';
  const error = typeof payload.error === 'string' ? normalizeText(payload.error) : '';

  if (parts.length > 0) {
    if (message && message.toLowerCase() !== 'success') {
      parts.push(message);
    }
  } else {
    // No data parts, rely on message/error
    if (message) parts.push(message);
    if (error) parts.push(`Error: ${error}`);
    if (parts.length === 0 && success !== undefined) {
      parts.push(success ? 'Success' : 'Failed');
    }
  }

  // 6. Warnings
  const warnings = Array.isArray(payload.warnings) ? payload.warnings : [];
  if (warnings.length > 0) {
    parts.push(`Warnings: ${warnings.length}`);
  }

  return parts.length > 0 ? parts.join(' | ') : `${toolName} responded`;
}

/**
 * Response Validator for MCP Tool Outputs
 * Validates tool responses against their defined output schemas
 */
export class ResponseValidator {
  // Keep ajv as any to avoid complex interop typing issues with Ajv's ESM/CJS dual export
  // shape when using NodeNext module resolution.
  private ajv: any;
  private validators: Map<string, any> = new Map();

  constructor() {
    // Cast Ajv to any for construction to avoid errors when TypeScript's NodeNext
    // module resolution represents the import as a namespace object.
    const AjvCtor: any = (Ajv as any)?.default ?? Ajv;
    this.ajv = new AjvCtor({
      allErrors: true,
      verbose: true,
      strict: true // Enforce strict schema validation
    });
  }

  /**
   * Register a tool's output schema for validation
   */
  registerSchema(toolName: string, outputSchema: any) {
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
  async validateResponse(toolName: string, response: any): Promise<{
    valid: boolean;
    errors?: string[];
    structuredContent?: any;
  }> {
    const validator = this.validators.get(toolName);

    if (!validator) {
      log.debug(`No validator found for tool: ${toolName}`);
      return { valid: true }; // Pass through if no schema defined
    }

    // Extract structured content from response
    let structuredContent = response;

    // If response has MCP format with content array
    if (response.content && Array.isArray(response.content)) {
      // Try to extract structured data from text content
      const textContent = response.content.find((c: any) => c.type === 'text');
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
      const errors = validator.errors?.map((err: any) =>
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
  async wrapResponse(toolName: string, response: any): Promise<any> {
    // Ensure response is safe to serialize first
    try {
      if (response && typeof response === 'object') {
        JSON.stringify(response);
      }
    } catch (_error) {
      log.error(`Response for ${toolName} contains circular references, cleaning...`);
      response = cleanObject(response);
    }

    // If handler already returned MCP content, keep it as-is (still validate)
    const alreadyMcpShaped = response && typeof response === 'object' && Array.isArray(response.content);

    // Choose the payload to validate: if already MCP-shaped, validate the
    // structured content extracted from text; otherwise validate the object directly.
    const validation = await this.validateResponse(toolName, response);
    const structuredPayload = validation.structuredContent;

    if (!validation.valid) {
      log.warn(`Tool ${toolName} response validation failed:`, validation.errors);
    }

    // If it's already MCP-shaped, return as-is (optionally append validation meta)
    if (alreadyMcpShaped) {
      if (structuredPayload !== undefined && response && typeof response === 'object' && (response as any).structuredContent === undefined) {
        try {
          (response as any).structuredContent = structuredPayload && typeof structuredPayload === 'object'
            ? cleanObject(structuredPayload)
            : structuredPayload;
        } catch { }
      }
      // Promote failure semantics to top-level isError when obvious
      try {
        const sc: any = (response as any).structuredContent || structuredPayload || {};
        const hasExplicitFailure = (typeof sc.success === 'boolean' && sc.success === false) || (typeof sc.error === 'string' && sc.error.length > 0);
        if (hasExplicitFailure && (response as any).isError !== true) {
          (response as any).isError = true;
        }
      } catch { }
      if (!validation.valid) {
        try {
          (response as any)._validation = { valid: false, errors: validation.errors };
        } catch { }
      }
      return response;
    }

    // Otherwise, wrap structured result into MCP content
    const summarySource = structuredPayload !== undefined ? structuredPayload : response;
    let text = buildSummaryText(toolName, summarySource);
    if (!text || !text.trim()) {
      text = buildSummaryText(toolName, response);
    }

    const wrapped = {
      content: [
        { type: 'text', text }
      ]
    } as any;

    // Surface a top-level success flag when available so clients and test
    // harnesses do not have to infer success from the absence of isError.
    try {
      if (structuredPayload && typeof (structuredPayload as any).success === 'boolean') {
        (wrapped as any).success = Boolean((structuredPayload as any).success);
      } else if (response && typeof (response as any).success === 'boolean') {
        (wrapped as any).success = Boolean((response as any).success);
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
    } else if (response && typeof response === 'object') {
      try {
        wrapped.structuredContent = cleanObject(response);
      } catch {
        wrapped.structuredContent = response;
      }
    }

    // Promote failure semantics to top-level isError when obvious
    try {
      const sc: any = wrapped.structuredContent || {};
      const hasExplicitFailure = (typeof sc.success === 'boolean' && sc.success === false) || (typeof sc.error === 'string' && sc.error.length > 0);
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
      const s = (wrapped as any).success;
      if (typeof s === 'boolean' && s === false) {
        (wrapped as any).isError = true;
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
