/**
 * Focused test: every manage_sequence record stamps canonical parent metadata
 * from the 23-parent lookup (records/parent-metadata.ts) without local
 * description/category duplication.
 */
import { describe, expect, it } from 'vitest';
import { MANAGE_SEQUENCE_RECORDS } from './index.js';
import { getParentToolMetadata } from '../parent-metadata.js';

const PARENT = getParentToolMetadata('manage_sequence');

function findByAction(action: string) {
  const record = MANAGE_SEQUENCE_RECORDS.find((r) => r.legacyIds[0].action === action);
  if (!record) throw new Error(`Record not found for action: ${action}`);
  return record;
}

describe('manage_sequence carries canonical parent metadata', () => {
  it('stamps the canonical parent metadata on representative records', () => {
    for (const action of ['create', 'play', 'add_keyframe', 'queue_render', 'play_media', 'start_recording', 'play_demo']) {
      const record = findByAction(action);
      expect(record.parent).toEqual(PARENT);
      expect(record.parent.parent).toBe('manage_sequence');
      expect(record.parent.parent).toBe(record.routing.parentTool);
    }
  });

  it('matches the canonical lookup description and category (no duplication)', () => {
    const record = findByAction('play');
    expect(record.parent.description).toBe(PARENT.description);
    expect(record.parent.category).toBe(PARENT.category);
    expect(record.parent.category).toBe('utility');
  });

  it('stamps parent metadata on all 81 records', () => {
    expect(MANAGE_SEQUENCE_RECORDS).toHaveLength(81);
    for (const record of MANAGE_SEQUENCE_RECORDS) {
      expect(record.parent).toEqual(PARENT);
    }
  });
});
