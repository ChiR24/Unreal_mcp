import Ajv from 'ajv';
import { Logger } from './logger.js';
import { cleanObject } from './safe-json.js';

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

  const parts: string[] = [];
  const message = typeof payload.message === 'string' ? normalizeText(payload.message) : '';
  const error = typeof payload.error === 'string' ? normalizeText(payload.error) : '';
  const success = typeof payload.success === 'boolean' ? (payload.success ? 'success' : 'failed') : '';
  const path = typeof payload.path === 'string' ? payload.path : '';
  const name = typeof payload.name === 'string' ? payload.name : '';
  const warningCount = Array.isArray(payload.warnings) ? payload.warnings.length : 0;

  if (message) parts.push(message);
  if (error && (!message || !message.includes(error))) parts.push(`error: ${error}`);
  if (success) parts.push(success);
  if (path) parts.push(`path: ${path}`);
  if (name) parts.push(`name: ${name}`);
  if (warningCount > 0) parts.push(`warnings: ${warningCount}`);

  const summary = isRecord(payload.summary) ? payload.summary : undefined;
  if (summary) {
    const summaryParts: string[] = [];
    for (const [key, value] of Object.entries(summary)) {
      if (typeof value === 'number' || typeof value === 'string') {
        summaryParts.push(`${key}: ${value}`);
      }
      if (summaryParts.length >= 3) break;
    }
    if (summaryParts.length) {
      parts.push(`summary(${summaryParts.join(', ')})`);
    }
  }

  if (parts.length === 0) {
    const keys = Object.keys(payload).slice(0, 3);
    if (keys.length) {
      return `${toolName} responded (${keys.join(', ')})`;
    }
  }

  return parts.length > 0
    ? parts.join(' | ')
    : `${toolName} responded`;
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
      strict: false // Allow additional properties for flexibility
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
  validateResponse(toolName: string, response: any): { 
    valid: boolean; 
    errors?: string[];
    structuredContent?: any;
  } {
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
        try {
          // Check if text is JSON
          structuredContent = JSON.parse(textContent.text);
        } catch {
          // Not JSON, use the full response
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
  wrapResponse(toolName: string, response: any): any {
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
    const validation = this.validateResponse(toolName, response);
    const structuredPayload = validation.structuredContent;

    if (!validation.valid) {
      log.warn(`Tool ${toolName} response validation failed:`, validation.errors);
    }

    // If it's already MCP-shaped, return as-is (optionally append validation meta)
    if (alreadyMcpShaped) {
      if (structuredPayload !== undefined && response && typeof response === 'object' && response.structuredContent === undefined) {
        try {
          (response as any).structuredContent = structuredPayload && typeof structuredPayload === 'object'
            ? cleanObject(structuredPayload)
            : structuredPayload;
        } catch {}
      }
      if (!validation.valid) {
        try {
          (response as any)._validation = { valid: false, errors: validation.errors };
        } catch {}
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

    if (!validation.valid) {
      wrapped._validation = { valid: false, errors: validation.errors };
    }

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