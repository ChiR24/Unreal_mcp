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
      log.info(`Registered output schema for tool: ${toolName}`);
    } catch (error) {
      log.error(`Failed to compile output schema for ${toolName}:`, error);
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
   * Wrap a tool response with validation
   */
  wrapResponse(toolName: string, response: any): any {
    // Ensure response is safe to serialize first
    try {
      // The response should already be cleaned, but double-check
      if (response && typeof response === 'object') {
        // Make sure we can serialize it
        JSON.stringify(response);
      }
    } catch (error) {
      log.error(`Response for ${toolName} contains circular references, cleaning...`);
      response = cleanObject(response);
    }
    
    const validation = this.validateResponse(toolName, response);
    
    // Add validation metadata
    if (!validation.valid) {
      log.warn(`Tool ${toolName} response validation failed:`, validation.errors);
      
      // Add warning to response but don't fail
      if (response && typeof response === 'object') {
        response._validation = {
          valid: false,
          errors: validation.errors
        };
      }
    }

    // Don't add structuredContent to the response - it's for internal validation only
    // Adding it can cause circular references
    
    return response;
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