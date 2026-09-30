/**
 * Attachment and advanced records: attach/attach_actor, detach/detach_actor,
 * set_blueprint_variables, create_snapshot, set_actor_collision,
 * call_actor_function.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildCoreRecord } from '../core/builder.js';
import { DOMAIN, P } from './properties.js';

const FAMILY_ATTACH = 'attachment';
const FAMILY_BLUEPRINT = 'blueprint';
const FAMILY_SNAPSHOT = 'snapshot';
const FAMILY_COLLISION = 'collision';
const FAMILY_FUNCTION = 'function';

export const ADVANCED_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'control_actor',
    action: 'attach',
    domain: DOMAIN,
    family: FAMILY_ATTACH,
    topics: ['attach actor', 'parent actor', 'attach to actor', 'child actor', 'attach to socket', 'set actor parent'],
    summary: 'Attach a child actor to a parent actor in the scene hierarchy.',
    whenToUse: ['An actor must follow another actor transform.'],
    whenNotToUse: ['The actor should remain independent (use detach).'],
    inputProps: { childActor: P.childActor, parentActor: P.parentActor },
    required: ['childActor', 'parentActor'],
    effect: 'write',
    costLatency: 'interactive',
    exampleInput: { action: 'attach', childActor: 'Sword', parentActor: 'Knight' },
  }),
  buildCoreRecord({
    parentTool: 'control_actor',
    action: 'detach',
    domain: DOMAIN,
    family: FAMILY_ATTACH,
    summary: 'Detach a child actor from its current parent.',
    whenToUse: ['An actor must be released from attachment.'],
    whenNotToUse: ['The actor is not attached (no-op).'],
    inputProps: { actorName: P.actorName },
    required: ['actorName'],
    effect: 'write',
    behavior: { idempotency: 'idempotent' },
    costLatency: 'interactive',
    exampleInput: { action: 'detach', actorName: 'Sword' },
  }),
  buildCoreRecord({
    parentTool: 'control_actor',
    action: 'set_blueprint_variables',
    topics: ['actor variable'],
    domain: DOMAIN,
    family: FAMILY_BLUEPRINT,
    summary: 'Set one or more Blueprint instance variables on a spawned actor, or on many actors in one call with actors.',
    whenToUse: ['Instance variables on a Blueprint actor must be configured.', 'Several actors each need their own variable values (actors).'],
    whenNotToUse: ['The actor is not a Blueprint instance (variables are undefined).'],
    inputProps: {
      actorName: P.actorName, variables: P.variables,
      actors: {
        type: 'array',
        items: { type: 'object', properties: { actorName: P.actorName, variables: P.variables }, required: ['actorName', 'variables'], additionalProperties: false },
        description: 'Many actors in one call, each {actorName, variables} with its own values; every actor is reported, and the call fails naming any that did not take all of its variables.',
      },
    },
    required: [],
    requiredOneOf: ['actorName', 'actors'],
    effect: 'write',
    behavior: { idempotency: 'idempotent' },
    costLatency: 'interactive',
    exampleInput: { action: 'set_blueprint_variables', actorName: 'Lamp1', variables: { Brightness: 2.0 } },
  }),
  buildCoreRecord({
    parentTool: 'control_actor',
    action: 'create_snapshot',
    domain: DOMAIN,
    family: FAMILY_SNAPSHOT,
    summary: 'Capture a named snapshot of actor state for later comparison.',
    whenToUse: ['Actor state must be recorded before a mutation.'],
    whenNotToUse: ['No later comparison or restore is planned.'],
    inputProps: { actorName: P.actorName, snapshotName: P.snapshotName },
    required: ['actorName', 'snapshotName'],
    effect: 'read',
    exampleInput: { action: 'create_snapshot', actorName: 'Cube1', snapshotName: 'before_move' },
  }),
  buildCoreRecord({
    parentTool: 'control_actor',
    action: 'set_actor_collision',
    domain: DOMAIN,
    family: FAMILY_COLLISION,
    summary: 'Enable or disable collision on an actor, or on many actors at once with actorNames; normalizes to set_collision.',
    whenToUse: ['Actor collision must be toggled.', 'Many actors must have collision toggled together (actorNames does them in one call).'],
    whenNotToUse: ['Per-component collision is required (use set_component_property).'],
    inputProps: {
      actorName: P.actorName,
      actorNames: { type: 'array', items: { type: 'string' }, description: 'Several actors to toggle in one call, in place of actorName; names not found are listed back under missing.' },
      collisionEnabled: P.collisionEnabled,
    },
    required: [],
    requiredOneOf: ['actorName', 'actorNames'],
    effect: 'write',
    behavior: { idempotency: 'idempotent' },
    exampleInput: { action: 'set_actor_collision', actorName: 'Cube1', collisionEnabled: true },
  }),
  buildCoreRecord({
    parentTool: 'control_actor',
    action: 'call_actor_function',
    domain: DOMAIN,
    family: FAMILY_FUNCTION,
    summary: 'Call a named function on an actor with optional arguments; normalizes to call_function.',
    whenToUse: ['A Blueprint or native function on an actor must be invoked.'],
    whenNotToUse: ['The function has side effects that cannot be undone.'],
    inputProps: { actorName: P.actorName, functionName: P.functionName, arguments: P.arguments },
    required: ['actorName', 'functionName'],
    outputProps: { value: P.value },
    outputRequired: [],
    effect: 'destructive',
    policyOverride: { consent: 'elevated' },
    costLatency: 'interactive',
    exampleInput: { action: 'call_actor_function', actorName: 'Lamp1', functionName: 'ToggleLight' },
    exampleOutput: { success: true, message: 'Called ToggleLight on Lamp1', value: true },
  }),
];
