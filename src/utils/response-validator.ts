import Ajv from 'ajv';
import { Logger } from './logger.js';
import { cleanObject } from './safe-json.js';

const log = new Logger('ResponseValidator');

/**
 * Response Validator for MCP Tool Outputs
 * Validates tool responses against their defined output schemas
 */
export class ResponseValidator {
  private ajv: Ajv;
  private validators: Map<string, any> = new Map();

  constructor() {
    this.ajv = new Ajv({
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
    const validationTarget = alreadyMcpShaped ? response : response;
    const validation = this.validateResponse(toolName, validationTarget);

    if (!validation.valid) {
      log.warn(`Tool ${toolName} response validation failed:`, validation.errors);
    }

    // If it's already MCP-shaped, return as-is (optionally append validation meta)
    if (alreadyMcpShaped) {
      if (!validation.valid) {
        try {
          (response as any)._validation = { valid: false, errors: validation.errors };
        } catch {}
      }
      return response;
    }

    // Otherwise, wrap structured result into MCP content
    let text: string;
    try {
      // Pretty-print small objects for readability
      text = typeof response === 'string'
        ? response
        : JSON.stringify(response ?? { success: true }, null, 2);
    } catch (_e) {
      text = String(response);
    }

    const wrapped = {
      content: [
        { type: 'text', text }
      ]
    } as any;

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