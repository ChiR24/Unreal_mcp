// src/server/gateway/gateway-execute-dispatch.ts
// Final stages of the canonical execute pipeline: send the capability to the
// plugin (the same request the native gateway builds), then hold the result to the capability's own
// declared output contract before any success envelope is built.
//
// A handler result that fails its declared output schema is returned as a typed
// OUTPUT_SCHEMA_VIOLATION with the raw payload preserved as structured detail,
// so a violation can never be dressed up as a success.

import type { ITools } from '../../types/tools/tool-interfaces.js';
import type { CapabilityRecord, Draft202012ObjectSchema } from '../../tools/catalog/capabilities/model.js';
import { cleanObject } from '../../utils/serialization/safe-json.js';
import { isRecord } from '../../utils/validation/type-guards.js';
import { Logger } from '../../utils/logging/logger.js';
import { executeAutomationRequest, type GatewayControls } from '../../tools/handlers/foundation/dispatch/automation-request-dispatch.js';
import { handleManageToolsCall } from '../tool-registry-manage-tools.js';
import { McpRequestCancelledError } from '../../automation/request-cancellation-error.js';
import { normalizeAutomationFrame } from '../../utils/responses/automation-frame-normalization.js';
import { validateAgainstCapabilitySchema } from './gateway-schema-validate.js';
import type { ExecuteTarget } from './gateway-execute-resolve.js';
import { resolveDispatchAction } from './gateway-dispatch-by.js';
import { buildNextCall } from './gateway-guidance.js';
import { executeSuccessEnvelope, refuseWithTarget } from './gateway-execute-envelope.js';
import type { GatewayReceiptContext } from './gateway-receipt-context.js';

export const MAX_EXECUTION_RESULT_CHARS = 100_000;

// An image is one indivisible base64 string: it can neither page nor filter, so
// the flat cap refused a working capture (a screenshot, a widget preview) with
// advice the caller cannot act on. The budget follows the reply's shape, not the
// capability that sent it: a top-level `imageBase64` string of at most
// MAX_IMAGE_BASE64_CHARS adds its own length to the flat cap. The rest of the
// reply is still held to 100k, and an image past its own ceiling is refused.
// Mirrors McpBuildGatewayExecuteReceipt on the native door, which promotes the
// same field to MCP image content.
export const MAX_IMAGE_BASE64_CHARS = 6_000_000;

export function resultCharBudget(result: unknown): number {
  const image = isRecord(result) ? result.imageBase64 : undefined;
  return typeof image === 'string' && image.length <= MAX_IMAGE_BASE64_CHARS
    ? MAX_EXECUTION_RESULT_CHARS + image.length
    : MAX_EXECUTION_RESULT_CHARS;
}

export type GatewayContext = {
  tools: ITools;
  logger: Logger;
  ensureConnected: () => Promise<boolean>;
};

// The declared output contract describes the capability payload, not the
// transport envelope, so each declared field is read from the handler result
// root and then from its `data` payload before the schema rules are applied.
export function projectCanonicalOutput(result: unknown, schema: Draft202012ObjectSchema): unknown {
  if (!isRecord(result)) return result;
  if (!isRecord(schema.properties)) return {};

  const payload = isRecord(result.data) ? result.data : undefined;
  const projected: Record<string, unknown> = {};
  for (const name of Object.keys(schema.properties)) {
    if (name in result) projected[name] = result[name];
    else if (payload !== undefined && name in payload) projected[name] = payload[name];
  }
  // Handlers publish rich payloads at the top level while many records declare
  // only {success, details}. Fold every undeclared handler field into the
  // declared `details` reflection-boundary object so the data survives a closed
  // contract. Mirrors McpProjectCanonicalOutput on the native transport.
  if ('details' in schema.properties) {
    const existing = isRecord(projected.details) ? projected.details : undefined;
    const details: Record<string, unknown> = existing ? { ...existing } : {};
    const fold = (source: Record<string, unknown>): void => {
      for (const [name, value] of Object.entries(source)) {
        if (name === 'data' || name === 'requestId' || name === 'type' || name in schema.properties) continue;
        if (!(name in details)) details[name] = value;
      }
    };
    fold(result);
    if (payload !== undefined) fold(payload);
    if (Object.keys(details).length > 0) projected.details = details;
  }
  return projected;
}

function handlerReportedFailure(result: unknown): boolean {
  return isRecord(result) && (result.success === false || result.isError === true);
}

function failureMessage(result: unknown): string {
  return isRecord(result) && typeof result.message === 'string'
    ? result.message
    : 'Unreal reported a failed execution.';
}

function failureString(result: unknown, key: string): string | undefined {
  if (!isRecord(result)) return undefined;
  const value = result[key];
  if (typeof value === 'string') return value;
  return typeof value === 'number' ? String(value) : undefined;
}

