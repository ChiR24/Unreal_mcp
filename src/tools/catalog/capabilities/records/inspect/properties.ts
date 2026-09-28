/**
 * Shared JSON-Schema property fragments for inspect records.
 *
 * Mirrors the canonical inspect-tool.ts input schema descriptions so each
 * record's closed input schema stays aligned with the public tool contract.
 * Property-only module; no records are constructed here.
 */
import type { JsonObject } from '../../model.js';
import { str, bool, num } from '../shared/schema-props.js';

type Prop = JsonObject;

const arrStr = (description: string): Prop => ({ type: 'array', items: { type: 'string' }, description });

export const P = {
  objectPath: str('Object path of the world actor or asset (e.g. /Game/Maps/Demo.Demo_PersistentLevel).'),
  runtimeObjectPath: str('Object path of the world actor or asset (e.g. /Game/Maps/Demo.Demo_PersistentLevel); while PIE runs, GameInstance, GameMode, GameState, PlayerController, PlayerPawn, PlayerState or HUD names that object of the running game.'),
  actorName: str('World actor name to inspect.'),
  actorNames: arrStr('Several world actors to act on in one call, in place of actorName; names not found are listed back.'),
  name: str('Actor name identifier (alias of actorName).'),
  propertyName: str('Property to read or write: a name, or a dotted path through structs at any depth (BodyInstance.CollisionEnabled). A bare name that is not on the object itself resolves to the one struct member carrying it. Give this or propertyPath.'),
  propertyPath: str('Same as propertyName (a name or dotted path); used when propertyName is absent. Give this or propertyName.'),
  componentName: str('Component name on the actor.'),
  className: str('Class name or /Script/ class path to inspect.'),
  classPath: str('Class asset path (alias of className).'),
  tag: str('Actor tag to match.'),
  filter: str('Runtime report filter expression.'),
  snapshotName: str('Snapshot name for create/restore.'),
  blueprintPath: str('Blueprint asset /Game path (for CDO/component inspection without spawning).'),
  detailed: bool('Return detailed property/component information.'),
  propertyNames: arrStr('Specific property names to include.'),
  componentNames: arrStr('Specific component names to include.'),
  value: { description: 'Property value to set (type depends on the target property).' } as Prop,
  structPath: str('UserDefinedStruct asset /Game path to introspect.'),
  limit: num('Maximum objects to return (default 100).'),
  offset: num('Matching objects to skip before the first one returned, for paging (default 0); hasMore says whether more follow.'),
  markDirty: bool('Default true: mark the package dirty and save an asset package. false leaves the package untouched and unsaved, so the change lives in memory only.'),
  format: str('Exporter format by file extension, default T3D (the text form of the object); fails with EXPORT_FAILED when no exporter for that format handles the object.'),
  outputPath: str('File to also write the export to, inside the project (relative to it, or absolute under it), e.g. Saved/Exports/Actor.t3d. A path outside the project is refused with INVALID_PATH.'),
} as const;
