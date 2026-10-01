/**
 * Shared JSON-Schema property fragments for manage_level records.
 *
 * Mirrors the canonical manage-level-tool.ts input schema descriptions so
 * each record's closed input schema stays aligned with the public tool
 * contract. Property-only module; no records are constructed here.
 */
import type { JsonObject } from '../../model.js';
import { str, bool, num } from '../shared/schema-props.js';
import {
  LIGHT_AIM_TEXT, LIGHT_CLASS_TEXT, LIGHT_COLOR_WITH_PROPERTIES_TEXT, LIGHT_INTENSITY_TEXT, LIGHT_LOCATION_TEXT, LIGHT_PROPERTIES_TEXT,
  LIGHT_TYPE_TEXT,
} from '../shared/light-text.js';

type Prop = JsonObject;

const arrStr = (description: string): Prop => ({ type: 'array', items: { type: 'string' }, description });

export const P = {
  levelPath: str('Level asset path (e.g. /Game/Maps/Demo).'),
  openLevelPath: str('The level you expect to be open in the editor; the call is refused when another level is open, instead of saving the wrong one.'),
  levelName: str('Level name identifier.'),
  levelPaths: arrStr('Array of level asset paths.'),
  savePath: str('Path to save the level asset.'),
  destinationPath: str('Destination path for move/copy.'),
  sourcePath: str('Source path for import/move/copy.'),
  exportPath: str('Export file path.'),
  packagePath: str('Package path for import.'),
  subLevelPath: str('Sub-level asset path to add as a streaming child.'),
  assetPath: str('Alias of levelPath resolved by the manage_level argument normalizer.'),
  path: str('Alias of levelPath resolved by the manage_level argument normalizer.'),
  targetPath: str('Alias of destinationPath resolved by the manage_level argument normalizer.'),
  parentLevel: str('Level the sub-level belongs to; it must be the level open in the editor, or the call is refused (load it first).'),
  streamingMethod: str('Streaming method: Blueprint or AlwaysLoaded.'),
  streaming: bool('Stream the level into the open level as a sub-level (like add_sublevel) instead of replacing it.'),
  shouldBeLoaded: bool('Whether the level should be loaded.'),
  shouldBeVisible: bool('Whether the level should be visible.'),
  saveDirtyPackages: bool('Save dirty packages before the operation.'),
  newName: str('New name for the level asset.'),
  overwrite: bool('Overwrite if the destination already exists.'),
  lightClass: str(LIGHT_CLASS_TEXT),
  lightProperties: { type: 'object', description: LIGHT_PROPERTIES_TEXT, additionalProperties: true, 'x-unreal-reflection-boundary': true } as Prop,
  lightType: str(LIGHT_TYPE_TEXT),
  name: str('Light actor name.'),
  intensity: num(LIGHT_INTENSITY_TEXT),
  color: { type: 'array', items: { type: 'number' }, description: `Light color [r, g, b] or [r, g, b, a]. ${LIGHT_COLOR_WITH_PROPERTIES_TEXT}` } as Prop,
  location: { type: 'object', description: LIGHT_LOCATION_TEXT, additionalProperties: true, 'x-unreal-reflection-boundary': true } as Prop,
  rotation: { type: 'object', description: `Actor rotation {pitch, yaw, roll} or {x, y, z, w}. ${LIGHT_AIM_TEXT}`, additionalProperties: true, 'x-unreal-reflection-boundary': true } as Prop,
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
