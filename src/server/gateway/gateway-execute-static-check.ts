// Stages 2-3 of the canonical execute pipeline, split out of the gateway
// execute entry: the folded-selector pin, the caller's params envelope, the
// reserved/gateway-control keys, the execution options and the declared
// defaults, then the exact per-action input schema.
//
// A refusal verdict here is terminal; nothing reaches the connection gate, the
// policy seam, or `handleConsolidatedToolCall` until this whole stage passes.

import type { Draft202012ObjectSchema } from '../../tools/catalog/capabilities/model.js';
import { isRecord } from '../../utils/validation/type-guards.js';
import { dynamicToolManager } from '../../tools/dynamic/dynamic-tool-manager.js';
import { buildNextCall, closestMatches, MAX_SUGGESTIONS } from './gateway-guidance.js';
import { executeTargetIndex, type ExecuteTarget } from './gateway-execute-resolve.js';
import { applyFoldedPins } from './gateway-dispatch-by.js';
import {
  applyDeclaredDefaults,
  coerceVectorShapes,
  checkPreviewSupport,
  findControlKeyInParams,
  hasOwn,
  HONORED_EXECUTION_OPTION_KEYS,
  validateAgainstCapabilitySchema,
  validateExecutionOptions,
  VIOLATION_GATEWAY_CODES
} from './gateway-execute-validate.js';
import type { ResolvedFailure } from './gateway-execute-envelope.js';
import {
  ExpectedRevisionsSchema,
  type ExpectedRevisions
} from '../../tools/catalog/capabilities/semantic/execution-options.js';

function declaredParameterNames(schema: Draft202012ObjectSchema): string[] {
  return isRecord(schema.properties)
    ? Object.keys(schema.properties).filter((name) => name !== 'action').sort()
    : [];
}

function validateInput(target: ExecuteTarget, params: Record<string, unknown>): ResolvedFailure | undefined {
  const record = target.record;
  // Canonical per-action schemas name the action as the capability itself, so
  // the dispatch action is supplied for validation only where it is declared.
  const declaresAction = isRecord(record.schemas.input.properties)
    && 'action' in record.schemas.input.properties;
  const candidate = declaresAction
    ? { ...params, action: record.routing.dispatchAction }
    : params;

  const violation = validateAgainstCapabilitySchema(candidate, record.schemas.input);
  if (violation === undefined) return undefined;

  const declared = declaredParameterNames(record.schemas.input);
  const offending = violation.pointer.split('/').filter((part) => part.length > 0).pop() ?? '';
  const suggestions = closestMatches(offending, declared, MAX_SUGGESTIONS);

  return {
    errorCode: VIOLATION_GATEWAY_CODES[violation.reason],
    message: `${violation.message} for ${record.id}. Call describe before execution.`,
    pointer: violation.pointer,
    ...(violation.reason === 'range' ? { field: violation.pointer } : {}),
    suggestions,
    allowedParameters: declared,
    nextCall: buildNextCall({
      operation: 'describe',
      tool: record.routing.parentTool,
      action: target.legacy.action,
      param: suggestions[0]
    })
  };
}

// The caller's own request minus the control the gateway cannot honor IS the
// call that will run for real, so the refusal hands back something executable
// rather than a description of what to change.
function previewFreeNextCall(
  target: ExecuteTarget,
  params: Record<string, unknown>,
  rawOptions: unknown
): Record<string, unknown> {
  const remaining = isRecord(rawOptions)
    ? Object.entries(rawOptions).filter(([key]) => key !== 'preview')
    : [];
  return {
    ...buildNextCall({
      operation: 'execute',
      tool: target.record.routing.parentTool,
      action: target.legacy.action
    }),
    params,
    ...(remaining.length === 0 ? {} : { options: Object.fromEntries(remaining) })
  };
}

export type StaticCheck =
  | { readonly failure: ResolvedFailure }
  | {
    readonly params: Record<string, unknown>;
    readonly expectedRevisions?: ExpectedRevisions;
    readonly timeoutMs?: number;
  };

