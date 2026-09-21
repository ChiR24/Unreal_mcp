/**
 * animation_physics capability record catalog: 99 authored
 * CapabilityRecordSource entries -- 58 animation/physics authoring records
 * (authoring-1/2/3: 20 + 23 + 16) plus 41 skeleton records (skeleton-bone 7,
 * skeleton-socket-weight 9, skeleton-physics-morph 11, skeleton-read-alias 14)
 * -- folded by ANIMATION_PHYSICS_FOLDS into ANIMATION_PHYSICS_SOURCES.
 * Ordered to match the animation_physics action enum in
 * animation-physics-tool.ts. Grounded in the TS handler bodies, the native
 * Animation/Physics/Skeleton domains, and the route-disposition ledger.
 *
 * The 16 hidden native skeleton routes and 4 hidden GAS / 3 hidden AI routes
 * are dispositioned in ../hidden-routes.ts, NOT part of this authored set.
 */
import type { CapabilityRecordSource } from '../../../index.js';

import { ANIM_AUTHORED_1 } from './authoring-1.data.js';
import { ANIM_AUTHORED_2 } from './authoring-2.data.js';
import { ANIM_AUTHORED_3 } from './authoring-3.data.js';
import { SKELETON_RECORDS } from './skeleton.data.js';
import { applyFolds } from '../../shared/fold.js';
import { ANIMATION_PHYSICS_FOLDS } from '../../folds/animation-physics.folds.js';

/** The authored records before folding; per-action contract tests pin these. */
export const ANIMATION_PHYSICS_UNFOLDED_SOURCES: readonly CapabilityRecordSource[] = [
  ...ANIM_AUTHORED_1,
  ...ANIM_AUTHORED_2,
  ...ANIM_AUTHORED_3,
  ...SKELETON_RECORDS,
];

export const ANIMATION_PHYSICS_SOURCES: readonly CapabilityRecordSource[] = applyFolds(ANIMATION_PHYSICS_UNFOLDED_SOURCES, ANIMATION_PHYSICS_FOLDS, 'animation_physics');
