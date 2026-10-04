import { Logger } from '../logging/logger.js';
import { buildImageContent, buildSummaryText, hasExplicitFailurePayload } from './response-content.js';
import { cleanObject } from '../serialization/safe-json.js';
import { isRecord } from '../validation/type-guards.js';
import { validateAgainstCapabilitySchema } from '../../server/gateway/gateway-schema-validate.js';
import { unrealGatewayToolDefinition } from '../../tools/catalog/unreal-gateway-definition.js';

const log = new Logger('ResponseValidator');

/**
 * Shape an `unreal` gateway result as an MCP tools/call response: one text
 * summary block, the object as structuredContent (checked against the gateway
 * output schema; a mismatch is logged and reported in `_validation`, not
 * thrown), an image block when the result carries one, and isError on failure.
 */
export function wrapGatewayResponse(response: unknown): Record<string, unknown> {
  const schema = unrealGatewayToolDefinition.outputSchema;
  const violation = schema === undefined ? undefined : validateAgainstCapabilitySchema(response, schema);
  const errors = violation === undefined ? undefined : [`${violation.pointer || 'root'}: ${violation.message}`];
  if (errors !== undefined) log.warn('Response validation failed for unreal:', errors);

  const wrapped: Record<string, unknown> = {
    content: [{ type: 'text', text: buildSummaryText('unreal', response) }]
  };
  // A top-level success flag, so clients need not infer it from a missing isError.
  if (isRecord(response) && typeof response.success === 'boolean') wrapped.success = response.success;
  const structured = typeof response === 'object' && response !== null ? cleanObject(response) : response;
  const imageContent = buildImageContent(structured);
  if (response !== undefined) wrapped.structuredContent = imageContent ? omitImagePayload(structured, 0) : structured;
  if (hasExplicitFailurePayload(wrapped.structuredContent) || wrapped.success === false) wrapped.isError = true;
  if (errors !== undefined) wrapped._validation = { valid: false, errors };

  if (imageContent) (wrapped.content as Array<Record<string, unknown>>).push(imageContent);
  return wrapped;
}

// Mirrors the native StripImagePayload: the image travels once, as image content.
const IMAGE_PLACEHOLDER = '<omitted; see image content>';
const MAX_IMAGE_DEPTH = 100;

function omitImagePayload(value: unknown, depth: number): unknown {
  if (depth > MAX_IMAGE_DEPTH) return value;
  if (Array.isArray(value)) return value.map((item) => omitImagePayload(item, depth + 1));
  if (!isRecord(value)) return value;
  const out: Record<string, unknown> = {};
  for (const [key, child] of Object.entries(value)) {
    out[key] = key === 'imageBase64' && typeof child === 'string' ? IMAGE_PLACEHOLDER : omitImagePayload(child, depth + 1);
  }
  return out;
}
