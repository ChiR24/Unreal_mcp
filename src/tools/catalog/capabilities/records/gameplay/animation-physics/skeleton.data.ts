/**
 * Skeleton family records (41 records: the SKELETON_ACTIONS spread plus the
 * promoted hidden native routes described below).
 */
import type { CapabilityRecordSource } from '../../../model.js';
import { SKELETON_BONE_RECORDS } from './skeleton-bone.data.js';
import { SKELETON_SOCKET_WEIGHT_RECORDS } from './skeleton-socket-weight.data.js';
import { SKELETON_PHYSICS_MORPH_RECORDS } from './skeleton-physics-morph.data.js';
import { SKELETON_READ_ALIAS_RECORDS } from './skeleton-read-alias.data.js';

export const SKELETON_RECORDS: readonly CapabilityRecordSource[] = [
  ...SKELETON_BONE_RECORDS,
  ...SKELETON_SOCKET_WEIGHT_RECORDS,
  ...SKELETON_PHYSICS_MORPH_RECORDS,
  ...SKELETON_READ_ALIAS_RECORDS,
];
