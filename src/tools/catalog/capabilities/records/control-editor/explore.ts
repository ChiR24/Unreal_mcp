/**
 * Explore record: explore_level, the level explored by a pawn of the running game with sticky random moves from an archive
 * of cells (Go-Explore's first phase without learning; plugin Domains/Play). Its own family, as playtest's is.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildCoreRecord } from '../core/builder.js';

const F = 'explore';
const D = 'editor';

const ROWS = (description: string) => ({ type: 'array', items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true }, description });

export const EXPLORE_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'control_editor', action: 'explore_level', domain: D, family: F,
    editorStates: ['pie'],
    topics: ['explore the level', 'find where the player can fall out', 'find holes the player falls through', 'automatic play test', 'find unreachable areas', 'level coverage', 'find places the player gets stuck'],
    summary: 'Explore the running level with a pawn the way a tester would, to find what a model cannot see by reading it: places the pawn falls out of the level (below its kill height) or leaves the playable area, traps it cannot get out of, parts of the nav mesh no path from the start reaches (islands: a sealed room), and Blueprint errors raised on the way. In a Play session started through this server (others are refused with PLAY_NOT_STARTED_BY_MCP). Go-Explore\'s first phase, without learning: an archive of cells (the grid cell, the height band and the truth of the track assertions), each kept with the moves that first reached it; every round walks the pawn back to a cell (along the nav mesh, else teleported) and explores from there with sticky random moves (the last one repeated 19 times in 20, jumps included), through the pawn\'s own movement: no OS input. op start begins a session for the player\'s pawn or actorName\'s and explores for seconds; continue explores more (cell picks where the next round starts, else cells chosen less often are preferred); read answers with what was found; stop ends the session. Each call answers after its seconds (at most 22); the window is held on screen without focus meanwhile.',
    whenToUse: [
      'Checking a level for holes, ledges the player falls off, rooms nobody can reach, or spots the player gets stuck, before a person plays it.',
      'Measuring how much of the level a pawn can reach from its start (coverage).',
    ],
    whenNotToUse: [
      'Whether one place can be reached from another: get_ai_info info path answers without playing, playtest walks there and judges the leg.',
    ],
    inputProps: {
      op: { type: 'string', enum: ['start', 'continue', 'read', 'stop'], description: 'start a session (and explore), continue exploring, read what was found, or stop the session.' },
      actorName: { type: 'string', description: 'start: the pawn to drive (one with a controller, such as a placed Character an AI controller possesses at Play); default the player\'s.' },
      cellCm: { type: 'number', description: 'start: the cell size on the floor plan, in cm (50-2000, default 200).' },
      heightCm: { type: 'number', description: 'start: the height band, in cm (25-1000, default 100): a ledge above another floor is another cell.' },
      track: { type: 'array', items: { type: 'string' }, description: 'start: assertions in execute\'s expect language whose truth joins the cell (Door_1 hidden, BP_Key exists), at most 16: the same place with a door open is a new cell to explore from. A teleport cannot restore them; such cells are marked.' },
      seed: { type: 'number', description: 'start: the seed of the random moves (default 1): the same seed repeats the same moves.' },
      seconds: { type: 'number', description: 'start and continue: real seconds to explore before answering (1-22, default 10).' },
      cell: { type: 'number', description: 'continue: the archive index of the cell the next round starts from (read with detail lists them).' },
      detail: { type: 'boolean', description: 'read and stop: list the archive, each cell as {cell, where, parent, chosen, leads, flags}, at most 200.' },
    },
    required: ['op'],
    outputProps: {
      cells: { type: 'number', description: 'Cells in the archive: places the pawn stood (none filed in mid-air).' },
      rounds: { type: 'number', description: 'Rounds begun so far, an escape cutting one short included.' },
      coverage: { type: 'number', description: 'Of the nav-mesh cells sampled at start, the share the pawn stood in (0-1).' },
      findings: ROWS('What was found, each {kind (escape_killz, escape_bounds, trap, script_error), text, where [x, y, z], cell (the first cell of its round: explore from it to see it again), t (game seconds)}; one per kind and floor-plan cell, at most 64.'),
      islands: { type: 'array', items: { type: 'array', items: { type: 'number' } }, description: 'Nav-mesh points sampled at start that no path from the start reaches and no visited cell holds, at most 32: a sealed room, a roof the level never connects.' },
      archive: ROWS('With detail: the cells, each {cell, where, parent, chosen, leads, flags}.'),
      stopped: { type: 'string', description: 'Why an exploring call answered: seconds, paused, pawn_lost or world_ended.' },
    },
    outputRequired: [],
    effect: 'write', behavior: { longRunning: true },
    costLatency: 'long-running', // a call explores up to 22 real seconds
    costResources: 'high',
    exampleInput: { action: 'explore_level', op: 'start', actorName: 'Tester', seconds: 15 },
    exampleOutput: { success: true, message: 'Explored.', cells: 42, rounds: 7, coverage: 0.61, findings: [{ kind: 'escape_killz', text: 'the pawn fell below the level\'s kill height', where: [1200, -40, -520], cell: 9, t: 6.2 }], islands: [[3400, 200, 10]], stopped: 'seconds' },
  }),
];
