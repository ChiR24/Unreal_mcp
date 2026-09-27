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
  startPoint: xyz('Start point of the navigation link (relative to the actor).'),
  endPoint: xyz('End point of the navigation link (relative to the actor).'),
  direction: pick('Link traversal direction.', ['BothWays', 'LeftToRight', 'RightToLeft']),
  snapRadius: num('Snap radius for link endpoints (default: 30).'),
  linkEnabled: bool('Whether the link is enabled.'),
  linkType: pick('Type of navigation link.', ['simple', 'smart']),

  // --- smart link behavior ---
  enabledAreaClass: str('Area class applied while the smart link is enabled.'),
  disabledAreaClass: str('Area class applied while the smart link is disabled.'),
  broadcastRadius: num('Radius for the state-change broadcast.'),
  broadcastInterval: num('Interval for the state-change broadcast (0 = single).'),
  bCreateBoxObstacle: bool('Add a box obstacle during navigation generation.'),
  obstacleOffset: xyz('Offset of the simple box obstacle.'),
  obstacleExtent: xyz('Extent of the simple box obstacle.'),
  obstacleAreaClass: str('Area class for the box obstacle.'),
} satisfies Record<string, JsonObject>;
