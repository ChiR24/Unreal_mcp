/**
 * PIE session lifecycle records: play, stop, stop_pie, pause, resume, eject,
 * possess.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildCoreRecord } from '../core/builder.js';
import { P } from './properties.js';

const F = 'session';
const D = 'editor';

export const SESSION_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'control_editor', action: 'play', domain: D, family: F,
    topics: ['play in editor', 'pie', 'start pie', 'start game', 'run game', 'press play', 'simulate'],
    aliases: ['control_editor.start_pie'],
    summary: 'Start Play-In-Editor (PIE) session; answers once the play world has begun play.',
    whenToUse: ['A PIE session must be started to test gameplay.'],
    whenNotToUse: ['PIE is already running.'],
    inputProps: {},
    required: [],
    effect: 'write',
    costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'play' },
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'stop', domain: D, family: F,
    topics: ['stop pie', 'stop game', 'end play', 'exit pie'],
    summary: 'Stop the running PIE session.',
    whenToUse: ['The PIE session must be terminated.'],
    whenNotToUse: ['PIE should only be paused.'],
    inputProps: {},
    required: [],
    effect: 'write', behavior: { idempotency: 'idempotent' },
    costLatency: 'interactive',
    exampleInput: { action: 'stop' },
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'stop_pie', dispatchAction: 'stop',
    domain: D, family: F,
    summary: 'Stop the running PIE session (alias for stop).',
    whenToUse: ['The PIE session must be terminated using the stop_pie alias.'],
    whenNotToUse: ['PIE should only be paused.'],
    inputProps: {},
    required: [],
    effect: 'write', behavior: { idempotency: 'idempotent' },
    costLatency: 'interactive',
    exampleInput: { action: 'stop_pie' },
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'pause', domain: D, family: F,
    summary: 'Pause the running PIE session.',
    whenToUse: ['PIE must be paused without stopping.'],
    whenNotToUse: ['PIE is not running or already paused.'],
    inputProps: {},
    required: [],
    effect: 'write', behavior: { idempotency: 'idempotent' },
   
    exampleInput: { action: 'pause' },
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'resume', domain: D, family: F,
    summary: 'Resume a paused PIE session.',
    whenToUse: ['A paused PIE session must resume.'],
    whenNotToUse: ['PIE is not paused.'],
    inputProps: {},
    required: [],
    effect: 'write', behavior: { idempotency: 'idempotent' },
   
    exampleInput: { action: 'resume' },
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'eject', domain: D, family: F,
    summary: 'Eject from the currently possessed pawn in PIE.',
    whenToUse: ['The player must detach from the possessed pawn.'],
    whenNotToUse: ['PIE is not running or no pawn is possessed.'],
    inputProps: {},
    required: [],
    effect: 'write',
   
    exampleInput: { action: 'eject' },
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'possess', domain: D, family: F,
    summary: 'Possess a specific actor by name in PIE.',
    whenToUse: ['The player must take control of a specific actor.'],
    whenNotToUse: ['PIE is not running or the target actor does not exist.'],
    inputProps: { actorName: P.actorName },
    required: ['actorName'],
    effect: 'write',
   
    exampleInput: { action: 'possess', actorName: 'BP_PlayerCharacter' },
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'restart_editor', domain: D, family: F,
    topics: ['restart editor', 'reload editor', 'apply plugin change', 'relaunch editor', 'quit editor', 'close editor', 'exit editor', 'unsaved packages', 'list unsaved changes'],
    summary: 'Restart the editor process, relaunching the same project, or close it (relaunch false). validateOnly lists the unsaved packages without doing either.',
    // enable_plugin and several project settings answer "restart the editor
    // for it to take effect", and nothing could: an automated pipeline hit a
    // wall there that only a human could clear.
    whenToUse: ['A plugin was enabled or disabled and the change needs a restart to take effect.',
      'A pipeline needs to know whether a restart is safe right now; pass validateOnly.',
      'A project setting that only applies at startup must be picked up.',
      'The editor is no longer needed and should close (relaunch false).',
      'Which packages have unsaved changes must be known (validateOnly).'],
    whenNotToUse: ['Only a level needs reloading; use open_level.',
      'A PIE session should end; use stop.'],
    inputProps: {
      validateOnly: P.validateOnly, discardUnsaved: P.discardUnsaved, delaySeconds: P.delaySeconds,
      relaunch: { type: 'boolean', description: 'false closes the editor instead of relaunching it (default true). Unsaved packages are refused the same way.' },
    },
    // Unsaved packages are refused rather than silently discarded, because
    // "the editor restarted" reads the same either way. The receipt is sent
    // before the restart fires, so a caller can tell acceptance from a crash.
    outputProps: {
      restarting: { type: 'boolean', description: 'True once the restart has been scheduled; false under validateOnly or when closing.' },
      closing: { type: 'boolean', description: 'True once closing the editor (relaunch false) has been scheduled.' },
      validateOnly: { type: 'boolean', description: 'True when this call only reported what a restart would do.' },
      wouldRestart: { type: 'boolean', description: 'Under validateOnly, whether a real restart would proceed.' },
      unsavedPackages: { type: 'array', items: { type: 'string', description: 'Package path.' }, description: 'Packages with unsaved changes a restart would discard.' },
      unsavedCount: { type: 'integer', description: 'How many packages have unsaved changes.' },
      delaySeconds: { type: 'number', description: 'Seconds the editor waits before relaunching.' },
      discardedPackageCount: { type: 'integer', description: 'Unsaved packages discarded by this restart.' },
      projectPath: { type: 'string', description: 'Project the editor relaunches with.' },
    },
    outputRequired: ['restarting'],
    effect: 'destructive', behavior: { idempotency: 'non-idempotent', longRunning: true },
    costLatency: 'long-running', costResources: 'high',
    exampleInput: { action: 'restart_editor', validateOnly: true },
    exampleOutput: { success: true, message: 'Restart would proceed.', restarting: false, validateOnly: true, wouldRestart: true, unsavedCount: 0, delaySeconds: 1 },
  }),
];
