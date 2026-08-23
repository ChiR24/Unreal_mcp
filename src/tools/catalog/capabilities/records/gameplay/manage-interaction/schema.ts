/**
 * Exact per-action schema vocabulary for manage_interaction.
 *
 * Every parameter name below is read by an editor-authoring handler under
 * plugins/McpAutomationBridge/.../Private/Domains/Interaction/. The
 * world-actor variants in *RuntimeActors.cpp / *RuntimeComponents.cpp
 * (doorName, switchName, chestName, isLocked, requiredKey, maxItems, location,
 * interactionDistance, traceChannel, widgetText, offsetZ, ...) are deliberately
 * NOT declared: McpAutomationBridge_InteractionHandlers.cpp consults the
 * authoring handlers first and each of them returns true for its sub-action, so
 * those payload fields are unreachable through manage_interaction and declaring
 * them would advertise a contract the tool cannot honour.
 *
 * `path` and `properties` are likewise absent — this parent folders assets with
 * `folder` and exposes no reflection property bag.
 *
 * A parameter name maps to exactly ONE fragment object and every action reuses
 * it, so the record-only parent derivation collapses the union to a single
 * shape instead of emitting a `oneOf`
 * (see scripts/canonical-registry/schema-merge.ts).
 */
import type { CapabilityRecordSource, JsonObject } from '../../../index.js';
import { buildRecord } from '../helpers.js';
import { P } from '../properties.js';
import type { PropertyMap } from '../properties.js';

const str = (desc: string): JsonObject => ({ type: 'string', description: desc });
const num = (desc: string): JsonObject => ({ type: 'number', description: desc });
const bool = (desc: string): JsonObject => ({ type: 'boolean', description: desc });

/** Interaction parameter vocabulary, keyed by the exact payload field name. */
export const NP: PropertyMap = {
  name: P.name,
  blueprintPath: P.blueprintPath,
  actorName: P.actorName,
  lootTablePath: P.lootTablePath,
  folder: str('Canonical /Game folder that receives the created asset.'),
  componentName: str('Name for the component added to the Blueprint.'),
  traceDistance: num('Interaction trace distance in world units.'),
  traceType: str('Interaction trace shape (line, sphere, or box).'),
  traceRadius: num('Interaction trace radius in world units.'),
  widgetClass: str('Canonical /Game interaction widget class path.'),
  showOnHover: bool('Whether the interaction widget appears on hover.'),
  showPromptText: bool('Whether the interaction widget shows prompt text.'),
  promptTextFormat: str('Interaction prompt format string, e.g. "Press {Key} to Interact".'),
  openAngle: num('Open angle in degrees.'),
  openTime: num('Open animation duration in seconds.'),
  autoClose: bool('Whether the door closes automatically.'),
  autoCloseDelay: num('Delay in seconds before the door auto-closes.'),
  requiresKey: bool('Whether opening requires a key.'),
  doorPath: str('Canonical /Game door actor Blueprint asset path.'),
  locked: bool('Whether the interactable starts locked.'),
  switchType: str('Switch type (button, lever, or pressure_plate).'),
  switchPath: str('Canonical /Game switch actor Blueprint asset path.'),
  canToggle: bool('Whether the switch can toggle back off.'),
  resetTime: num('Switch reset delay in seconds.'),
  chestPath: str('Canonical /Game chest actor Blueprint asset path.'),
  triggerShape: str('Trigger volume shape (box, sphere, or capsule).'),
  triggerPath: str('Canonical /Game trigger actor Blueprint asset path.'),
};

export interface InteractionActionSpec {
  readonly action: string;
  readonly summary: string;
  readonly inputProps: PropertyMap;
  readonly required?: readonly string[];
  readonly exampleInput: JsonObject;
  /** Only get_interaction_info reads without writing. */
  readonly read?: boolean;
  /**
   * Extra declared output fields, MERGED over the shared assetPath handle.
   * Merged rather than replaced so a record cannot accidentally drop the
   * identity handle records.test.ts requires of every capability.
   */
  readonly outputProps?: PropertyMap;
}

/**
 * Build one manage_interaction record. Non-input facets (output handle, editor
 * state, cost, effect) are held identical to the previous compact records so
 * the 22 records and their route metadata stay intact; only the input contract
 * becomes exact.
 */
export function interactionRecord(spec: InteractionActionSpec): CapabilityRecordSource {
  return buildRecord({
    parentTool: 'manage_interaction',
    id: `manage_interaction.${spec.action}`,
    action: spec.action,
    family: 'interaction',
    summary: spec.summary,
    whenToUse: [`Use the leaf-backed ${spec.action} capability.`],
    whenNotToUse: ['Do not substitute a similarly named action with different semantics.'],
    inputProps: { action: P.action, ...spec.inputProps },
    required: ['action', ...(spec.required ?? [])],
    outputProps: { assetPath: P.assetPath, ...(spec.outputProps ?? {}) },
    outputRequired: [],
    effect: spec.read === true ? 'read' : 'write',
    latency: 'interactive',
    resources: 'medium',
    editorStates: ['edit'],
    exampleInput: spec.exampleInput,
    exampleOutput: { success: true, message: `${spec.action} handled` },
  });
}
