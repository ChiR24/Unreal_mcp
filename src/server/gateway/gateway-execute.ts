// src/server/gateway/gateway-execute.ts
// The gateway `execute` operation: canonical validation, one dispatch, one receipt.
//
// Task 24 extracted this seam unchanged; Task 26 replaced its manifest-driven
// tool-union checks with the generated per-action capability contracts.
//
// Stage order is normative and shared with the native `/mcp` surface (the
// executable specification is `tests/unit/task-27-suite/execute-reference.ts`):
//
//   resolve form + alias -> availability -> params envelope -> reserved and
//   gateway-control keys -> options -> declared defaults -> exact per-action
//   input schema -> connection -> dispatch -> output schema -> receipt
//
// Nothing reaches `handleConsolidatedToolCall` until every earlier stage
// passes, and a result that fails the declared output schema can never be
// returned as a success. `NOT_CONNECTED`, elicitation and `RESULT_TOO_LARGE`
// are TS-local stages the native surface does not share.

import type { Draft202012ObjectSchema } from '../../tools/catalog/capabilities/model.js';
import { isRecord } from '../../utils/validation/type-guards.js';
import { dynamicToolManager } from '../../tools/dynamic/dynamic-tool-manager.js';
import { getString } from './gateway-shared.js';
import { buildNextCall, closestMatches, MAX_SUGGESTIONS } from './gateway-guidance.js';
import { executeTargetIndex, resolveExecuteTarget, type ExecuteTarget } from './gateway-execute-resolve.js';
import {
  applyDeclaredDefaults,
  findControlKeyInParams,
  hasOwn,
  validateAgainstCapabilitySchema,
  validateExecutionOptions,
  VIOLATION_GATEWAY_CODES
} from './gateway-execute-validate.js';
import {
  executeErrorEnvelope,
  refuseWithTarget,
  type ResolvedFailure
} from './gateway-execute-envelope.js';
import { dispatchAndValidate, type GatewayContext } from './gateway-execute-dispatch.js';

export type { GatewayContext };

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

type StaticCheck =
  | { readonly failure: ResolvedFailure }
  | { readonly params: Record<string, unknown> };

function checkStaticRequest(target: ExecuteTarget, args: Record<string, unknown>): StaticCheck {
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
  const params = isRecord(args.params) ? args.params : {};

  if (hasOwn(params, 'action') || hasOwn(params, 'subAction')) {
    return refuse({
      errorCode: 'INVALID_PARAMS',
      message: 'params must not override action or subAction. Supply the selected action at the gateway level.'
    });
  }

  const control = findControlKeyInParams(params);
  if (control !== undefined) {
    return refuse({
      errorCode: 'UNSUPPORTED_OPTION',
      option: control,
      message: `Gateway control '${control}' must not appear in action params. Supply it in options.`
    });
  }

  const optionViolation = validateExecutionOptions(args.options);
  if (optionViolation !== undefined) {
    return refuse({
      errorCode: optionViolation.errorCode,
      message: optionViolation.message,
      ...(optionViolation.option === undefined
        ? {}
        : { option: optionViolation.option, field: optionViolation.option }),
      ...(optionViolation.pointer === undefined ? {} : { pointer: optionViolation.pointer })
    });
  }

  const withDefaults = applyDeclaredDefaults(params, record.schemas.input);
  const inputFailure = validateInput(target, withDefaults);
  return inputFailure === undefined ? { params: withDefaults } : { failure: inputFailure };
}


export async function executeGatewayCall(
  args: Record<string, unknown>,
  context: GatewayContext
): Promise<Record<string, unknown>> {
  const index = executeTargetIndex();
  const resolution = resolveExecuteTarget(
    {
      capability: getString(args, 'capability'),
      tool: getString(args, 'tool'),
      action: getString(args, 'action'),
      params: isRecord(args.params) ? args.params : {}
    },
    index
  );

  if (!resolution.ok) {
    const { capabilityId, ...failure } = resolution.failure;
    return executeErrorEnvelope({
      ...failure,
      record: capabilityId === undefined ? undefined : index.byId.get(capabilityId),
      requestedTool: getString(args, 'tool'),
      requestedAction: getString(args, 'action')
    });
  }

  const target = resolution.target;
  const checked = checkStaticRequest(target, args);
  if ('failure' in checked) return refuseWithTarget(target, checked.failure);

  const canRunWithoutConnection = target.record.id === 'system_control.get_project_settings';
  if (!canRunWithoutConnection && !await context.ensureConnected()) {
    return refuseWithTarget(target, {
      errorCode: 'NOT_CONNECTED',
      message: 'Unreal Engine is not connected.',
      nextCall: buildNextCall({ operation: 'search' })
    });
  }

  return await dispatchAndValidate(
    target,
    checked.params,
    isRecord(args.options) ? args.options : undefined,
    context
  );
}
