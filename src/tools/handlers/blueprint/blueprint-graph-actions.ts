import { promoteScalarResultFields } from '../foundation/dispatch/common-handlers.js';
import {
  blueprintTarget,
  cleanRecord,
  executeBlueprintRequest,
  optionalNumber,
  optionalString
} from './blueprint-action-context.js';
import type { BlueprintActionContext, BlueprintActionHandler } from './blueprint-action-context.js';

const NODE_ALIASES: Readonly<Record<string, string>> = {
  CallFunction: 'K2Node_CallFunction',
  VariableGet: 'K2Node_VariableGet',
  VariableSet: 'K2Node_VariableSet',
  If: 'K2Node_IfThenElse',
  Branch: 'K2Node_IfThenElse',
  Switch: 'K2Node_SwitchInteger',
  Select: 'K2Node_Select',
  Cast: 'K2Node_DynamicCast',
  CustomEvent: 'K2Node_CustomEvent',
  Event: 'K2Node_Event',
  MakeArray: 'K2Node_MakeArray',
  ForEach: 'K2Node_ForEachElementInEnum',
  Sequence: 'K2Node_ExecutionSequence',
  ExecutionSequence: 'K2Node_ExecutionSequence',
  // ForLoop / ForLoopWithBreak / WhileLoop / ForEachLoop are StandardMacros library
  // macros (no UK2Node_* class). Do NOT alias them here: the bare name must reach the
  // bridge so TryCreateMacroNode can spawn a K2Node_MacroInstance. Aliasing ForEachLoop
  // to K2Node_ForEachElementInEnum was wrong — that is the enum iterator, and the
  // ForLoop/WhileLoop aliases pointed at nonexistent classes. ('ForEach' above
  // stays — that one genuinely is the enum-ForEach node.)
  Gate: 'K2Node_Gate',
  DoOnce: 'K2Node_DoOnce',
  FlipFlop: 'K2Node_FlipFlop',
  MultiGate: 'K2Node_MultiGate',
  Delay: 'K2Node_CallFunction',
  PrintString: 'K2Node_CallFunction',
  SetTimer: 'K2Node_CallFunction'
};

export const blueprintGraphHandlers: Readonly<Record<string, BlueprintActionHandler>> = {
  add_node: async (context) => await handleAddNode(context)
};

async function handleAddNode(context: BlueprintActionContext): Promise<Record<string, unknown>> {
  validateCallFunctionNode(context);
  const resolvedNodeType = resolveNodeType(context.argsTyped.nodeType);
  validateEventNode(context, resolvedNodeType);

  const result = await executeBlueprintRequest(context, 'manage_blueprint', {
    subAction: 'create_node',
    assetPath: blueprintTarget(context),
    nodeType: resolvedNodeType,
    graphName: context.argsTyped.graphName,
    memberName: optionalString(context.argsRecord.functionName),
    variableName: context.argsTyped.variableName,
    nodeName: optionalString(context.argsRecord.nodeName),
    eventName: optionalString(context.argsRecord.eventName) || optionalString(context.argsRecord.customEventName),
    memberClass: optionalString(context.argsRecord.memberClass) || optionalString(context.argsRecord.nodeClass),
    actionPath: optionalString(context.argsRecord.actionPath),
    inputActionPath: optionalString(context.argsRecord.inputActionPath),
    inputActionAssetPath: optionalString(context.argsRecord.inputActionAssetPath),
    posX: optionalNumber(context.argsRecord.posX),
    posY: optionalNumber(context.argsRecord.posY),
    parameters: context.argsRecord.parameters,
    timeoutMs: optionalNumber(context.argsRecord.timeoutMs)
  });
  return cleanRecord(promoteScalarResultFields(result));
}

function validateCallFunctionNode(context: BlueprintActionContext): void {
  const nodeType = context.argsTyped.nodeType;
  if ((nodeType === 'CallFunction' || nodeType === 'K2Node_CallFunction') &&
      !optionalString(context.argsRecord.functionName) &&
      !context.argsTyped.memberName) {
    throw new Error('CallFunction node requires functionName parameter');
  }
}

function validateEventNode(context: BlueprintActionContext, resolvedNodeType: string): void {
  const isEvent = resolvedNodeType === 'K2Node_Event' || resolvedNodeType === 'K2Node_CustomEvent';
  if (!isEvent || optionalString(context.argsRecord.eventName) || optionalString(context.argsRecord.customEventName) || context.argsTyped.name) {
    return;
  }

  context.argsRecord.eventName = context.argsTyped.name;
  if (!optionalString(context.argsRecord.eventName)) {
    throw new Error(`${resolvedNodeType} requires eventName (or customEventName) parameter`);
  }
}

function resolveNodeType(nodeType: string | undefined): string {
  if (nodeType && NODE_ALIASES[nodeType]) {
    return NODE_ALIASES[nodeType];
  }
  return nodeType || 'K2Node_CallFunction';
}
