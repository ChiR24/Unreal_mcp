/**
 * Shared JSON-Schema property fragments for inspect records.
 *
 * Mirrors the canonical inspect-tool.ts input schema descriptions so each
 * record's closed input schema stays aligned with the public tool contract.
 * Property-only module; no records are constructed here.
 */
import type { JsonObject } from '../../index.js';

type Prop = JsonObject;

const str = (description: string): Prop => ({ type: 'string', description });
const bool = (description: string): Prop => ({ type: 'boolean', description });
const arrStr = (description: string): Prop => ({ type: 'array', items: { type: 'string' }, description });

export const P = {
  action: str('The action to execute on the parent tool.'),
  objectPath: str('Object path of the world actor or asset (e.g. /Game/Maps/Demo.Demo_PersistentLevel).'),
  actorName: str('World actor name to inspect.'),
  name: str('Actor name identifier (alias of actorName).'),
  propertyName: str('Property name to read or write.'),
  propertyPath: str('Property path (alias of propertyName).'),
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
} as const;
