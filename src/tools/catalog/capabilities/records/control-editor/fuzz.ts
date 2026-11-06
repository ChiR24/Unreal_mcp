/**
 * Fuzz record: fuzz_blueprint, a Blueprint actor of the running game called through its own functions with edge-case
 * inputs, every Blueprint error a call raises named with its node (plugin Domains/Play). Its own family, as playtest's
 * and explore's are.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildCoreRecord } from '../core/builder.js';

const ROWS = (description: string) => ({ type: 'array', items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true }, description });

export const FUZZ_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'control_editor', action: 'fuzz_blueprint', domain: 'editor', family: 'fuzz',
    editorStates: ['pie'],
    topics: ['fuzz a blueprint', 'fuzz test', 'edge case inputs', 'call blueprint functions with bad inputs', 'which inputs break a function', 'try edge cases on a blueprint function'],
    summary: 'Call a Blueprint\'s functions in the running game with edge-case inputs and catch the errors they raise, before a player does. Each function and custom event is called once with every input at a plain value (numbers 1, false, text "A", for an object the actor itself or another actor of that class from the game, an array of 3), then once per edge value of one input at a time with the others kept plain: numbers 0, -1 and the int limits (or +-1e9 for floats), empty and 256-character text, None, an empty array, each enum value (up to 8), a zero and a huge vector. A failing call is so already the smallest change from one that works. A call fails when it raises a Blueprint runtime error: an Accessed None, an endless loop, a divide or modulo by zero, an array index out of range; each comes back with the node it came from (node title and nodeId, the node\'s GUID; a pure node such as a divide runs inside the node its value feeds, which is the one named, and the message names the function). Only in a Play session started through this server (others are refused with PLAY_NOT_STARTED_BY_MCP), on actorName or the first instance of blueprintPath placed in the game. By default the Blueprint\'s own functions and custom events that take inputs (functions names any, a parent\'s included; one without inputs is called once). The calls run one after another inside this call, at most maxCalls (default 200) and 20 s; a function stops after its third failing call. They run the game\'s own logic: what they change stays changed in the running game (never in the level), and what runs after a Delay or a timer runs later, outside the call it belongs to. Break on Exceptions is held off during the calls. Refused with BREAKPOINTS_SET (the Blueprint has breakpoints: a call reaching one would stop the game in the debugger), ACTOR_NOT_FOUND, BLUEPRINT_NOT_FOUND, NOT_A_BLUEPRINT (a native actor), FUNCTION_NOT_FOUND (lists the Blueprint\'s own), NOTHING_TO_FUZZ (no function of its own takes inputs).',
    whenToUse: [
      'Checking a Blueprint\'s functions against inputs nobody tried yet (0, negative numbers, empty text, None) before a player does.',
      'Reproducing a lint_blueprint finding (divide_by_zero, index_out_of_range) with the input that triggers it.',
    ],
    whenNotToUse: [
      'Errors the game already raised: runtime_report lists each with its node and cause.',
      'Reading a Blueprint for mistakes without running it: lint_blueprint.',
      'Watching what a node receives while the game plays: set_logpoints.',
    ],
    inputProps: {
      actorName: { type: 'string', description: 'The Blueprint actor in the running game to call (its label or name).' },
      blueprintPath: { type: 'string', description: 'Instead of actorName: a Blueprint whose first instance in the running game is called; its own functions are the default.' },
      functions: { type: 'array', items: { type: 'string' }, minItems: 1, maxItems: 64, description: 'The functions and custom events to call, a parent Blueprint\'s included (default: the Blueprint\'s own that take inputs, at most 64).' },
      maxCalls: { type: 'number', minimum: 1, maximum: 500, description: 'Calls at most, across every function (default 200).' },
    },
    required: [],
    requiredOneOf: ['actorName', 'blueprintPath'],
    outputProps: {
      target: { type: 'string', description: 'The actor called.' },
      blueprintPath: { type: 'string', description: 'The Blueprint whose functions were the default.' },
      calls: { type: 'number', description: 'Calls made.' },
      failures: { type: 'number', description: 'Calls that raised a Blueprint runtime error.' },
      functions: ROWS('Per function: function, first (each input\'s plain value), planned (the calls its inputs give), calls (made: fewer after its third failing call, or when the run stopped), failed (at most 3, each {inputs: the input that differed from first, none when the plain call itself failed; errors: [{message, node, nodeId, count}]}).'),
      stopped: { type: 'string', description: 'Why the run ended before every planned call: budget (maxCalls), deadline (20 s), target_destroyed or play_ending (stoppedBy names the call).' },
      stoppedBy: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: '{function, inputs}: the call after which the actor was gone, or Play was ending.' },
      seconds: { type: 'number', description: 'Real seconds the calls took.' },
    },
    outputRequired: [],
    effect: 'write', behavior: { longRunning: true },
    costLatency: 'long-running', // up to 20 s of calls
    costResources: 'medium',
    exampleInput: { action: 'fuzz_blueprint', actorName: 'BP_Scoreboard_C_0', functions: ['AverageScore'] },
    exampleOutput: {
      success: true, message: 'Called 1 function(s) 9 time(s) with edge-case inputs: 1 call(s) raised Blueprint errors (failed, per function).',
      target: 'BP_Scoreboard_C_0', blueprintPath: '/Game/UI/BP_Scoreboard', calls: 9, failures: 1,
      functions: [{ function: 'AverageScore', first: { Total: 1, Count: 1 }, planned: 9, calls: 9, failed: [{ inputs: { Count: 0 }, errors: [{ message: 'Divide by zero: Divide_DoubleDouble', node: 'Divide', nodeId: '3F2A9C7E4B1D4E0A9C55D1E2F3A4B5C6' }] }] }],
      seconds: 0.02,
    },
  }),
];
