import { AI_GAS_ROUTE_DISPOSITIONS } from '../../normalization/routedispositions-ai.data.js';
import { ANIMATION_SKELETON_ROUTE_DISPOSITIONS } from '../../normalization/routedispositions-animation.data.js';
import type { RawRouteDisposition } from '../../normalization/routedispositions-paths.js';

export type GameplayHiddenRouteDisposition = {
  readonly key: string;
  readonly route: string;
  readonly disposition: 'promote' | 'map' | 'remove';
  readonly rationale: string;
  readonly evidence: {
    readonly source: string;
    readonly symbol: string;
    readonly tool: string;
  };
};

const SKELETON_KEYS = [
  'route:skeleton:add_socket', 'route:skeleton:modify_socket',
  'route:skeleton:modify_physics_body', 'route:skeleton:set_physics_asset',
  'route:skeleton:remove_physics_body', 'route:skeleton:get_physics_asset_info',
  'route:skeleton:list_morph_targets', 'route:skeleton:delete_morph_target',
  'route:skeleton:delete_socket', 'route:skeleton:remove_socket',
  'route:skeleton:get_bone_transform', 'route:skeleton:list_virtual_bones',
  'route:skeleton:delete_virtual_bone', 'route:skeleton:set_physics_constraint',
  'route:skeleton:set_morph_target_value', 'route:skeleton:preview_physics',
] as const;

const GAS_KEYS = [
  'route:gas:create_ability_set', 'route:gas:add_ability',
  'route:gas:grant_ability', 'route:gas:create_execution_calculation',
] as const;

const AI_KEYS = [
  'route:ai:create_nav_modifier', 'route:ai:set_ai_movement',
  'route:ai:set_ai_perception',
] as const;

function selectRoutes(
  records: readonly RawRouteDisposition[],
  keys: readonly string[],
): readonly GameplayHiddenRouteDisposition[] {
  return Object.freeze(keys.map((key) => {
    const record = records.find((candidate) => candidate.key === key);
    if (record === undefined) throw new TypeError(`Missing gameplay hidden-route disposition: ${key}`);
    return Object.freeze({
      key: record.key,
      route: record.route,
      disposition: record.disposition,
      rationale: record.rationale,
      evidence: Object.freeze({
        source: record.evidenceSource,
        symbol: record.evidenceSymbol,
        tool: record.evidenceTool,
      }),
    });
  }));
}

export const GAMEPLAY_HIDDEN_ROUTE_DISPOSITIONS = Object.freeze({
  skeleton: selectRoutes(ANIMATION_SKELETON_ROUTE_DISPOSITIONS, SKELETON_KEYS),
  gas: selectRoutes(AI_GAS_ROUTE_DISPOSITIONS, GAS_KEYS),
  ai: selectRoutes(AI_GAS_ROUTE_DISPOSITIONS, AI_KEYS),
});
