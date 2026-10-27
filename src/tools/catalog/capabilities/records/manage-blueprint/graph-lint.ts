/**
 * lint_blueprint: the mistakes a read of a Blueprint's graphs finds before Play does (plugin
 * Domains/BlueprintGraph/Lint). Its own record: folded into a family, its action name's words would
 * join the family primary's aliases.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { BP_PLUGINS, buildRecord } from './helpers.js';
import { P } from './properties.js';

const ROW = { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true } as const;
/** The plugin's rule list, in its order (McpBPLint.cpp; a contract test holds the two equal). */
export const LINT_RULES = ['tick_heavy', 'unreachable', 'random_fanout', 'infinite_while', 'never_assigned', 'unguarded_read'];

export const GRAPH_LINT_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({
    id: 'blueprint.lint_blueprint',
    action: 'lint_blueprint',
    family: 'lint',
    domain: 'blueprint',
    topics: ['lint blueprint', 'blueprint mistakes', 'check a blueprint for bugs', 'slow event tick', 'nodes that never run', 'accessed none before play'],
    summary: 'Read every graph of a Blueprint for the mistakes that show in Play, each with the node to fix: tick_heavy (Event Tick reaches a level search such as Get All Actors Of Class, or a spawn, every frame), unreachable (no event or function leads to a node, so it never runs), random_fanout (one pure Random node read by several nodes rolls once per read), infinite_while (a While Loop whose Condition is always true), never_assigned (an object variable read but never set, neither Instance Editable nor Expose on Spawn, with no default: every read is None), unguarded_read (an object variable set from a cast, a call or nothing, used as the target of a call or property where not every path to it checks it first with Is Valid, a validated get or a cast: a None there is an Accessed None). Nothing compiles or changes, and a function library is read too. The same rules run in the editor\'s Data Validation (Validate Assets, saves, validate_assets dataValidation): infinite_while fails the asset, the rest warn.',
    whenToUse: [
      'A Blueprint should be checked before Play, or after an edit, for mistakes that only show while the game runs.',
      'A game runs slowly and a Blueprint\'s Event Tick may search the level every frame.',
      'An Accessed None should be found before it fires (never_assigned, unguarded_read).',
    ],
    whenNotToUse: [
      'An error already fired in Play: inspect runtime_report lists it, with the cause of an Accessed None.',
      'The Blueprint does not compile: compile it and read its messages (manage_blueprint compile).',
    ],
    inputProps: {
      blueprintPath: P.blueprintPath,
      rules: { type: 'array', items: { type: 'string', enum: LINT_RULES }, description: 'The rules to run, default all: tick_heavy, unreachable, random_fanout, infinite_while, never_assigned, unguarded_read. An unknown name is INVALID_ARGUMENT.' },
      maxFindings: { type: 'number', minimum: 1, maximum: 200, description: 'Most findings listed (1-200, default 50); counts and findingCount still count them all.' },
    },
    required: ['blueprintPath'],
    outputProps: {
      findings: { type: 'array', items: ROW, 'x-unreal-reflection-boundary': true, description: 'One row per finding: rule, severity (warning, or error for a loop that never ends), graph, node (its title), nodeId (as edit_graph takes it) and detail (what is wrong and the fix). unreachable names only the first node of each dead chain, unguarded_read the first unchecked use of each variable (detail counts the rest).' },
      counts: { type: 'object', additionalProperties: { type: 'number' }, description: 'Findings per rule, listed or not.' },
      findingCount: { type: 'number', description: 'Every finding, listed or not.' },
      truncated: { type: 'boolean', description: 'True when there were more findings than maxFindings: the rest are counted, not listed.' },
      graphsChecked: { type: 'number', description: 'Graphs read: event graphs, functions and the graphs they collapse; macros and delegate signatures are not.' },
    },
    outputRequired: [],
    effect: 'read',
    latency: 'interactive',
    resources: 'low',
    plugins: BP_PLUGINS,
    exampleInput: { action: 'lint_blueprint', blueprintPath: '/Game/Blueprints/BP_Door', rules: ['tick_heavy', 'never_assigned'] },
    exampleOutput: {
      success: true,
      message: '1 finding(s) in BP_Door.',
      findings: [{ rule: 'never_assigned', severity: 'warning', graph: 'EventGraph', node: 'Get Key', nodeId: '8C1D0E6A4F2B4C1D9E0F1A2B3C4D5E6F', detail: 'Key is read but nothing assigns it, it is not Instance Editable nor Expose on Spawn, and it has no default: every read is None (Accessed None in Play).' }],
      counts: { never_assigned: 1 },
      findingCount: 1,
      truncated: false,
      graphsChecked: 2,
    },
  }),
];
