/**
 * Routing contract for the promoted Skeleton routes.
 *
 * Fifteen skeleton operations are implemented under `Private/Domains/Skeleton/`
 * and dispatchable there, but nothing upstream names them. `animation_physics`
 * forwards a sub-action to `HandleManageSkeleton` only when `IsSkeletonAction()`
 * accepts it, and the TypeScript registry reaches `handleSkeletonTools` only
 * when `skeletonActionSet` holds it. An action missing from either list is
 * answered by the wrong handler rather than refused, so both are pinned here.
 */

import { readFileSync } from 'node:fs';
import { describe, expect, it } from 'vitest';
import { skeletonActionSet } from '../../../src/tools/orchestration/consolidated-routing.js';

const PROMOTED_SKELETON_ACTIONS = [
  'add_socket',
  'delete_morph_target',
  'delete_socket',
  'delete_virtual_bone',
  'get_bone_transform',
  'get_physics_asset_info',
  'list_morph_targets',
  'list_virtual_bones',
  'modify_physics_body',
  'modify_socket',
  'remove_physics_body',
  'remove_socket',
  'set_morph_target_value',
  'set_physics_asset',
  'set_physics_constraint',
] as const;

const NATIVE_ROUTING_HEADER =
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Routing/'
  + 'McpConsolidatedActionRoutingAnimationSystem.h';

/**
 * The header declares several action arrays, so a bare substring search would
 * pass on a name that belongs to a sibling list. Only `Skeleton()` gates
 * `IsSkeletonAction`, so the assertion reads that array and nothing else.
 */
function skeletonArrayBody(header: string): string {
  const start = header.indexOf('inline const TArray<FString>& Skeleton()');
  if (start < 0) throw new Error('Skeleton() is missing from the native routing header');
  const end = header.indexOf('\n}', start);
  if (end < 0) throw new Error('the Skeleton() body is unterminated');
  return header.slice(start, end);
}

describe('promoted skeleton routes are named on both routing surfaces', () => {
  it('the TypeScript registry routes each promoted action to the skeleton handler', () => {
    const missing = PROMOTED_SKELETON_ACTIONS.filter((action) => !skeletonActionSet.has(action));

    expect(
      missing,
      'absent from SKELETON_ACTIONS, so consolidated-handler-registration never reaches '
      + 'handleSkeletonTools and answers from the animation_physics handler instead',
    ).toEqual([]);
  });

  it('the native Skeleton() array names each promoted action', () => {
    const body = skeletonArrayBody(readFileSync(NATIVE_ROUTING_HEADER, 'utf8'));
    const missing = PROMOTED_SKELETON_ACTIONS.filter(
      (action) => !body.includes(`TEXT("${action}")`),
    );

    expect(
      missing,
      'absent from Skeleton(), so IsSkeletonAction() is false and animation_physics '
      + 'never forwards to HandleManageSkeleton',
    ).toEqual([]);
  });
});
