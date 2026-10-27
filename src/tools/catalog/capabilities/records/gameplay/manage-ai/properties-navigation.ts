/**
 * AI-local JSON-schema property fragments for the navigation route.
 */
import type { JsonObject } from '../../../model.js';
import { xyz } from './properties.js';
import { str, num, bool } from '../../shared/schema-props.js';

const pick = (description: string, values: readonly string[]): JsonObject =>
  ({ type: 'string', enum: [...values], description });

export const NAV = {
  // --- agent properties (set_nav_agent_properties) ---
  agentRadius: num('Navigation agent radius (default: 35).'),
  agentHeight: num('Navigation agent height (default: 144).'),
  agentStepHeight: num('Maximum step height the agent can climb (default: 35).'),
  agentMaxSlope: num('Maximum slope angle in degrees (default: 44).'),

  // --- navmesh generation (configure_nav_mesh_settings) ---
  cellSize: num('NavMesh cell size (default: 19).'),
  cellHeight: num('NavMesh cell height (default: 10).'),
  tileSizeUU: num('NavMesh tile size in Unreal units (default: 1000).'),
  minRegionArea: num('Minimum region area to keep.'),
  mergeRegionSize: num('Region merge threshold.'),
  maxSimplificationError: num('Edge simplification error.'),

  // --- nav areas / modifiers ---
  areaClass: str('Navigation area class path.'),
  areaCost: num('Pathfinding cost multiplier for the area (1.0 = normal).'),
  failsafeExtent: xyz('Failsafe extent used when the actor has no collision.'),

  // --- nav links ---
  location: xyz('World location for the spawned link actor.'),
  startPoint: xyz('Start of the link, relative to the link actor (its location and rotation).'),
  endPoint: xyz('End of the link, relative to the link actor (its location and rotation).'),
  direction: pick('Which way agents may cross: BothWays (default), LeftToRight (start to end) or RightToLeft.', ['BothWays', 'LeftToRight', 'RightToLeft']),
  snapRadius: num('How far from each end of the point link the navmesh is looked for (default 30).'),
  linkEnabled: bool('Switch the smart link on (true) or off.'),
  linkType: pick('simple (the point link of the proxy) or smart (its smart link, which can switch on and off).', ['simple', 'smart']),

  // --- smart link behavior ---
  enabledAreaClass: str('Navigation area class the smart link marks while on, e.g. /Script/NavigationSystem.NavArea_Default.'),
  disabledAreaClass: str('Navigation area class the smart link marks while off, e.g. /Script/NavigationSystem.NavArea_Null (impassable).'),
  broadcastRadius: num('Radius, in units, of the agents told when the smart link switches; the engine default 0 tells none.'),
  broadcastInterval: num('Seconds between repeats of that notice; 0 sends it once.'),
  bCreateBoxObstacle: bool('true sets the box obstacle of the smart link, which marks the navmesh with obstacleAreaClass.'),
  obstacleOffset: xyz('Offset of the box obstacle from the link actor.'),
  obstacleExtent: xyz('Half size of the box obstacle (default 100 on each axis).'),
  obstacleAreaClass: str('Area the box obstacle marks (default /Script/NavigationSystem.NavArea_Null, impassable).'),
} satisfies Record<string, JsonObject>;