export function checkStaticRequest(target: ExecuteTarget, args: Record<string, unknown>): StaticCheck {
  const record = target.record;
  const refuse = (failure: ResolvedFailure): StaticCheck => ({ failure });

  if (!dynamicToolManager.isToolEnabled(record.routing.parentTool)) {
    return refuse({
      errorCode: 'TOOL_DISABLED',
      message: `Tool '${record.routing.parentTool}' is disabled or unavailable.`,
      suggestions: closestMatches(
        record.routing.parentTool,
        [...executeTargetIndex().parentTools],
        MAX_SUGGESTIONS
      ),
      nextCall: buildNextCall({ operation: 'configure', tool: record.routing.parentTool })
    });
  }

  if (args.params !== undefined && !isRecord(args.params)) {
    return refuse({
      errorCode: 'INVALID_PARAMS',
      message: 'params must be an object.',
      suggestions: declaredParameterNames(record.schemas.input).slice(0, MAX_SUGGESTIONS),
      nextCall: buildNextCall({
        operation: 'describe',
        tool: record.routing.parentTool,
        action: target.legacy.action
      })
    });
  }
  // params must not override action or subAction is matched by
  // request-side validation before any Unreal call, so describe examples
  // echoing the gateway action inside params only occur when a caller
  // copy-pastes an example verbatim into params; strip a matching value:
  // allow it to proceed to schema validation without altering its meaning,
  // and refuse conflicting values as a smuggling attempt.
  const rawParams = isRecord(args.params) ? args.params : {};
  const paramsAction = hasOwn(rawParams, 'action') ? rawParams.action : undefined;
  const scrubbed = typeof paramsAction === 'string' && paramsAction === target.legacy.action
    ? (({ action: _droppedAction, ...rest }: Record<string, unknown>) => rest)(rawParams)
    : rawParams;
  if (hasOwn(scrubbed, 'action') || hasOwn(scrubbed, 'subAction')) {
    return refuse({
      errorCode: 'INVALID_PARAMS',
      message: 'params must not override action or subAction. Supply the selected action at the gateway level.'
    });
  }

  const control = findControlKeyInParams(scrubbed);
  if (control !== undefined) {
    return refuse({
      errorCode: 'UNSUPPORTED_OPTION',
      option: control,
      message: `Gateway control '${control}' must not appear in action params. Supply it in options.`
    });
  }

  const optionViolation = validateExecutionOptions(args.options)
    ?? checkPreviewSupport(args.options, record.id);
  if (optionViolation !== undefined) {
    return refuse({
      errorCode: optionViolation.errorCode,
      message: optionViolation.message,
      ...(optionViolation.option === undefined
        ? {}
        : { option: optionViolation.option, field: optionViolation.option }),
      ...(optionViolation.pointer === undefined ? {} : { pointer: optionViolation.pointer }),
      ...(optionViolation.errorCode !== 'UNSUPPORTED_PREVIEW'
        ? {}
        : {
          suggestions: closestMatches('preview', [...HONORED_EXECUTION_OPTION_KEYS], MAX_SUGGESTIONS),
          nextCall: previewFreeNextCall(target, scrubbed, args.options)
        })
    });
  }

  const expectedRevisions = !isRecord(args.options) || args.options.expectedRevisions === undefined
    ? undefined
    : ExpectedRevisionsSchema.parse(args.options.expectedRevisions);

  // Already bounded to an integer in 1..MAX_TIMEOUT_MS by validateExecutionOptions.
  const timeoutMs = isRecord(args.options) && typeof args.options.timeoutMs === 'number'
    ? args.options.timeoutMs
    : undefined;

  // Vector parameters arrive as arrays or {x,y,z} objects depending on the caller; both shapes are
  // accepted by every handler, so convert to the declared one before validation (dogfood #226).
  // An old name folded into this record implies selector values; they are
  // pinned before validation so the folded contract accepts the old call. A
  // caller who names the old action AND sends a conflicting selector value is
  // contradictory, not legacy: the action wins on dispatch, so the mismatched
  // value would ride into the handler — refuse instead.
  const pinned = applyFoldedPins(target, scrubbed);
  if (pinned === undefined) {
    return refuse({
      errorCode: 'INVALID_PARAMETER_VALUE',
      message: 'The named action pins a selector value that conflicts with the one supplied. Call the primary action to choose it freely, or drop the selector parameter.',
      nextCall: buildNextCall({
        operation: 'describe',
        tool: record.routing.parentTool,
        action: target.legacy.action
      })
    });
  }
  const withDefaults = coerceVectorShapes(applyDeclaredDefaults(pinned, record.schemas.input),
    record.schemas.input);
  const inputFailure = validateInput(target, withDefaults);
  return inputFailure === undefined
    ? {
      params: withDefaults,
      ...(expectedRevisions === undefined ? {} : { expectedRevisions }),
      ...(timeoutMs === undefined ? {} : { timeoutMs })
    }
    : { failure: inputFailure };
}
