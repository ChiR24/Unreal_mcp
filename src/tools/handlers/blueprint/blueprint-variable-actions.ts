import {
  blueprintCandidates,
  blueprintTarget,
  commonTimingPayload,
  executeBlueprintRequest,
  optionalNumber,
  optionalObject,
  optionalString
} from './blueprint-action-context.js';
import type { BlueprintActionHandler } from './blueprint-action-context.js';

export const blueprintVariableHandlers: Readonly<Record<string, BlueprintActionHandler>> = {
  add_variable: async (context) => await executeBlueprintRequest(context, 'blueprint_add_variable', {
    blueprintCandidates: blueprintCandidates(context),
    requestedPath: blueprintTarget(context),
    variableName: context.argsTyped.variableName ?? '',
    variableType: optionalString(context.argsRecord.variableType) ?? 'Boolean',
    defaultValue: context.argsRecord.defaultValue,
    category: optionalString(context.argsRecord.category),
    isReplicated: context.argsRecord.isReplicated,
    isPublic: context.argsRecord.isPublic,
    variablePinType: optionalObject(context.argsRecord.variablePinType),
    ...commonTimingPayload(context)
  }),
  set_variable_metadata: async (context) => await executeBlueprintRequest(context, 'blueprint_set_variable_metadata', {
    blueprintCandidates: blueprintCandidates(context),
    requestedPath: blueprintTarget(context),
    variableName: context.argsTyped.variableName ?? '',
    variableNames: Array.isArray(context.argsRecord.variableNames) ? context.argsRecord.variableNames : undefined,
    metadata: context.argsTyped.metadata ?? {},
    timeoutMs: optionalNumber(context.argsRecord.timeoutMs)
  }),
  remove_variable: async (context) => await executeBlueprintRequest(context, 'blueprint_remove_variable', {
    blueprintCandidates: blueprintCandidates(context),
    requestedPath: blueprintTarget(context),
    variableName: context.argsTyped.variableName ?? '',
    ...commonTimingPayload(context)
  }),
  rename_variable: async (context) => await executeBlueprintRequest(context, 'blueprint_rename_variable', {
    blueprintCandidates: blueprintCandidates(context),
    requestedPath: blueprintTarget(context),
    oldName: optionalString(context.argsRecord.oldName) ?? '',
    newName: optionalString(context.argsRecord.newName) ?? '',
    ...commonTimingPayload(context)
  })
};
