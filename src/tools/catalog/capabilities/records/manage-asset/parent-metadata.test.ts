/**
 * Focused test: every manage_asset record stamps canonical parent metadata
 * from the 23-parent lookup (records/parent-metadata.ts) without local
 * description/category duplication.
 */
import { describe, expect, it } from 'vitest';
import { MANAGE_ASSET_RECORDS } from './index.js';
import { getParentToolMetadata } from '../parent-metadata.js';

const PARENT = getParentToolMetadata('manage_asset');

function findByAction(action: string) {
  const record = MANAGE_ASSET_RECORDS.find((r) => r.legacyIds[0].action === action);
  if (!record) throw new Error(`Record not found for action: ${action}`);
  return record;
}

describe('manage_asset carries canonical parent metadata', () => {
  it('stamps the canonical parent metadata on representative records', () => {
    for (const action of ['list', 'import', 'create_material', 'create_struct', 'create_data_table', 'create_enum']) {
      const record = findByAction(action);
      expect(record.parent).toEqual(PARENT);
      expect(record.parent.parent).toBe('manage_asset');
      expect(record.parent.parent).toBe(record.routing.parentTool);
    }
  });

  it('matches the canonical lookup description and category (no duplication)', () => {
    const record = findByAction('import');
    expect(record.parent.description).toBe(PARENT.description);
    expect(record.parent.category).toBe(PARENT.category);
    expect(record.parent.category).toBe('core');
  });

  it('stamps parent metadata on all 158 records', () => {
    expect(MANAGE_ASSET_RECORDS).toHaveLength(158);
    for (const record of MANAGE_ASSET_RECORDS) {
      expect(record.parent).toEqual(PARENT);
    }
  });
});