// A world edit made while Play-In-Editor runs lands in the PIE world and is
// discarded when play stops; the only trace was a UEDPIE_ prefix buried in
// details.worldName, so a caller editing "the level" while someone was playing
// believed a whole spawn batch had stuck. Mirrors McpAddPieWorldWarning (native).
const WORLD_EDIT_CAPABILITY_PREFIXES = ['control_actor.', 'build_environment.'];

function pieWorldWarnings(record: CapabilityRecord, result: unknown): readonly string[] {
  if (record.behavior.effect === 'read'
    || !WORLD_EDIT_CAPABILITY_PREFIXES.some((prefix) => record.id.startsWith(prefix))) {
    return [];
  }
  const details = isRecord(result) ? result.details : undefined;
  const world = failureString(result, 'worldName') ?? failureString(details, 'worldName');
  return world?.includes('/UEDPIE_') === true
    ? [`Applied to the running Play-In-Editor world (${world}): the change is discarded when play stops and the editor level is unchanged. Stop PIE first to edit the level.`]
    : [];
}

const HANDLER_CODE = /^[A-Z][A-Z0-9_]{0,63}$/;

function readHandlerCode(result: unknown): string | undefined {
  for (const field of ['errorCode', 'error']) {
    const value = failureString(result, field);
    if (value !== undefined && HANDLER_CODE.test(value)) return value;
  }
  return undefined;
}

// The narrowing parameters this capability itself declares, so a
// RESULT_TOO_LARGE refusal names filters the call actually has instead of
// promising paging on a capability that declares none.
const NARROWING_PARAM = /summary|filter|name|path|kind|type|limit|offset|page|cursor|count|max|top|depth/i;

function narrowingGuidance(
  record: CapabilityRecord,
  target: ExecuteTarget
): { readonly suggestions?: readonly string[]; readonly nextCall?: Record<string, unknown> } {
  const properties = record.schemas.input.properties;
  const filters = properties === undefined
    ? []
    : Object.keys(properties).filter((key) => NARROWING_PARAM.test(key)).slice(0, 6);
  return {
    ...(filters.length === 0 ? {} : { suggestions: filters.map((name) => `narrow with '${name}'`) }),
    nextCall: buildNextCall({
      operation: 'describe',
      tool: record.routing.parentTool,
      action: target.legacy.action
    })
  };
}

// ACTOR_NOT_FOUND from any handler said only "Actor not found" ("Bug1" beside Bug_01..Bug_10).
// A find by the same name answers with the labels it resembles (similar), so the refusal hands
// that call over. Mirrors the native completion in McpNativeTransportPendingRequests.cpp.
export function actorNotFoundGuidance(
  handlerCode: string | undefined,
  params: Record<string, unknown>
): { readonly suggestions?: readonly string[]; readonly nextCall?: Record<string, unknown> } {
  const name = params.actorName;
  if (handlerCode !== 'ACTOR_NOT_FOUND' || typeof name !== 'string' || name === '') return {};
  return {
    suggestions: [`No actor in the world is labeled or named '${name}'; control_actor find by name lists near labels under similar.`],
    nextCall: { operation: 'execute', tool: 'control_actor', action: 'find', params: { findBy: 'name', name } }
  };
}

// A handler that refuses a call because another call settles it can name that call in its own reply:
// the Fab add turns a second import away while one runs and hands back the read that reports the first.
// Only an executable gateway call is passed on, so a malformed value never reaches a caller as guidance.
// Mirrors AddHandlerNextCall in McpNativeTransportPendingRequests.cpp.
export function handlerNextCall(result: unknown): { readonly nextCall?: Record<string, unknown> } {
  if (!isRecord(result)) return {};
  for (const source of [result, result.data, result.result]) {
    const next = isRecord(source) ? source.nextCall : undefined;
    if (isRecord(next) && next.operation === 'execute' && typeof next.tool === 'string'
      && typeof next.action === 'string' && isRecord(next.params)) {
      return { nextCall: { operation: 'execute', tool: next.tool, action: next.action, params: next.params } };
    }
  }
  return {};
}

// Every capability runs the way the native gateway runs it: the record's parent
// tool receives {action, ...params} and the plugin's parent routing picks the
// domain handler. Only manage_tools records are served in process.
async function runCapability(
  record: CapabilityRecord,
  action: string,
  params: Record<string, unknown>,
  tools: ITools,
  controls: GatewayControls
): Promise<unknown> {
  if (record.routing.parentTool === 'manage_tools') return handleManageToolsCall({ ...params, action });
  const tool = record.routing.parentTool;
  try {
    return normalizeAutomationFrame(await executeAutomationRequest(tools, tool, { ...params, action }, controls));
  } catch (err: unknown) {
    const message = err instanceof Error ? err.message : String(err);
    // A client cancel is not a tool failure; it read as TOOL_EXECUTION_FAILED (or TOOL_TIMEOUT
    // when the message happened to contain "timeout").
    const code = err instanceof McpRequestCancelledError ? err.code
      : /timeout/i.test(message) ? 'TOOL_TIMEOUT' : /security violation/i.test(message) ? 'SECURITY_VIOLATION' : 'TOOL_EXECUTION_FAILED';
    return cleanObject({ success: false, isError: true, error: code, message: `Failed to execute ${tool}: ${message}`, toolName: tool, action });
  }
}

