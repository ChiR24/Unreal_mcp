/**
 * Shared JSON-Schema property fragments for manage_level records.
 *
 * Mirrors the canonical manage-level-tool.ts input schema descriptions so
 * each record's closed input schema stays aligned with the public tool
 * contract. Property-only module; no records are constructed here.
 */
import type { JsonObject } from '../../index.js';

type Prop = JsonObject;

const str = (description: string): Prop => ({ type: 'string', description });
const bool = (description: string): Prop => ({ type: 'boolean', description });
const num = (description: string): Prop => ({ type: 'number', description });
const arrStr = (description: string): Prop => ({ type: 'array', items: { type: 'string' }, description });

export const P = {
  action: str('The action to execute on the parent tool.'),
  levelPath: str('Level asset path (e.g. /Game/Maps/Demo).'),
  levelName: str('Level name identifier.'),
  levelPaths: arrStr('Array of level asset paths.'),
  savePath: str('Path to save the level asset.'),
  destinationPath: str('Destination path for move/copy.'),
  sourcePath: str('Source path for import/move/copy.'),
  exportPath: str('Export file path.'),
  packagePath: str('Package path for import.'),
  subLevelPath: str('Sub-level asset path to add as a streaming child.'),
  sublevelPath: str('Alias of subLevelPath resolved by the manage_level argument normalizer.'),
  assetPath: str('Alias of levelPath resolved by the manage_level argument normalizer.'),
  path: str('Alias of levelPath resolved by the manage_level argument normalizer.'),
  targetPath: str('Alias of destinationPath resolved by the manage_level argument normalizer.'),
  template: str('Level template path accepted for compatibility; create_level dispatch does not apply it.'),
  parentLevel: str('Parent level path for the sub-level.'),
  parentPath: str('Parent directory path for the sub-level.'),
  streamingMethod: str('Streaming method: Blueprint or AlwaysLoaded.'),
  streaming: bool('Load the level in streaming mode.'),
  shouldBeLoaded: bool('Whether the level should be loaded.'),
  shouldBeVisible: bool('Whether the level should be visible.'),
  saveDirtyPackages: bool('Save dirty packages before the operation.'),
  newName: str('New name for the level asset.'),
  overwrite: bool('Overwrite if the destination already exists.'),
  lightType: str('Light type: Point, Directional, Spot, Sky, or Rect (short/class/lowercase accepted).'),
  name: str('Light actor name.'),
  intensity: num('Light intensity.'),
  color: { type: 'array', items: { type: 'number' }, description: 'Linear color [r, g, b] or [r, g, b, a].' } as Prop,
  location: { type: 'object', description: 'Actor world location {x, y, z}.', additionalProperties: true, 'x-unreal-reflection-boundary': true } as Prop,
  rotation: { type: 'object', description: 'Actor rotation {pitch, yaw, roll} or {x, y, z, w}.', additionalProperties: true, 'x-unreal-reflection-boundary': true } as Prop,
  quality: str('Lighting build quality: Preview, Medium, High, or Production.'),
  useWorldPartition: bool('Create the level with World Partition enabled.'),
  metadata: { type: 'object', description: 'Metadata key/value pairs to write.', additionalProperties: true, 'x-unreal-reflection-boundary': true } as Prop,
  gameMode: str('GameMode override for the level. Accepts the Blueprint asset path or its generated _C class path.'),
  killZ: num('Z height below which actors are destroyed.'),
  gravityZ: num('World gravity along Z; setting it also enables the global gravity override.'),
  timeDilation: num('Global time dilation multiplier for the level.'),
  enableWorldBoundsChecks: bool('Whether actors leaving the world bounds are culled.'),
  settingsApplied: bool('Whether any world setting was written.'),
  appliedSettings: arrStr('Names of the world settings actually written by this call.'),
} as const;
