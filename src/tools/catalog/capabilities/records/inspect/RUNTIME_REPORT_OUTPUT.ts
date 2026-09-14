/**
 * Output contract for the shared runtime-report handler
 * (McpAutomationBridge_EnvironmentHandlersInspectRuntime.cpp), read by both
 * runtime_report and pie_report.
 */
import type { JsonObject } from '../../index.js';

export const RUNTIME_REPORT_OUTPUT = {
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
