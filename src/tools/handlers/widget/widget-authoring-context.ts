import type { HandlerArgs } from '../../../types/handlers/handler-types.js';
import type { ITools } from '../../../types/tools/tool-interfaces.js';
import { cleanObject } from '../../../utils/serialization/safe-json.js';
import { executeAutomationRequest, getTimeoutMs, normalizePathFields, requireNonEmptyString } from '../foundation/dispatch/common-handlers.js';

export type WidgetAuthoringContext = {
  readonly argsRecord: Record<string, unknown>;
  readonly timeoutMs: number;
  readonly tools: ITools;
};

export function createWidgetAuthoringContext(args: HandlerArgs, tools: ITools): WidgetAuthoringContext {
  const rawArgs: Record<string, unknown> = args;
  return {
    argsRecord: normalizePathFields(rawArgs, ['widgetPath', 'folder', 'path']),
    timeoutMs: getTimeoutMs(),
    tools
  };
}

export function validateWidgetRequiredFields(
  argsRecord: Record<string, unknown>,
  fieldNames: readonly string[]
): void {
  for (const fieldName of fieldNames) {
    requireNonEmptyString(argsRecord[fieldName], fieldName, `Missing required parameter: ${fieldName}`);
  }
}

export async function sendWidgetAuthoringRequest(
  context: WidgetAuthoringContext,
  subAction: string
): Promise<Record<string, unknown>> {
  const payload: HandlerArgs = { ...context.argsRecord, subAction };
  if (subAction === 'set_position') {
    normalizeVector2Alias(payload, 'position', 'positionX', 'positionY');
  }
  if (subAction === 'set_size') {
    normalizeVector2Alias(payload, 'size', 'sizeX', 'sizeY');
  }

  const result = await executeAutomationRequest(
    context.tools,
    'manage_widget_authoring',
    payload,
    `Automation bridge not available for widget authoring action: ${subAction}`,
    { timeoutMs: context.timeoutMs }
  );
  const cleaned = cleanRecord(result);
  if (cleaned.slotName === undefined && typeof cleaned.widgetName === 'string' && cleaned.widgetName !== '') {
    cleaned.slotName = cleaned.widgetName;
  }
  return cleaned;
}

function normalizeVector2Alias(payload: Record<string, unknown>, objectKey: string, xKey: string, yKey: string): void {
  if (payload[objectKey] !== undefined) {
    return;
  }

  const x = finiteNumber(payload[xKey]);
  const y = finiteNumber(payload[yKey]);
  if (x === undefined && y === undefined) {
    return;
  }

  const value: Record<string, number> = {};
  if (x !== undefined) {
    value.x = x;
  }
  if (y !== undefined) {
    value.y = y;
  }
  payload[objectKey] = value;
  delete payload[xKey];
  delete payload[yKey];
}

function finiteNumber(value: unknown): number | undefined {
  return typeof value === 'number' && Number.isFinite(value) ? value : undefined;
}

function cleanRecord(value: unknown): Record<string, unknown> {
  const cleaned = cleanObject(value);
  if (cleaned !== null && typeof cleaned === 'object' && !Array.isArray(cleaned)) {
    const record: Record<string, unknown> = {};
    for (const [key, entry] of Object.entries(cleaned)) {
      record[key] = entry;
    }
    return record;
  }
  return {};
}