export async function dispatchAndValidate(
  target: ExecuteTarget,
  params: Record<string, unknown>,
  options: Record<string, unknown> | undefined,
  context: GatewayContext,
  receiptContext: GatewayReceiptContext,
  controls: GatewayControls,
  unread: readonly string[] = []
): Promise<Record<string, unknown>> {
  const record = target.record;
  // A folded family dispatches the action the caller named, or maps the
  // primary operation's selector to one; either way the handlers see an
  // action they already implement. An unmapped selector value fails closed —
  // a record whose validation passed always maps, so this only guards a
  // malformed record from silently dispatching the primary.
  const action = resolveDispatchAction(target, params);
  if (action === undefined) {
    return refuseWithTarget(target, {
      errorCode: 'INVALID_PARAMETER_VALUE',
      message: 'The selector value does not map to an action on this capability.',
      nextCall: buildNextCall({
        operation: 'describe',
        tool: record.routing.parentTool,
        action: target.legacy.action
      })
    }, receiptContext);
  }

  const result = cleanObject(
    await runCapability(record, action, params, context.tools, controls)
  );

  // Computed BEFORE the failure branch. The guard used to sit only on the
  // success path, so a `success:false` result echoed an unbounded `detail`
  // straight into the error envelope (measured: a 2.2 MB envelope) while the
  // same bytes with `success:true` were refused. Size is a transport concern and
  // does not care which way the handler reported.
  const serialized = JSON.stringify(result);
  const oversized = serialized !== undefined && serialized.length > resultCharBudget(result);

  if (handlerReportedFailure(result)) {
    // The plugin owns the live-state comparison (it must happen on the game
    // thread), so a precondition refusal reaches us as a handler
    // failure. Flattening it here would make the SAME refusal an untyped
    // execution error over stdio/WebSocket while the native transport reports a
    // typed staleState, so the code and any current/expected references are
    // carried through unchanged.
    const currentRevision = failureString(result, 'currentRevision');
    const expectedRevision = failureString(result, 'expectedRevision');
    // The bridge and the native surface disagree about WHERE the code lives: an
    // automation_response carries it as `error`, the native receipt as
    // `errorCode`. Reading only one of them is what let the identical refusal
    // arrive typed on native and flattened over stdio. Exact equality against
    // the sentinel, so free text that merely mentions staleness cannot promote
    // an unrelated failure.
    const staleState = failureString(result, 'errorCode') === 'STALE_STATE'
      || failureString(result, 'error') === 'STALE_STATE';
    const handlerCode = staleState ? undefined : readHandlerCode(result);
    return refuseWithTarget(target, {
      errorCode: staleState ? 'STALE_STATE' : handlerCode ?? 'UNREAL_EXECUTION_ERROR',
      message: failureMessage(result),
      ...(handlerCode === undefined ? {} : { handlerCode }),
      ...actorNotFoundGuidance(handlerCode, params),
      ...handlerNextCall(result),
      ...(staleState && currentRevision !== undefined ? { currentRevision } : {}),
      ...(staleState && expectedRevision !== undefined ? { expectedRevision } : {}),
      // The code and message stay - they are small and are the actionable part.
      // Only the unbounded payload is withheld, with the size named so the
      // caller can tell "no detail" from "detail dropped".
      ...(oversized
        ? { detailOmitted: true, resultChars: serialized?.length ?? 0 }
        : { detail: result })
    }, receiptContext);
  }

  if (oversized) {
    const resultChars = serialized?.length ?? 0;
    return refuseWithTarget(target, {
      errorCode: 'RESULT_TOO_LARGE',
      message: `Result exceeded the gateway safety limit (${resultChars} chars). Narrow the request with one of this capability's own filter parameters, then retry.`,
      resultChars,
      ...narrowingGuidance(record, target)
    }, receiptContext);
  }

  const canonicalOutput = projectCanonicalOutput(result, record.schemas.output);
  const violation = validateAgainstCapabilitySchema(canonicalOutput, record.schemas.output);
  if (violation !== undefined) {
    return refuseWithTarget(target, {
      errorCode: 'OUTPUT_SCHEMA_VIOLATION',
      message: `${record.id} returned a result that violates its declared output contract: ${violation.message}`,
      pointer: violation.pointer,
      detail: result
    }, receiptContext);
  }

  return executeSuccessEnvelope({
    record,
    result,
    canonicalOutput,
    resolvedFromAlias: target.resolvedFromAlias,
    migratedFrom: target.migratedFrom,
    options,
    warnings: [...pieWorldWarnings(record, result), ...unread]
  }, receiptContext);
}
