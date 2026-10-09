/**
 * Runtime inspection records (2 actions): runtime_report and pie_report.
 *
 * Both dispatch to the shared runtime-report handler. pie_report is aliased to
 * runtime_report in inspect-actions.ts for switch routing, but the handler
 * re-dispatches the original pie_report action, so the record keeps pie_report.
 */
import type { CapabilityRecordSource, JsonObject } from '../../model.js';
import { ANY_EDITOR_STATE, buildCoreRecord } from '../core/builder.js';
import { P } from './properties.js';

const D = 'inspect';

// Output of the shared runtime-report handler
// (McpAutomationBridge_EnvironmentHandlersInspectRuntime.cpp), read by both actions.
const RUNTIME_REPORT_OUTPUT = {
  // Four McpDescribeRuntimeActor() results: full describes rendered as object
  // paths, not nested objects. Declaring them as objects made every pie_report
  // fail output validation; BB-036 pins the plugin to emit object-path strings.
  worldName: { type: 'string', description: 'Name of the world the report describes.' },
  worldPath: { type: 'string', description: 'Package path of that world.' },
  worldType: { type: 'string', description: 'World type, e.g. PIE or Editor.' },
  isPIE: { type: 'boolean', description: 'Whether a PIE session is active.' },
  actors: { type: 'array', items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true }, description: 'Matching runtime actors and their inspected components/properties.' },
  count: { type: 'number', description: 'Number of actors returned after filtering.' },
  totalActorCount: { type: 'number', description: 'Total actors in the inspected world.' },
  playerController: { type: 'string', description: 'Object path of the active PlayerController (inspect_object it for details).' },
  pawn: { type: 'string', description: 'Object path of the possessed pawn; inspect it to find where the player actually is.' },
  viewTarget: { type: 'string', description: 'Object path of the current view target.' },
  playerCameraManager: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'PlayerCameraManager described as a runtime actor, plus cameraLocation and cameraRotation as {x,y,z} / {pitch,yaw,roll} objects.' },
} as const satisfies JsonObject;

export const RUNTIME_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'inspect', action: 'runtime_report', dispatchAction: 'runtime_report', domain: D, family: 'runtime',
    editorStates: ANY_EDITOR_STATE,
    summary: 'Return a runtime report for the current PIE/simulate session.',
    whenToUse: ['Runtime state of actors/components/properties must be inspected during PIE.', 'PIE-only runtime state must be inspected.'],
    whenNotToUse: ['The editor is not in PIE; the report will be empty.'],
    inputProps: {
      filter: P.filter, actorName: P.actorName, name: P.name,
      componentName: P.componentName, componentNames: P.componentNames,
      propertyName: P.propertyName, propertyPath: P.propertyPath, propertyNames: P.propertyNames,
    },
    required: [],
    effect: 'read',
    outputProps: { ...RUNTIME_REPORT_OUTPUT },
    outputRequired: [],
    exampleInput: { action: 'runtime_report', actorName: 'PlayerStart_1' },
    exampleOutput: { success: true, message: 'Runtime report', worldName: 'Demo', worldType: 'PIE', isPIE: true, count: 1, totalActorCount: 39 },
  }),
];
