import Ajv, { ValidateFunction } from 'ajv';
import { Logger } from '../logging/logger.js';
import { buildImageContent, buildSummaryText, hasExplicitFailurePayload } from './response-content.js';
import { cleanObject } from '../serialization/safe-json.js';
import { isRecord } from '../validation/type-guards.js';
const log = new Logger('ResponseValidator');

type AjvModuleWithDefault = typeof Ajv & { default?: typeof Ajv.default };

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
    const AjvClass = (Ajv as AjvModuleWithDefault).default ?? Ajv.default;
    this.ajv = new AjvClass({
      allErrors: true,
      verbose: true,
      strict: true, // Enforce strict schema validation
      allowUnionTypes: true // Accept intentional multiple-type unions (e.g. frameRate: ['number','string'])
    });

    // Register the UE reflection-boundary annotation as a boolean, annotation-only keyword (valid:true) so strict compilation accepts it without weakening strict mode.
    this.ajv.addKeyword({
      keyword: 'x-unreal-reflection-boundary',
      schemaType: 'boolean',
      valid: true
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
    } catch (error) {
      log.error(`Failed to compile output schema for ${toolName}:`, error instanceof Error ? error : String(error));
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

    let structuredContent = response;
    const responseObj = isRecord(response) ? response : null;

    // If response has MCP format with content array
    if (responseObj && responseObj.content && Array.isArray(responseObj.content)) {
      const textContent = responseObj.content.find((c: unknown): c is Record<string, unknown> =>
        isRecord(c) && c.type === 'text'
      );
      if (textContent?.text) {
        const rawText = String(textContent.text);
        const trimmed = rawText.trim();
        const looksLikeJson = trimmed.startsWith('{') || trimmed.startsWith('[');

        if (looksLikeJson) {
          try {
            structuredContent = JSON.parse(rawText);
          } catch {
            // If JSON parsing fails, fall back to using the full response
            structuredContent = response;
          }
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
    let safeResponse = response;
    try {
      if (response && typeof response === 'object') {
        JSON.stringify(response);
      }
    } catch (error) {
      log.error(`Response for ${toolName} contains circular references, cleaning...`, error instanceof Error ? error : String(error));
      safeResponse = cleanObject(response);
    }

    const responseObj = isRecord(safeResponse) ? safeResponse : null;

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
        } catch (error) {
          log.debug(`Unable to attach structured content for ${toolName}`, error instanceof Error ? error : String(error));
        }
      }
      // Promote failure semantics to top-level isError when obvious
      const structuredContent = responseObj.structuredContent ?? structuredPayload;
      if ((hasExplicitFailurePayload(structuredContent) || hasExplicitFailurePayload(responseObj)) && responseObj.isError !== true) {
        responseObj.isError = true;
      }
      if (!validation.valid) {
        try {
          responseObj._validation = { valid: false, errors: validation.errors };
        } catch (error) {
          log.debug(`Unable to attach validation metadata for ${toolName}`, error instanceof Error ? error : String(error));
        }
      }
      const imageContent = buildImageContent(responseObj.structuredContent ?? structuredPayload);
      if (imageContent && Array.isArray(responseObj.content)) {
        const hasImageContent = responseObj.content.some((part: unknown) => isRecord(part) && part.type === 'image');
        if (!hasImageContent) {
          responseObj.content.push(imageContent);
        }
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
    if (isRecord(structuredPayload) && typeof structuredPayload.success === 'boolean') {
      wrapped.success = Boolean(structuredPayload.success);
    } else if (isRecord(safeResponse) && typeof safeResponse.success === 'boolean') {
      wrapped.success = Boolean(safeResponse.success);
    }

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
    if (hasExplicitFailurePayload(wrapped.structuredContent)) {
      wrapped.isError = true;
    }

    if (!validation.valid) {
      wrapped._validation = { valid: false, errors: validation.errors };
    }

    const imageContent = buildImageContent(wrapped.structuredContent ?? structuredPayload ?? safeResponse);
    if (imageContent) {
      (wrapped.content as Array<Record<string, unknown>>).push(imageContent);
    }

    // Mark explicit error when success is false to avoid false positives in
    // clients that check only for the absence of isError.
    const success = wrapped.success;
    if (typeof success === 'boolean' && success === false) {
      wrapped.isError = true;
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
