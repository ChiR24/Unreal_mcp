/**
 * run_script: many capability calls in one request, with the loops, conditions and values in between that a model
 * would otherwise carry through its own context (plugin Domains/Scripts). Its own family: folded into one, its action
 * name's words would join that family primary's aliases.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { ANY_EDITOR_STATE, buildCoreRecord } from '../core/builder.js';

const PT = 'system_control';

export const SCRIPT_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: PT,
    action: 'run_script',
    domain: 'system',
    family: 'script',
    topics: ['run a script of calls', 'loop over actors', 'many calls in one', 'call for each result', 'if then else over results', 'all or nothing changes'],
    summary: 'Run a script of capability calls in one request, on every UE 5.x, in the S-expression grammar of read_graph_as_code: (let name (capability.id :param value ...)) calls a capability and keeps what it returns (what a direct execute returns: its declared output fields, the rest under details), name.field.0.field reads into it; (for item list ...) goes over a list, (range n) counts 0 to n-1; (if cond ... (else ...)); (expect "assertion") checks the editor in execute\'s expect language (Crate1.location.z in 0..100, Lamp on Table; the text can be made as it runs: (expect (+ c.label " on Floor"))); (return value) ends the script with its result. Expressions: (not a), (and a b), (or a b), (== a b), (!= a b), (< a b), (<= a b), (> a b), (>= a b), (+ a b) (it also joins text: (+ "Crate_" i)), (- a b), (* a b), (/ a b), (list a b), (obj :x 1 :y 2), literals, null and names; a name is bound once in its block. The whole script is checked before anything runs: every call names a capability, with only parameters its contract declares and each one it requires, and every name is bound before use. Each call is then held to everything a direct execute is (its contract, the caller\'s scope and path limits, the console policy) and runs on its handler inside this one request. run_script always asks for elevated consent, and each capability the script calls that asks for consent must be named in destructive, which that one consent then covers. A failed call or expectation stops the script and rolls back what it changed, as far as editor undo reaches: a Blueprint compile or a level load clears the undo history (rollback then says what could not be undone), saved files stay saved, and assets it created are listed in cleanupCall. A script cannot call run_script, undo, redo or restore_point, and does not start while a restore point is open (RESTORE_POINT_OPEN). The reply: returned, and steps (one line per call: its line, capability, and ok or its error code and message).',
    whenToUse: [
      'The same call must run for many actors or assets, or later calls depend on what earlier ones returned: one script instead of a round trip per call.',
      'Several changes must all happen or none: a failed call or expectation rolls back the ones before it.',
    ],
    whenNotToUse: [
      'One call does the job, or the capability takes many targets itself (spawn with actors, set_transform with actors, build_graph with steps).',
      'Logic the game runs is wanted: write_graph_from_code writes it into a Blueprint.',
    ],
    inputProps: {
      script: { type: 'string', description: 'The script, statements one after another (; starts a comment to the end of the line). Refused before anything runs: SCRIPT_PARSE_ERROR, SCRIPT_INVALID, UNKNOWN_CAPABILITY (a name search does not find), UNDECLARED_PARAMETER and MISSING_REQUIRED_PARAMETER (checked against the capability\'s contract), SCRIPT_UNBOUND_NAME, CONSENT_NOT_LISTED, SCRIPT_STEP_REFUSED. Stopped and rolled back while running: the failed call\'s own error code, SCRIPT_TYPE (an operator or for over the wrong kind of value), SCRIPT_VALUE (a path into a result that holds no such field: the fields it has are named), EXPECTATION_FAILED, ENGINE_ERROR (the engine logged an error during a call), STEP_DEFERRED (a call that answers later cannot run in a script), SCRIPT_BUDGET, SCRIPT_DEADLINE.' },
      destructive: { type: 'array', items: { type: 'string' }, description: 'The capabilities the script calls that ask for consent, by id or alias (control_actor.delete, asset.delete); this run\'s one elevated consent covers exactly these.' },
      maxSteps: { type: 'integer', minimum: 1, maximum: 1000, description: 'Calls the script may make; default 200. Past it the script stops (SCRIPT_BUDGET) and rolls back.' },
      deadlineMs: { type: 'integer', minimum: 100, maximum: 25000, description: 'Milliseconds after which no further call starts; default 20000, at most 25000 (a client gives up on a call at 30 s). Past it the script stops (SCRIPT_DEADLINE) and rolls back.' },
    },
    required: ['script'],
    outputProps: {
      steps: { type: 'array', items: { type: 'string' }, description: 'One line per call made: "line N capability ok" (a loop\'s run of the same call as one line, "ok x25"), or "line N capability CODE: message" for the call that stopped the script.' },
      returned: { type: ['string', 'number', 'boolean', 'array', 'object', 'null'], description: 'The value of (return ...), when the script returned one.' },
      rolledBack: { type: 'boolean', description: 'On a failure: whether everything the script changed was undone.' },
      rollback: { type: 'string', description: 'On a failure: what undoing the script did, or why it could not undo everything.' },
      cleanupCall: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'On a failure: the manage_asset bulk_delete call that removes the assets the script created, which undo does not remove.' },
    },
    outputRequired: ['steps'],
    effect: 'destructive',
    policyOverride: { consent: 'elevated' },
    costLatency: 'interactive',
    costResources: 'medium',
    editorStates: ANY_EDITOR_STATE,
    exampleInput: {
      action: 'run_script',
      script: '(let crates (control_actor.list :filter "Crate"))\n(for c crates.actors\n  (if (< c.location.z 0)\n    (control_actor.set_transform :actorName c.name :location (obj :x c.location.x :y c.location.y :z 50))))\n(expect "Crate1.location.z in 0..100")\n(return crates.count)',
    },
    exampleOutput: {
      success: true,
      message: 'The script ran 2 step(s).',
      steps: ['line 1 control_actor.list ok', 'line 4 control_actor.set_transform ok'],
      returned: 2,
    },
  }),
];
