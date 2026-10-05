import { Logger } from '../logging/logger.js';
import { buildImageContent, hasExplicitFailurePayload } from './response-content.js';
import { compactGatewayReply } from './gateway-reply-compaction.js';
import { cleanObject } from '../serialization/safe-json.js';
import { isRecord } from '../validation/type-guards.js';
import { validateAgainstCapabilitySchema } from '../../server/gateway/gateway-schema-validate.js';
import { unrealGatewayToolDefinition } from '../../tools/catalog/unreal-gateway-definition.js';

const log = new Logger('ResponseValidator');

/**
 * Shape an `unreal` gateway result as an MCP tools/call response: the object without its log-only fields
 * (gateway-reply-compaction.ts) as structuredContent (checked against the gateway output schema; a mismatch
 * is logged and reported in `_validation`, not thrown), the same object as text, an image block when the
 * result carries one, and isError on failure.
 */
export function wrapGatewayResponse(fullResponse: unknown): Record<string, unknown> {
  const response = compactGatewayReply(fullResponse);
  const schema = unrealGatewayToolDefinition.outputSchema;
  const violation = schema === undefined ? undefined : validateAgainstCapabilitySchema(response, schema);
  const errors = violation === undefined ? undefined : [`${violation.pointer || 'root'}: ${violation.message}`];
  if (errors !== undefined) log.warn('Response validation failed for unreal:', errors);

  const structured = typeof response === 'object' && response !== null ? cleanObject(response) : response;
  const imageContent = buildImageContent(structured);
  const shown = imageContent ? omitImagePayload(structured, 0) : structured;
  const failed = hasExplicitFailurePayload(shown) || (isRecord(response) && response.success === false);
  const wrapped: Record<string, unknown> = { content: [{ type: 'text', text: replyText(shown, failed) }] };
  // A top-level success flag, so clients need not infer it from a missing isError.
  if (isRecord(response) && typeof response.success === 'boolean') wrapped.success = response.success;
  if (response !== undefined) wrapped.structuredContent = shown;
  if (failed) wrapped.isError = true;
  if (errors !== undefined) wrapped._validation = { valid: false, errors };

  if (imageContent) (wrapped.content as Array<Record<string, unknown>>).push(imageContent);
  return wrapped;
}

// The text block the native transport sends (FMcpJsonRpc::BuildToolResult): the outcome on the first line, then
// the reply less what that line says. A client that shows the model only text (Claude Code does, for an error)
// keeps every field; a field-by-field summary used to cut nested values such as a nextCall's params to {...}.
function replyText(reply: unknown, failed: boolean): string {
  if (!isRecord(reply)) return typeof reply === 'string' && reply.trim() !== '' ? reply : 'unreal responded';
  const message = typeof reply.message === 'string' && reply.message !== '' ? reply.message : failed ? 'execute failed' : 'ok';
  const code = typeof reply.errorCode === 'string' && reply.errorCode !== '' ? reply.errorCode : undefined;
  const line = !failed ? message : code === undefined ? `Error: ${message}` : `Error [${code}]: ${message}`;
  const body = { ...reply };
  if (body.message === message) delete body.message;
  if (failed && code !== undefined) delete body.errorCode;
  return `${line}\n\n${JSON.stringify(body)}`;
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
