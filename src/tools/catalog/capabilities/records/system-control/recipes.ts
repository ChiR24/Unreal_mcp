/**
 * Recipe records (S14): save_recipe, run_recipe, list_recipes. A run_script script kept in the project under a name, with
 * parameters, and run later as one checked call (plugin Domains/Scripts/McpRecipes). propose_recipe (S13) writes one from
 * the caller's last calls (McpRecipeInduce). Own records: a fold takes its first member's effect, and a read must not
 * share one with a destructive run.
 */
import { CODE_MAX_LENGTH } from '../../constants.js';
import type { CapabilityRecordSource } from '../../model.js';
import { ANY_EDITOR_STATE, buildCoreRecord } from '../core/builder.js';

const D = 'system';
const F = 'recipes';
const NAME = { type: 'string', description: 'The recipe\'s name: 1-64 letters, digits or underscores (it is the file name).' };

export const RECIPE_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'system_control', action: 'save_recipe', domain: D, family: F, editorStates: ANY_EDITOR_STATE,
    topics: ['save a recipe', 'save this workflow', 'reusable script', 'remember how to do this'],
    summary: 'Keep a run_script script in the project as a named recipe with parameters, so a later call (or another agent) runs it as one checked call with run_recipe. The script is checked as run_script checks one, its parameters bound by name (each a variable the script reads); nothing runs at save time. destructive lists the destructive capabilities it calls, as run_script takes them, and run_recipe\'s consent covers exactly those. A taken name is refused (RECIPE_EXISTS) unless overwrite is true. Stored under Saved/EngineWire/Recipes/<name>.json.',
    whenToUse: [
      'A sequence of calls worked and will be needed again with other values: save it, then run_recipe.',
      'Search lists a saved recipe beside the capabilities (its recipes field, for a caller that may read the project) when its name and summary name the words of the request: write the summary as a caller would ask for the task.',
    ],
    whenNotToUse: ['Running a script once: run_script.'],
    inputProps: {
      name: NAME,
      summary: { type: 'string', maxLength: 300, description: 'What the recipe does, in a sentence: search and list_recipes match its words.' },
      script: { type: 'string', maxLength: CODE_MAX_LENGTH, description: 'The script, in run_script\'s language; it reads each of paramNames as a variable. Its strings take only \\" and \\\\ as escapes, as there: \\n stays a backslash and an n, and a line break inside the quotes is a line break.' },
      paramNames: { type: 'array', items: { type: 'string' }, description: 'The parameter names (at most 16), each bound by run_recipe\'s args before the script runs.' },
      destructive: { type: 'array', items: { type: 'string' }, description: 'The destructive capabilities the script calls, as run_script takes them.' },
      overwrite: { type: 'boolean', description: 'Replace a recipe of the same name (default false).' },
      remove: { type: 'boolean', description: 'Delete the recipe named instead of saving one (script is then not needed); RECIPE_NOT_FOUND when there is none.' },
    },
    required: ['name'],
    outputProps: {
      name: { type: 'string', description: 'The recipe saved or removed.' },
      path: { type: 'string', description: 'Its file, when saved.' },
      removed: { type: 'boolean', description: 'Present and true when remove deleted it.' },
    },
    outputRequired: [],
    effect: 'write',
    exampleInput: { action: 'save_recipe', name: 'raise_crates', summary: 'Lift every crate under a height onto the floor.', paramNames: ['minZ'], script: '(let crates (control_actor.list :filter "Crate"))\n(for c crates.actors\n  (if (< c.location.z minZ)\n    (control_actor.set_transform :actorName c.name :location (obj :x c.location.x :y c.location.y :z minZ))))' },
  }),
  buildCoreRecord({
    parentTool: 'system_control', action: 'run_recipe', domain: D, family: F, editorStates: ANY_EDITOR_STATE,
    topics: ['run a recipe', 'run a saved workflow'],
    summary: 'Run a recipe saved with save_recipe: args gives each parameter a value (bound as (let name value) before its first line), and the script runs exactly as run_script runs one: every step checked as a direct execute is, its destructive steps covered by this call\'s one consent, and everything rolled back when a step or an expectation fails. A run that succeeds marks the recipe verified with the engine version. RECIPE_NOT_FOUND for an unknown name, RECIPE_ARGS when args lacks a parameter.',
    whenToUse: ['A recipe exists for the task: search lists it under recipes, and list_recipes finds it by its words.'],
    whenNotToUse: ['No recipe fits: run_script, or the capabilities themselves.'],
    inputProps: {
      name: NAME,
      args: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'Each parameter\'s value by name: a number, text, true/false, a list or an object.' },
      maxSteps: { type: 'integer', minimum: 1, maximum: 1000, description: 'As run_script: calls the recipe may make; default 200.' },
      deadlineMs: { type: 'integer', minimum: 100, maximum: 25000, description: 'As run_script: milliseconds from the start after which no further call starts, a call that answers later waited for until then; default 20000.' },
    },
    required: ['name'],
    outputProps: {
      steps: { type: 'array', items: { type: 'string' }, description: 'As run_script: one line per call made.' },
      returned: { type: ['string', 'number', 'boolean', 'array', 'object', 'null'], description: 'The value of (return ...), when the recipe returned one.' },
      rolledBack: { type: 'boolean', description: 'On a failure: whether everything the recipe changed was undone.' },
      rollback: { type: 'string', description: 'On a failure: what undoing it did, or why it could not undo everything.' },
      cleanupCall: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'On a failure: the call that removes the assets the recipe created.' },
    },
    outputRequired: [],
    effect: 'destructive',
    policyOverride: { consent: 'elevated' },
    costLatency: 'interactive',
    exampleInput: { action: 'run_recipe', name: 'raise_crates', args: { minZ: 10 } },
  }),
  buildCoreRecord({
    parentTool: 'system_control', action: 'list_recipes', domain: D, family: F, editorStates: ANY_EDITOR_STATE,
    topics: ['list recipes', 'saved workflows'],
    summary: 'List the project\'s recipes: name, summary, paramNames, destructive, and verified (the engine version and time of the last run that succeeded) when a run has. query keeps those whose name or summary holds every word of it.',
    whenToUse: ['Before writing a multi-call script: a saved recipe may already do it.'],
    whenNotToUse: ['Reading what a recipe does step by step: its script is in its file under Saved/EngineWire/Recipes/.'],
    inputProps: { query: { type: 'string', description: 'Words every listed recipe\'s name or summary holds.' } },
    required: [],
    outputProps: { recipes: { type: 'array', items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true }, description: 'Each {name, summary, paramNames, destructive, verified}.' } },
    outputRequired: [],
    effect: 'read',
    exampleInput: { action: 'list_recipes', query: 'crate' },
  }),
  buildCoreRecord({
    parentTool: 'system_control', action: 'propose_recipe', domain: D, family: F, editorStates: ANY_EDITOR_STATE,
    topics: ['turn what I just did into a recipe', 'make a recipe from my last calls', 'record these steps as a recipe', 'reuse the steps that worked'],
    summary: 'Turn your last calls that succeeded (up to 32, kept since the editor started) into a recipe script for save_recipe, saving nothing. A value a call took from an earlier call\'s reply becomes that call\'s variable (s1.assetPath); a target path, name, label, location, rotation, scale or written value no earlier call produced becomes a parameter, its value this run\'s (args), one parameter per distinct value; a field that shapes the call (a class, property, pin or function name) stays as written. A read whose reply no later call uses is left out, and so are calls a script cannot make (skipped). The reply holds the script, paramNames, args, destructive and saveCall, the save_recipe call that keeps it: read the script first, then send saveCall with a summary a caller would search for.',
    whenToUse: [
      'A sequence of calls just worked and will be needed again with other values: propose it, check it, then send its saveCall.',
    ],
    whenNotToUse: ['The script is already written: save_recipe.', 'Running the steps again now: run_script.'],
    inputProps: {
      steps: { type: 'integer', minimum: 1, maximum: 32, description: 'How many of your last calls that succeeded to read, oldest left out first (default all kept, up to 32).' },
      name: NAME,
      summary: { type: 'string', maxLength: 300, description: 'Put in saveCall: what the recipe does, in a sentence.' },
    },
    required: [],
    outputProps: {
      script: { type: 'string', description: 'The recipe script: one (let sN (capability :param value ...)) per call kept, parameters and earlier variables in place of the values they stand for.' },
      paramNames: { type: 'array', items: { type: 'string' }, description: 'The parameters, as save_recipe takes them.' },
      args: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'Each parameter\'s value in the calls read: run_recipe with these args repeats them.' },
      destructive: { type: 'array', items: { type: 'string' }, description: 'The capabilities in the script that ask for consent, as save_recipe takes them.' },
      stepCount: { type: 'number', description: 'Calls in the script.' },
      skipped: { type: 'array', items: { type: 'string' }, description: 'Present when calls a script cannot make were left out (run_script, run_recipe, undo).' },
      saveCall: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'The save_recipe execute call with this proposal (name default proposed_recipe): send it to keep the recipe.' },
    },
    outputRequired: [],
    effect: 'read',
    exampleInput: { action: 'propose_recipe', steps: 3, name: 'red_crate', summary: 'Make a red material and put it on a new crate.' },
  }),
];
