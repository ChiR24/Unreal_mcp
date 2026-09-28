/**
 * Shared JSON-schema property fragments for control_actor capability records.
 *
 * Private to the control-actor domain. These are plain JsonObject fragments
 * consumed by buildCoreRecord input/output props; they do not touch the shared
 * capability model, schema, generator, or any aggregate code.
 */
import { bool, num, str, vec3 } from '../shared/schema-props.js';

export const P = {
  actorName: str('Target actor name in the current level.'),
  pieActorName: str('Target actor name in the current level; while PIE runs, PlayerPawn names the player\'s pawn (PlayerController, GameMode, GameState, PlayerState and HUD work too).'),
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
  propertyName: str('Component property to read or write: a name (Intensity), or a dotted path through structs at any depth (BodyInstance.CollisionEnabled, LightmassSettings.bShadowIndirectOnly). A bare name that is not on the component itself resolves to the one struct member carrying it (CollisionEnabled reads and writes BodyInstance.CollisionEnabled); a name several structs carry is refused with the full paths to choose from.'),
  propertyPath: str('Same as propertyName (a name or a dotted path such as BodyInstance.CollisionEnabled); used when propertyName is absent.'),
  templateBlueprintPath: str('Canonical /Game Blueprint asset path. Reads the component template on the Blueprint CDO instead of a live actor, so a Blueprint with no instance in the level can still be inspected. Supply this or actorName.'),
  properties: {
    type: 'object',
    description: 'Component property key-value pairs. Each key is a name or dotted path, as propertyName takes it (BodyInstance.CollisionEnabled works).',
    additionalProperties: true,
    'x-unreal-reflection-boundary': true,
  },
  value: { description: 'Property value (any type).' },
  visible: bool('Desired visibility state.'),
  newName: str('New name for the duplicate or renamed actor.'),
  tag: str('Actor tag: one entry in the Tags list of an actor (not a Gameplay Tag) to add, remove, find or delete by.'),
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
  totalCount: num('Listable actors matching the filter, before the limit is applied.'),
  excludedCount: num('Actors present in the world but never listable here: templates, transient actors, the builder brush and WorldSettings. Explains why this total is below the actorCount get_editor_state reports for the same world.'),
  isPieWorld: bool('Whether the list was produced while a Play-In-Editor (PIE) session is active.'),
  worldName: str('Name of the active world (or PIE world) the actors were listed from.'),
} as const;

export const DOMAIN = 'actor' as const;


