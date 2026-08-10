/**
 * Focused test: every manage_blueprint record stamps canonical parent metadata
 * from the 23-parent lookup (records/parent-metadata.ts) without local
 * description/category duplication.
 */
import { describe, expect, it } from 'vitest';
import { MANAGE_BLUEPRINT_RECORDS } from './index.js';
import { getParentToolMetadata } from '../parent-metadata.js';

const PARENT = getParentToolMetadata('manage_blueprint');

function findByAction(action: string) {
  const record = MANAGE_BLUEPRINT_RECORDS.find((r) => r.legacyIds[0].action === action);
  if (!record) throw new Error(`Record not found for action: ${action}`);
  return record;
}

describe('manage_blueprint carries canonical parent metadata', () => {
  it('stamps the canonical parent metadata on representative records', () => {
    for (const action of ['create', 'add_component', 'add_variable', 'create_node', 'create_widget_blueprint', 'add_button']) {
      const record = findByAction(action);
      expect(record.parent).toEqual(PARENT);
      expect(record.parent.parent).toBe('manage_blueprint');
      expect(record.parent.parent).toBe(record.routing.parentTool);
    }
  });

  it('matches the canonical lookup description and category (no duplication)', () => {
    const record = findByAction('create');
    expect(record.parent.description).toBe(PARENT.description);
    expect(record.parent.category).toBe(PARENT.category);
    expect(record.parent.category).toBe('core');
  });

  it('stamps parent metadata on all 104 records', () => {
    expect(MANAGE_BLUEPRINT_RECORDS).toHaveLength(104);
    for (const record of MANAGE_BLUEPRINT_RECORDS) {
      expect(record.parent).toEqual(PARENT);
    }
  });
});
