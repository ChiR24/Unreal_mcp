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
    description: 'Component property key-value pairs. Each key is a name or dotted path, as propertyName takes it (BodyInstance.CollisionEnabled works). Collision goes through the component setters: CollisionProfileName (a preset), CollisionEnabled, CollisionResponses ({"Pawn": "Ignore"}), PhysMaterialOverride (a physical material path, None to clear).',
    additionalProperties: true,
    'x-unreal-reflection-boundary': true,
  },
  value: { description: 'Property value (any type).' },
  visible: bool('Desired visibility state.'),
  newName: str('New name for the duplicate or renamed actor.'),
  tag: str('Actor tag: one entry in the Tags list of an actor (not a Gameplay Tag) to add, remove, find or delete by.'),
  variables: {
    type: 'object',
    description: 'Blueprint variable name to value map. An actor variable, or an array of them, takes each actor by its label or name as list shows it ("Door_1", ["Fire_1", "Fire_2"]) or by its object path.',
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
        staticMesh: str('Static mesh asset path (static mesh components).'),
        materials: { type: 'array', items: { type: 'string' }, description: 'Material drawn in each slot, overrides included (mesh components).' },
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

// Where an actor ended up after a spawn or a move, so a mesh whose pivot is its bounds centre does not sit half under the
// floor behind a bare success, and a wall run up to the ceiling's top does not flicker behind one. Each fact is present
// only when the check could measure it.
export const PLACEMENT_FACTS = {
  groundZ: num('Z of the solid surface under the actor; absent when nothing is below it.'),
  groundClearance: num('How far the bottom of the actor\'s bounds sits above that surface; negative means it is sunk into it.'),
  placementWarning: str('Present when the placement looks wrong: it intersects other actors, is sunk below the surface under it (the bottom of its bounds is under it, wherever its pivot is; suggestedLocation rests it), floats more than 50 units above it, has nothing below it (an actor held up from the side is mounted, never floating: see mountedOn; a floor or platform, four times wider than tall, is ground itself and never reported for it), or has a face in the same plane as another actor\'s, pointing the same way, which flickers where they overlap (z-fighting: see coplanarFaces). A water body is a surface, not a solid: an actor partly under water (a buoy, a hull, a wading pawn) is neither intersecting nor sunk, and groundZ is the water level. Tag the actor mcp.placement.ok when the placement is deliberate and it is never reported; the tag does not hide a shared plane, which always flickers.'),
  mountedOn: str('The actor this one hangs on from the side (a window band flush on a hall, an awning on a facade): its bounds touch or overlap that actor\'s vertical face, so it is held up and is not reported floating or without a surface below. Present only then.'),
  overlappingActors: {
    type: 'array',
    items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true },
    'x-unreal-reflection-boundary': true,
    description: 'The actors it intersects (up to 8): actorName, actorClass, penetrationDepth.',
  },
  coplanarFaces: {
    type: 'array',
    items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true },
    'x-unreal-reflection-boundary': true,
    description: 'Its faces that lie in the same plane as a face of another actor and point the same way, so the overlap flickers between the two (z-fighting): direction (+X, -Z, ...), component, otherActor, otherComponent, overlapU and overlapV (the overlap\'s size in units), largest first. Move one about 2 units apart along direction, or stop a wall at a ceiling\'s underside; control_actor fix_coplanar nudges every pair. Present only when there is one.',
  },
  suggestedLocation: {
    type: 'object',
    additionalProperties: true,
    'x-unreal-reflection-boundary': true,
    description: 'The actor location (x, y, z) that rests the actor on the surface under it: its own location raised by the depth it is sunk, whatever its pivot. Present only when it is sunk.',
  },
} as const;

export const DOMAIN = 'actor' as const;


