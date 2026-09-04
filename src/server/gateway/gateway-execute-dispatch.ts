// src/server/gateway/gateway-execute-dispatch.ts
// Final stages of the canonical execute pipeline: dispatch through the existing
// consolidated handler boundary, then hold the result to the capability's own
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
import { maybeElicitMissingArgs } from '../tool-registry-elicitation.js';
import { handleConsolidatedToolCall } from '../../tools/orchestration/consolidated-tool-handlers.js';
import { validateAgainstCapabilitySchema } from './gateway-execute-validate.js';
import type { ExecuteTarget } from './gateway-execute-resolve.js';
import { executeSuccessEnvelope, refuseWithTarget } from './gateway-execute-envelope.js';
import type { GatewayReceiptContext } from './gateway-receipt-context.js';

const MAX_EXECUTION_RESULT_CHARS = 100_000;

// A screenshot is one indivisible base64 image: it can neither page nor filter,
// so the flat cap refused a working capture with advice the caller cannot act on.
// The native transport already raises the budget for exactly these two
// capabilities (McpNativeGatewayExecuteReceiptBuild.cpp:53-56); without the
// mirror the identical call succeeds over /mcp and fails over stdio. The image
// stays separately bounded by the handler's own base64 ceiling, so this is the
// limit already enforced upstream rather than a general escape hatch.
const MAX_IMAGE_RESULT_CHARS = 6_000_000;
const IMAGE_PAYLOAD_CAPABILITIES: ReadonlySet<string> = new Set([
  'control_editor.screenshot',
  'control_editor.take_screenshot',
  'system_control.screenshot'
]);

export type GatewayContext = {
  tools: ITools;
  logger: Logger;
  elicitationTimeoutMs: number;
  ensureConnected: () => Promise<boolean>;
};

// The declared output contract describes the capability payload, not the
// transport envelope, so each declared field is read from the handler result
// root and then from its `data` payload before the schema rules are applied.
function projectCanonicalOutput(result: unknown, schema: Draft202012ObjectSchema): unknown {
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

function deprecationWarnings(record: CapabilityRecord): readonly string[] {
  return record.deprecation.status === 'deprecated'
    ? [`Capability '${record.id}' is deprecated: ${record.deprecation.guidance}`]
    : [];
}

export async function dispatchAndValidate(
  target: ExecuteTarget,
  params: Record<string, unknown>,
  options: Record<string, unknown> | undefined,
  context: GatewayContext,
  receiptContext: GatewayReceiptContext
): Promise<Record<string, unknown>> {
  const record = target.record;
  const action = target.legacy.action;

  const targetArgs = await maybeElicitMissingArgs(
    record.routing.parentTool,
    { ...params, action, subAction: action },
    context.tools.elicit,
    context.elicitationTimeoutMs,
    context.logger
  );
  const result = cleanObject(
    await handleConsolidatedToolCall(record.routing.parentTool, targetArgs, context.tools)
  );

  // Computed BEFORE the failure branch. The guard used to sit only on the
  // success path, so a `success:false` result echoed an unbounded `detail`
  // straight into the error envelope (measured: a 2.2 MB envelope) while the
  // same bytes with `success:true` were refused. Size is a transport concern and
  // does not care which way the handler reported.
  const serialized = JSON.stringify(result);
  const resultCharBudget = IMAGE_PAYLOAD_CAPABILITIES.has(record.id)
    ? MAX_IMAGE_RESULT_CHARS
    : MAX_EXECUTION_RESULT_CHARS;
  const oversized = serialized !== undefined && serialized.length > resultCharBudget;

  if (handlerReportedFailure(result)) {
    // The plugin owns the live-state comparison (it must happen on the game
    // thread), so a Task 42 precondition refusal reaches us as a handler
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
    return refuseWithTarget(target, {
      errorCode: staleState ? 'STALE_STATE' : 'UNREAL_EXECUTION_ERROR',
      message: failureMessage(result),
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
    return refuseWithTarget(target, {
      errorCode: 'RESULT_TOO_LARGE',
      message: 'Result exceeded the gateway safety limit. Retry with the action pagination or filtering parameters described by this capability.',
      resultChars: serialized?.length ?? 0
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
    warnings: deprecationWarnings(record)
  }, receiptContext);
}
