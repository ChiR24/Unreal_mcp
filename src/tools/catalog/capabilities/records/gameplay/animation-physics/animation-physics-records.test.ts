/**
 * Structural contract for the animation_physics record family.
 *
 * It is the largest authored family in the catalog (100 sources across four
 * data modules, folded to 28) and it was the only gameplay sub-family with no
 * test at all. Its index.ts documents the per-module breakdown in prose --
 * "19 + 23 + 17" and "skeleton-bone 7, skeleton-socket-weight 9,
 * skeleton-physics-morph 11, skeleton-read-alias 14" -- and nothing checked
 * those numbers, so the file could describe a shape the tree had left.
 *
 * Deliberately NOT a per-action contract table: 100 rows transcribed from the
 * records would be a second copy of them, and a second copy answers with last
 * month's rules. These are the facts the records cannot restate about
 * themselves -- how many there are, that no action is declared twice across
 * four independently edited modules, and that folding loses none of them.
 */
import { describe, expect, it } from 'vitest';

import { ANIM_AUTHORED_1 } from './authoring-1.data.js';
import { ANIM_AUTHORED_2 } from './authoring-2.data.js';
import { ANIM_AUTHORED_3 } from './authoring-3.data.js';
import { SKELETON_BONE_RECORDS } from './skeleton-bone.data.js';
import { SKELETON_PHYSICS_MORPH_RECORDS } from './skeleton-physics-morph.data.js';
import { SKELETON_READ_ALIAS_RECORDS } from './skeleton-read-alias.data.js';
import { SKELETON_SOCKET_WEIGHT_RECORDS } from './skeleton-socket-weight.data.js';
import { SKELETON_RECORDS } from './skeleton.data.js';
import {
  ANIMATION_PHYSICS_SOURCES,
  ANIMATION_PHYSICS_UNFOLDED_SOURCES,
} from './index.js';
import { createCapabilityRecord } from '../../../parser.js';

const AUTHORED_TOTAL = 100;
const FOLDED_TOTAL = 28;

describe('animation_physics record family', () => {
  it('holds exactly the per-module counts its index documents', () => {
    expect(ANIM_AUTHORED_1).toHaveLength(19);
    expect(ANIM_AUTHORED_2).toHaveLength(23);
    expect(ANIM_AUTHORED_3).toHaveLength(17);
    expect(SKELETON_BONE_RECORDS).toHaveLength(7);
    expect(SKELETON_SOCKET_WEIGHT_RECORDS).toHaveLength(9);
    expect(SKELETON_PHYSICS_MORPH_RECORDS).toHaveLength(11);
    expect(SKELETON_READ_ALIAS_RECORDS).toHaveLength(14);
    expect(SKELETON_RECORDS).toHaveLength(41);

    expect(ANIMATION_PHYSICS_UNFOLDED_SOURCES).toHaveLength(AUTHORED_TOTAL);
    expect(ANIMATION_PHYSICS_SOURCES).toHaveLength(FOLDED_TOTAL);
  });

  it('declares every action exactly once across the four data modules', () => {
    // The modules are edited independently, so a copy-pasted record is the
    // realistic failure -- and a duplicate id would otherwise surface far away,
    // as an aggregate count mismatch with no pointer to the offender.
    const seen = new Map<string, number>();
    for (const source of ANIMATION_PHYSICS_UNFOLDED_SOURCES) {
      const id = String(source.id);
      seen.set(id, (seen.get(id) ?? 0) + 1);
    }

    expect([...seen.entries()].filter(([, count]) => count > 1)).toEqual([]);
    expect(seen.size).toBe(AUTHORED_TOTAL);
  });

  it('routes every authored record to animation_physics', () => {
    const offenders: string[] = [];
    for (const source of ANIMATION_PHYSICS_UNFOLDED_SOURCES) {
      const record = createCapabilityRecord(source);
      if (String(record.routing.parentTool) !== 'animation_physics') {
        offenders.push(`${String(record.id)} -> ${String(record.routing.parentTool)}`);
      }
    }

    expect(offenders).toEqual([]);
  });

  it('keeps every authored action callable after folding', () => {
    // Folding is only safe because each collapsed action survives as one of the
    // surviving record's legacy pairs. 100 authored actions must still be
    // reachable through the 28 folded records.
    const reachable = new Set(
      ANIMATION_PHYSICS_SOURCES.flatMap((source) =>
        createCapabilityRecord(source).legacyIds.map((pair) => String(pair.action)),
      ),
    );

    const lost: string[] = [];
    for (const source of ANIMATION_PHYSICS_UNFOLDED_SOURCES) {
      const action = String(createCapabilityRecord(source).routing.dispatchAction);
      if (!reachable.has(action)) lost.push(action);
    }

    expect(lost).toEqual([]);
  });
});
