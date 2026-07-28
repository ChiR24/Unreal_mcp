/**
 * Asset and level navigation records: open_asset, close_asset, open_level,
 * focus_actor, save_all.
 *
 * Grounded in src/tools/handlers/editor/editor-asset-actions.ts and
 * editor-viewport-actions.ts (focus_actor). open_asset is read-effect
 * navigation; close_asset, open_level, save_all are write-effect. focus_actor
 * is read-effect viewport navigation. TS normalizes focus_actor to 'focus'
 * for handler routing but dispatches 'focus_actor' to the bridge.
 */
import type { CapabilityRecordSource, JsonObject } from '../../index.js';
import { buildCoreRecord } from '../core/builder.js';
import { P } from './properties.js';

const F = 'asset';
const D = 'editor';
const NR = 'Distinct control_editor asset or level navigation operation with unique target semantics.';

// The compensating-cleanup receipt save_all emits, declared so it survives the
// gateway's output narrowing. `projectCanonicalOutput` copies only DECLARED
// properties into the payload a client reads, so an undeclared block is dropped
// on the success path and reaches the caller only as un-narrowed error detail.
// Mirrors FMcpCompensationReceipt::DescribeInto() in
// plugins/.../Private/Foundation/McpCompensationReceipt.cpp exactly.
const COMPENSATION_STEP: JsonObject = {
  type: 'object',
  description: 'One step of the non-atomic save, naming what landed or why it did not.',
  properties: {
    step: { type: 'string', description: 'Machine-readable step id, e.g. save:/Game/Maps/Main.' },
    detail: { type: 'string', description: 'What landed on disk, or the reason nothing did.' },
  },
  required: ['step', 'detail'],
  additionalProperties: false,
};

const COMPENSATION: JsonObject = {
  type: 'object',
  description:
    'Compensating-cleanup receipt. Present on every outcome including the all-succeeded one, because '
    + 'each package lands independently and a completed save is already durable: non-atomic is a property '
    + 'of the operation, not of one result.',
  properties: {
    operation: { type: 'string', description: 'Canonical capability this receipt describes.' },
    atomic: {
      type: 'boolean',
      enum: [false],
      description: 'Always false. Packages land one at a time and the ones that landed stay landed.',
    },
    rollback: {
      type: 'string',
      enum: ['unavailable'],
      description: 'Always "unavailable". No editor transaction can reach a finished save, so this call was not and cannot be undone.',
    },
    rollbackReason: { type: 'string', description: 'Why no rollback exists for this class of work.' },
    state: {
      type: 'string',
      enum: ['completed', 'partial', 'failed', 'noop'],
      description: 'Outcome across all steps: everything landed, some did, none did, or there was nothing to do.',
    },
    completed: { type: 'array', items: COMPENSATION_STEP, description: 'Steps whose effect is now durable on disk.' },
    notCompleted: { type: 'array', items: COMPENSATION_STEP, description: 'Steps that did not complete, each with its reason.' },
    skipped: { type: 'array', items: COMPENSATION_STEP, description: 'Steps deliberately not attempted, such as transient packages.' },
    compensatingCapabilities: {
      type: 'array',
      items: { type: 'string', description: 'Canonical capability id that reverses a durable effect.' },
      description: 'Separate calls the caller may make to reverse a durable effect. Never a rollback of this call.',
    },
    callerAction: { type: 'string', description: 'Exact instruction for reaching a clean state; empty when nothing is outstanding.' },
  },
  required: ['atomic', 'rollback', 'state'],
  additionalProperties: false,
};

export const ASSET_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'control_editor', action: 'open_asset', domain: D, family: F,
    summary: 'Open an asset in the appropriate editor by asset path.',
    whenToUse: ['An asset must be opened for editing or inspection.'],
    whenNotToUse: ['The asset is already open.'],
    inputProps: { assetPath: P.assetPath, path: P.path },
    required: ['assetPath'],
    effect: 'read',
    costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'open_asset', assetPath: '/Game/Materials/M_Base' },
    exampleOutput: { success: true, message: 'Asset opened' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'close_asset', domain: D, family: F,
    summary: 'Close an open asset editor by asset path.',
    whenToUse: ['An open asset editor must be closed.'],
    whenNotToUse: ['The asset is not open.'],
    inputProps: { assetPath: P.assetPath, path: P.path },
    required: ['assetPath'],
    effect: 'write',
    costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'close_asset', assetPath: '/Game/Materials/M_Base' },
    exampleOutput: { success: true, message: 'Asset closed' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'open_level', domain: D, family: F,
    summary: 'Open a level by asset path, loading it as the current level.',
    whenToUse: ['A different level must be loaded into the editor.'],
    whenNotToUse: ['The level is already loaded.'],
    inputProps: { levelPath: P.levelPath, path: P.path, assetPath: P.assetPath },
    required: ['levelPath'],
    effect: 'write',
    costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'open_level', levelPath: '/Game/Maps/EntryMap' },
    exampleOutput: { success: true, message: 'Level loaded' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'focus_actor', domain: D, family: F,
    summary: 'Focus the viewport camera on a specific actor by name.',
    whenToUse: ['The viewport must frame a specific actor.'],
    whenNotToUse: ['The actor does not exist in the current level.'],
    inputProps: { actorName: P.actorName, name: P.name },
    required: ['actorName'],
    effect: 'read',
    costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'focus_actor', actorName: 'BP_PlayerStart' },
    exampleOutput: { success: true, message: 'Focused on BP_PlayerStart' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET',
    normalizationRationale: 'TS normalizes focus_actor to focus for handler routing; bridge dispatches focus_actor. Distinct viewport navigation verb.',
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'save_all', domain: D, family: F,
    summary: 'Save all dirty assets and levels in the editor.',
    whenToUse: ['All unsaved changes must be persisted.'],
    whenNotToUse: ['Specific assets should be saved individually.'],
    inputProps: {},
    required: [],
    outputProps: { compensation: COMPENSATION },
    effect: 'write',
    costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'save_all' },
    exampleOutput: {
      success: true,
      message: 'All assets saved',
      compensation: {
        operation: 'control_editor.save_all',
        atomic: false,
        rollback: 'unavailable',
        rollbackReason: 'Completed steps are already durable on disk. No editor transaction can reach a finished save, build or render, so nothing here was or can be undone.',
        state: 'completed',
        completed: [{ step: 'save:/Game/Maps/EntryMap', detail: 'level package written to disk' }],
        notCompleted: [],
        skipped: [],
        compensatingCapabilities: [],
        callerAction: '',
      },
    },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
];
