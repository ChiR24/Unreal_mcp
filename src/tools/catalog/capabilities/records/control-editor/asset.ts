/**
 * Asset and level navigation records: open_asset, close_asset, open_level,
 * focus_actor, save_all.
 */
import type { CapabilityRecordSource, JsonObject } from '../../model.js';
import { buildCoreRecord } from '../core/builder.js';
import { P } from './properties.js';

const F = 'asset';
const D = 'editor';

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
    inputProps: { assetPath: P.assetPath },
    required: ['assetPath'],
    effect: 'read',
    costLatency: 'interactive',
    exampleInput: { action: 'open_asset', assetPath: '/Game/Materials/M_Base' },
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'close_asset', domain: D, family: F,
    summary: 'Close every editor open on an asset, by asset path. Fails with EDITOR_NOT_OPEN when none is open; the asset is never loaded to do this.',
    whenToUse: ['An open asset editor must be closed.'],
    whenNotToUse: ['The asset is not open.', 'The Unreal Editor itself must close: that is restart_editor with relaunch false.'],
    inputProps: { assetPath: P.assetPath },
    required: ['assetPath'],
    effect: 'write',
   
    exampleInput: { action: 'close_asset', assetPath: '/Game/Materials/M_Base' },
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'open_level', domain: D, family: F,
    topics: ['open map', 'open level asset'],
    summary: 'Open a level by asset path, loading it as the current level.',
    whenToUse: ['A different level must be loaded into the editor.'],
    whenNotToUse: ['The level is already loaded.'],
    inputProps: { levelPath: P.levelPath, path: P.path, assetPath: P.assetPath },
    required: ['levelPath'],
    effect: 'write',
    costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'open_level', levelPath: '/Game/Maps/EntryMap' },
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'focus_actor', domain: D, family: F,
    aliases: ['control_editor.move_camera_to_actor'],
    summary: 'Focus the viewport camera on a specific actor by name.',
    whenToUse: ['The viewport must frame a specific actor.'],
    whenNotToUse: ['The actor does not exist in the current level.'],
    inputProps: { actorName: P.actorName, name: P.name },
    required: ['actorName'],
    effect: 'read',
   
    exampleInput: { action: 'focus_actor', actorName: 'BP_PlayerStart' },
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'save_all', domain: D, family: F,
    topics: ['save all', 'save everything', 'save dirty packages', 'save project', 'save assets', 'save asset', 'save one asset'],
    summary: 'Save all dirty assets and levels in the editor, or only the ones listed in assetPaths.',
    whenToUse: ['All unsaved changes must be persisted.', 'Only the assets this task changed should be saved (assetPaths), leaving other unsaved work alone.'],
    whenNotToUse: ['Nothing has changed since the last save.'],
    inputProps: {
      // Saving everything also wrote out unrelated work someone had open, and the
      // contract pointed at a per-asset save that did not exist.
      assetPaths: { type: 'array', items: { type: 'string' }, description: 'Save only these assets or levels, e.g. ["/Game/UI/WBP_Menu"], and leave every other dirty package as it is; omit to save everything dirty.' },
    },
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
  }),
];
