/**
 * animation_physics capability record catalog: the authoring shards
 * (authoring-1/2/3) plus the skeleton shards, folded by
 * ANIMATION_PHYSICS_FOLDS into ANIMATION_PHYSICS_SOURCES.
 */
import type { CapabilityRecordSource } from '../../../model.js';

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
