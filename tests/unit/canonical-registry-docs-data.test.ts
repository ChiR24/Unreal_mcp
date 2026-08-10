// Docs-data contract: every parent's actionCount must reflect its real action enum.
import { describe, expect, it } from 'vitest';
import { loadAllCapabilityRecords } from '../../scripts/qa/capability-metadata-audit.js';
import { deriveParents } from '../../scripts/canonical-registry/parent-derivation.js';
import { buildDocsData } from '../../scripts/canonical-registry/types.js';

describe('Task-23 docs data actionCount', () => {
  const parents = deriveParents(loadAllCapabilityRecords());
  const docs = buildDocsData(parents);

  it('derives exactly one DocEntry per parent (23)', () => {
    expect(parents.length).toBe(23);
    expect(docs.length).toBe(23);
  });

  it('every DocEntry has a positive actionCount from its action enum', () => {
    for (const entry of docs) {
      expect(entry.name).toBeTruthy();
      expect(entry.actionCount).toBeGreaterThan(0);
    }
  });

  it('sum of all actionCounts equals the total capability-record count (1,373)', () => {
    const total = docs.reduce((sum, entry) => sum + entry.actionCount, 0);
    expect(total).toBe(1373);
  });
});
