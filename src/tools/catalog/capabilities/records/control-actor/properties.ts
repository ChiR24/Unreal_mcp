/**
 * Shared JSON-schema property fragments for control_actor capability records.
 *
 * Private to the control-actor domain. These are plain JsonObject fragments
 * consumed by buildCoreRecord input/output props; they do not touch the shared
 * capability model, schema, generator, or any aggregate code.
 */
import type { JsonObject } from '../../index.js';

const str = (desc: string): JsonObject => ({ type: 'string', description: desc });
const num = (desc: string): JsonObject => ({ type: 'number', description: desc });
const bool = (desc: string): JsonObject => ({ type: 'boolean', description: desc });
const vec3 = (desc: string): JsonObject => ({
  type: 'array',
  items: { type: 'number' },
  minItems: 3,
  maxItems: 3,
  description: desc,
});

export const P = {
  action: str('The control_actor action to execute.'),
  actorName: str('Target actor name in the current level.'),
  actorNames: {
    type: 'array',
    items: str('Actor name.'),
    description: 'Actor names to act on (batch delete).',
  },
  classPath: str('Unreal class path (e.g. /Script/Engine.PointLight) for the actor to spawn.'),
  actorClass: str('Alias of classPath accepted by the spawn handler (normalizeArgs alias).'),
  meshPath: str('Canonical /Game mesh asset path to assign on spawn.'),
  materialPath: str('Canonical /Game material asset path to apply.'),
  materialSlot: num('Material slot/index to override (0-based).'),
  materialIndex: num('Alias of materialSlot accepted by the material handlers (normalizeArgs alias).'),
  allComponents: bool('When true, apply the material to all mesh components.'),
  blueprintPath: str('Canonical /Game Blueprint asset path to spawn from.'),
  location: vec3('World or relative location as [x, y, z].'),
  rotation: vec3('Rotation as [pitch, yaw, roll] in degrees.'),
  scale: vec3('Scale as [x, y, z].'),
  force: vec3('Force vector to apply as [x, y, z].'),
  offset: vec3('Spawn/duplicate offset as [x, y, z].'),
  componentType: str('Component class to add.'),
  componentName: str('Target component name on the actor.'),
  propertyName: str('Component property name to read or write.'),
  properties: {
    type: 'object',
    description: 'Component property key-value pairs.',
    additionalProperties: true,
    'x-unreal-reflection-boundary': true,
  },
  value: { description: 'Property value (any type).' },
  visible: bool('Desired visibility state.'),
  newName: str('New name for the duplicate or renamed actor.'),
  tag: str('Gameplay tag string to add, remove, or find.'),
  variables: {
    type: 'object',
    description: 'Blueprint variable name to value map.',
    additionalProperties: true,
    'x-unreal-reflection-boundary': true,
  },
  snapshotName: str('Name for the actor snapshot.'),
  className: str('Unreal class name or path to find actors by class.'),
  collisionEnabled: bool('Desired collision enabled state.'),
  functionName: str('Actor function name to call.'),
  arguments: { description: 'Function arguments (any type).' },
  childActor: str('Child actor name to attach.'),
  parentActor: str('Parent actor name to attach to.'),
  limit: num('Maximum number of actors to return in a list.'),
  filter: str('Optional name substring filter for list.'),
  name: str('Actor name or search query.'),
  actors: {
    type: 'array',
    items: {
      type: 'object',
      properties: {
        label: str('Actor label.'),
        name: str('Actor name.'),
        path: str('Actor path.'),
        class: str('Actor class.'),
      },
      additionalProperties: true,
      'x-unreal-reflection-boundary': true,
    },
    description: 'Matched actors.',
  },
  components: {
    type: 'array',
    items: {
      type: 'object',
      properties: {
        name: str('Component name.'),
        class: str('Component class.'),
        relativeLocation: vec3('Relative location [x, y, z].'),
        relativeRotation: vec3('Relative rotation [pitch, yaw, roll].'),
        relativeScale: vec3('Relative scale [x, y, z].'),
      },
      additionalProperties: true,
      'x-unreal-reflection-boundary': true,
    },
    description: 'Actor components.',
  },
  count: num('Number of actors returned.'),
  totalCount: num('Total actors matched before pagination.'),
  isPieWorld: bool('Whether the list was produced while a Play-In-Editor (PIE) session is active.'),
  worldName: str('Name of the active world (or PIE world) the actors were listed from.'),
  actorPath: str('Resolved actor path.'),
} as const;

export const DOMAIN = 'actor' as const;

export const CANONICAL_NR = 'Distinct control_actor operation with dedicated TS handler and native dispatch.';
export function aliasNr(canonical: string): string {
  return `Alias of control_actor.${canonical}; normalizeActorAction maps this action to ${canonical} before dispatch.`;
}

/**
 * The whole normalization verdict for a dispatch alias, so the target is named
 * once. Declaring only the rationale left the class at the `C` default, which
 * said "distinct capability" about a record the same line called an alias.
 */
export function actorAlias(canonical: string) {
  return {
    normalizationClass: 'B_ALIAS',
    normalizationDisposition: 'alias',
    normalizationRationale: aliasNr(canonical),
    normalizationAliasOf: `control_actor.${canonical}`,
  } as const;
}

export function internalDispatchNr(action: string, dispatchTarget: string): string {
  return `Distinct control_actor operation (canonical record cap:control_actor:${action}). normalizeActorAction maps this action to ${dispatchTarget} as an internal dispatch alias; ${dispatchTarget} is not a separate canonical record in the normalization inventory.`;
}
