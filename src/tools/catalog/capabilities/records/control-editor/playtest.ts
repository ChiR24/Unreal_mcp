/**
 * Playtest record: playtest, a pawn of the running game walked to a goal and the leg judged (plugin Domains/Play). Its
 * own family: folded into the session family, its action name's words would join that family primary's aliases.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildCoreRecord } from '../core/builder.js';

const F = 'playtest';
const D = 'editor';

const point = (description: string) => ({
  type: 'object',
  properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } },
  additionalProperties: false,
  description,
});

export const PLAYTEST_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'control_editor', action: 'playtest', domain: D, family: F,
    editorStates: ['pie'],
    topics: ['playtest', 'test a level', 'walk to the goal', 'can the player get there', 'drive the player', 'play test verdict'],
    summary: 'Walk a pawn of the running game to a goal along the nav mesh and judge the leg, in a Play session started through this server (control_editor play; a session started in the editor is refused with PLAY_NOT_STARTED_BY_MCP, since someone may be playing it). The player\'s pawn by default, or actorName\'s; goal is a point, or goalActor where that actor is. The verdict: pass only with evidence (the goal reached within acceptRadius, every check held, no script error), fail with what went wrong (stuck: no progress for stuckSeconds; fell: below its start by fallCm or out of the world; a script error during the leg; a check that did not hold), inconclusive (the nav mesh gives no full path, so the leg was not tried; still on the way at maxSeconds; or the game paused itself during the leg), error (the game left the level: Play stopped or the level changed). A paused game (a title screen, a pause menu) is refused with GAME_PAUSED: resume it with control_editor play, control resume, unpauseGame true. checks are assertions in execute\'s expect language, judged in the running game when the leg ends. The editor window is held on screen without focus for the leg, so the game runs at speed. The reply: verdict, ended (reached, stuck, fell, timeout, pawn_lost), checks, scriptErrors, timeline (where the pawn was, at most 50 samples), last, bestDistance, traveled, frames.',
    whenToUse: ['Whether the player can actually get from one place to another in the running game, with what went wrong if not.'],
    whenNotToUse: ['Only whether a nav path exists, without playing: manage_ai get_ai_info info path.'],
    inputProps: {
      goal: point('Where the pawn should get to.'),
      goalActor: { type: 'string', description: 'An actor of the running game to walk to, in place of goal.' },
      actorName: { type: 'string', description: 'The pawn to drive (one with a controller); default the player\'s.' },
      acceptRadius: { type: 'number', description: 'How near the goal counts as reached, in cm (default 100).' },
      stuckSeconds: { type: 'number', description: 'Game seconds without getting nearer that count as stuck (default 2).' },
      fallCm: { type: 'number', description: 'How far below its start the pawn may drop before it counts as fallen (default 500).' },
      maxSeconds: { type: 'number', description: 'Game seconds the leg may take (default 15, at most 60; at most 25 real seconds).' },
      checks: { type: 'array', items: { type: 'string' }, description: 'Assertions judged when the leg ends, in execute\'s expect language (Door_1 hidden, BP_Player.Health > 0), at most 16.' },
    },
    required: [],
    outputProps: {
      verdict: { type: 'string', enum: ['pass', 'fail', 'inconclusive', 'error'], description: 'The judgement of the leg.' },
      ended: { type: 'string', description: 'How the leg ended: reached, stuck, fell, timeout, paused, pawn_lost, world_ended, or the reason no leg was tried (no_path, partial, start_off_navmesh, goal_off_navmesh).' },
      checks: { type: 'array', items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true }, description: 'Each check: text, verdict (ok, failed, unknown), actual.' },
      scriptErrors: { type: 'array', items: { type: 'string' }, description: 'Blueprint runtime errors during the leg, each with the node that raised it.' },
      timeline: { type: 'array', items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true }, description: 'Where the pawn was: { t, location }, at most 50 samples.' },
      last: { type: 'array', items: { type: 'number' }, description: 'Where the pawn ended, [x, y, z].' },
      bestDistance: { type: 'number', description: 'The nearest it got to the goal, in cm (2D).' },
      traveled: { type: 'number', description: 'How far it moved, in cm.' },
      frames: { type: 'number', description: 'Frames the leg ran.' },
    },
    outputRequired: [],
    effect: 'write', behavior: { longRunning: true },
    costLatency: 'long-running', // a leg runs up to 25 real seconds
    costResources: 'medium',
    exampleInput: { action: 'playtest', goalActor: 'Exit_Door', checks: ['Exit_Door hidden'] },
    exampleOutput: { success: true, message: 'Pass: the goal was reached after 4.2 s, every check held.', verdict: 'pass', ended: 'reached', checks: [{ text: 'Exit_Door hidden', verdict: 'ok', actual: 'hidden' }], scriptErrors: [], bestDistance: 80, traveled: 1220, frames: 260 },
  }),
];
