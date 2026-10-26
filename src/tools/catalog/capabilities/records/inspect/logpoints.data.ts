/**
 * Logpoint records (S6): set_logpoints, read_logpoints, clear_logpoints. Nodes of a Blueprint marked to record their pin
 * values each time they run in Play, without pausing (Domains/Debug/Logpoints/).
 */
import type { CapabilityRecordSource } from '../../model.js';
import { ANY_EDITOR_STATE, buildCoreRecord } from '../core/builder.js';

const D = 'inspect';
const FAMILY = 'logpoints';

const BLUEPRINT_PATH = { type: 'string', description: 'The Blueprint whose nodes are marked, as /Game/Folder/BP_Name.' };
const NODE_IDS = { type: 'array', minItems: 1, maxItems: 32, items: { type: 'string' }, description: 'Nodes by id: the GUID edit_graph and get_graph_details give, or the tag read_graph_as_code prints (n + the GUID\'s first hex digits, as n3fa9c1).' };
const POINTS = { type: 'array', items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true }, description: 'Each marked node as {nodeId, node (its title), graph, blueprintPath, runs (times it ran this play session), timeline}: timeline lists its newest runs as {t (game seconds), object (the actor or object it ran on), in {pin: value}, out {pin: value}} by pin name; out is present once the node has finished (a latent node still waiting keeps only in). Values are text as the property exports it, read through reroute knots; "(not kept by the compiler)" for a value the compiler folded away. Recorded in Play only: Simulate (settle_physics runs one) neither records nor empties them.' };

export const LOGPOINT_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'inspect', action: 'set_logpoints', domain: D, family: FAMILY, editorStates: ANY_EDITOR_STATE,
    topics: ['logpoint', 'trace blueprint execution', 'breakpoint without pausing', 'record pin values during play', 'log node values', 'print a blueprint value during play', 'debug blueprint values', 'what values did the node get'],
    summary: 'Mark Blueprint nodes to record the values on their pins each time they run in Play, without pausing the game: a print statement that changes no graph. Before Play or during it; each play session starts the timelines empty. Read them with read_logpoints. Marks last until clear_logpoints or the editor closes; at most 32 at once.',
    whenToUse: ['Finding why a Blueprint does the wrong thing at run time: what a Branch tested, what a Set wrote, what a call received, on which actor and when.', 'Checking a value over time (a timer, a counter, a health drain) without adding Print String nodes.'],
    whenNotToUse: ['A runtime error is the question: runtime_report lists each one with its node and cause.', 'A pure node (no exec pins): it runs inside the node that reads it; mark that one (refused with PURE_NODE).'],
    inputProps: {
      blueprintPath: BLUEPRINT_PATH,
      nodeIds: NODE_IDS,
      maxHits: { type: 'number', description: 'How many of each node\'s newest runs are kept (1-500, default 100); runs counts them all.' },
    },
    required: ['blueprintPath', 'nodeIds'],
    outputProps: { logpoints: POINTS },
    outputRequired: [],
    effect: 'read',
    exampleInput: { action: 'set_logpoints', blueprintPath: '/Game/BP_Door', nodeIds: ['n3fa9c1'] },
  }),
  buildCoreRecord({
    parentTool: 'inspect', action: 'read_logpoints', domain: D, family: FAMILY, editorStates: ANY_EDITOR_STATE,
    summary: 'Read what marked Blueprint nodes recorded in Play: for each node its runs and newest values, input and output pins by name, with the game time and the object it ran on. Readable during Play and after it stops, until the next play session starts.',
    whenToUse: ['After set_logpoints and some play: what each marked node received and produced.'],
    whenNotToUse: ['No node is marked yet: set_logpoints first.'],
    inputProps: {
      blueprintPath: { ...BLUEPRINT_PATH, description: 'Only the marks of this Blueprint (default: all).' },
      nodeIds: { ...NODE_IDS, minItems: 0, description: 'Only these marks, by GUID or tag (default: all).' },
      last: { type: 'number', description: 'How many of each node\'s newest runs to list (1-500, default 20).' },
    },
    required: [],
    outputProps: { logpoints: POINTS },
    outputRequired: [],
    effect: 'read',
    exampleInput: { action: 'read_logpoints', blueprintPath: '/Game/BP_Door', last: 5 },
    exampleOutput: { success: true, message: '1 mark(s) read.', logpoints: [{ nodeId: '3FA9C1D2E4B5A6978877665544332211', node: 'Branch', graph: 'EventGraph', blueprintPath: '/Game/BP_Door', runs: 3, timeline: [{ t: 4.512, object: 'BP_Door_C_1', in: { Condition: 'false' } }] }] },
  }),
  buildCoreRecord({
    parentTool: 'inspect', action: 'clear_logpoints', domain: D, family: FAMILY, editorStates: ANY_EDITOR_STATE,
    summary: 'Remove logpoint marks: all of them, those of one Blueprint, or chosen nodes. With none left, nothing listens to the running game any more.',
    whenToUse: ['A trace is done, or before marking other nodes past the limit of 32.'],
    whenNotToUse: ['Only the recorded values should be emptied: the next play session starts them empty.'],
    inputProps: {
      blueprintPath: { ...BLUEPRINT_PATH, description: 'Only the marks of this Blueprint (default: all).' },
      nodeIds: { ...NODE_IDS, minItems: 0, description: 'Only these marks, by GUID or tag (default: all).' },
    },
    required: [],
    outputProps: {
      cleared: { type: 'number', description: 'Marks removed.' },
      remaining: { type: 'number', description: 'Marks left.' },
    },
    outputRequired: [],
    effect: 'read',
    exampleInput: { action: 'clear_logpoints' },
  }),
];
