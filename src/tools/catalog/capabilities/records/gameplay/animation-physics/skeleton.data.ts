/**
 * Skeleton family records (41 records: the SKELETON_ACTIONS spread plus the
 * promoted hidden native routes described below).
 *
 * Grounded in animation_physics action enum (...SKELETON_ACTIONS) and native
 * HandleManageSkeleton (Plugins/.../Private/Domains/Skeleton/
 * McpAutomationBridge_SkeletonHandlers.cpp). These route through the
 * manage_skeleton bridge tool (skeleton-handlers.ts:155). All are editor
 * authoring mutations on skeleton/physics-asset assets.
 *
 * Fifteen of the hidden native skeleton routes are promoted here, authored
 * after the gateway migration and marked `post-migration` so the normalization
 * audit of the pre-gateway surface stays truthful. Five of them share a native
 * handler method with a route already declared above, so they are aliases and
 * mirror that record's schema exactly; the rest are distinct capabilities.
 * `preview_physics` remains dispositioned in ../../hidden-routes.ts only.
 *
 * Split into four contiguous sibling modules; SKELETON_RECORDS preserves the
 * original declaration order exactly.
 */
import type { CapabilityRecordSource } from '../../../index.js';
import { SKELETON_BONE_RECORDS } from './skeleton-bone.data.js';
import { SKELETON_SOCKET_WEIGHT_RECORDS } from './skeleton-socket-weight.data.js';
import { SKELETON_PHYSICS_MORPH_RECORDS } from './skeleton-physics-morph.data.js';
import { SKELETON_READ_ALIAS_RECORDS } from './skeleton-read-alias.data.js';

export {
  SKELETON_BONE_RECORDS,
  SKELETON_SOCKET_WEIGHT_RECORDS,
  SKELETON_PHYSICS_MORPH_RECORDS,
  SKELETON_READ_ALIAS_RECORDS,
};

export const SKELETON_RECORDS: readonly CapabilityRecordSource[] = [
  ...SKELETON_BONE_RECORDS,
  ...SKELETON_SOCKET_WEIGHT_RECORDS,
  ...SKELETON_PHYSICS_MORPH_RECORDS,
  ...SKELETON_READ_ALIAS_RECORDS,
];
