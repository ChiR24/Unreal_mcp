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

const MAX_EXECUTION_RESULT_CHARS = 100_000;

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

function deprecationWarnings(record: CapabilityRecord): readonly string[] {
  return record.deprecation.status === 'deprecated'
    ? [`Capability '${record.id}' is deprecated: ${record.deprecation.guidance}`]
    : [];
}

export async function dispatchAndValidate(
  target: ExecuteTarget,
  params: Record<string, unknown>,
  options: Record<string, unknown> | undefined,
  context: GatewayContext
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

  if (handlerReportedFailure(result)) {
    return refuseWithTarget(target, {
      errorCode: 'UNREAL_EXECUTION_ERROR',
      message: failureMessage(result),
      detail: result
    });
  }

  const serialized = JSON.stringify(result);
  if (serialized !== undefined && serialized.length > MAX_EXECUTION_RESULT_CHARS) {
    return refuseWithTarget(target, {
      errorCode: 'RESULT_TOO_LARGE',
      message: 'Result exceeded the gateway safety limit. Retry with the action pagination or filtering parameters described by this capability.',
      resultChars: serialized.length
    });
  }

  const canonicalOutput = projectCanonicalOutput(result, record.schemas.output);
  const violation = validateAgainstCapabilitySchema(canonicalOutput, record.schemas.output);
  if (violation !== undefined) {
    return refuseWithTarget(target, {
      errorCode: 'OUTPUT_SCHEMA_VIOLATION',
      message: `${record.id} returned a result that violates its declared output contract: ${violation.message}`,
      pointer: violation.pointer,
      detail: result
    });
  }

  return executeSuccessEnvelope({
    record,
    result,
    canonicalOutput,
    resolvedFromAlias: target.resolvedFromAlias,
    migratedFrom: target.migratedFrom,
    options,
    warnings: deprecationWarnings(record)
  });
}
