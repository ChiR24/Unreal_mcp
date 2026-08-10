/**
 * animation_physics capability record catalog: exactly 87 canonical records
 * (58 direct animation/physics actions + 29 SKELETON_ACTIONS spread),
 * ordered to match the animation_physics action enum in
 * animation-physics-tool.ts. Grounded in the TS handler bodies, the native
 * Animation/Physics/Skeleton domains, and the route-disposition ledger.
 *
 * The 16 hidden native skeleton routes and 4 hidden GAS / 3 hidden AI routes
 * are dispositioned in ../hidden-routes.ts, NOT part of this 87-record set.
 */
import type { CapabilityRecordSource } from '../../../index.js';

import { ANIM_AUTHORED_1 } from './authoring-1.data.js';
import { ANIM_AUTHORED_2 } from './authoring-2.data.js';
import { ANIM_AUTHORED_3 } from './authoring-3.data.js';
import { SKELETON_RECORDS } from './skeleton.data.js';

export const ANIMATION_PHYSICS_SOURCES: readonly CapabilityRecordSource[] = [
  ...ANIM_AUTHORED_1,
  ...ANIM_AUTHORED_2,
  ...ANIM_AUTHORED_3,
  ...SKELETON_RECORDS,
];
