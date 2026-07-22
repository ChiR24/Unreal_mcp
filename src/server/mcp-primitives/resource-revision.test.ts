import { describe, expect, it } from 'vitest';
import {
  INITIAL_REVISION,
  SUBSCRIBABLE_URIS,
  asResourceRevision,
  isSubscribableUri,
  nextRevision,
  InMemoryRevisionProvider,
  type ResourceRevision,
} from './resource-revision.js';

describe('resource-revision C2 primitive', () => {
  it('allowlists exactly the nine subscribable URIs', () => {
    // Given / When
    const uris = [...SUBSCRIBABLE_URIS];

    // Then
    expect(uris).toEqual([
      'ue://capability/catalog',
      'ue://project',
      'ue://level',
      'ue://selection',
      'ue://asset-registry',
      'ue://pie',
      'ue://build',
      'ue://render',
      'ue://logs',
    ]);
    expect(new Set(uris).size).toBe(9);
  });

  it('narrows only allowlisted URIs via the guard', () => {
    // Given / When / Then
    for (const uri of SUBSCRIBABLE_URIS) {
      expect(isSubscribableUri(uri)).toBe(true);
    }
    expect(isSubscribableUri('ue://assets')).toBe(false);
    expect(isSubscribableUri('ue://capability/manage_asset')).toBe(false);
    expect(isSubscribableUri('file:///etc/passwd')).toBe(false);
  });

  it('parses valid revisions and rejects non-monotonic candidates', () => {
    // Given / When
    const parsed = asResourceRevision(7);

    // Then
    expect(parsed).toBe(7);
    expect(() => asResourceRevision(0)).toThrow(RangeError);
    expect(() => asResourceRevision(-1)).toThrow(RangeError);
    expect(() => asResourceRevision(1.5)).toThrow(RangeError);
  });

  it('advances revisions monotonically', () => {
    // Given
    const start: ResourceRevision = INITIAL_REVISION;

    // When
    const second = nextRevision(start);
    const third = nextRevision(second);

    // Then
    expect(start).toBe(1);
    expect(second).toBe(2);
    expect(third).toBe(3);
    expect(third).toBeGreaterThan(start);
  });

  it('reports INITIAL_REVISION until a URI is advanced', () => {
    // Given
    const provider = new InMemoryRevisionProvider();

    // When
    const before = provider.currentRevision('ue://selection');
    provider.set('ue://selection', asResourceRevision(4));
    const after = provider.currentRevision('ue://selection');

    // Then
    expect(before).toBe(INITIAL_REVISION);
    expect(after).toBe(4);
    expect(provider.currentRevision('ue://project')).toBe(INITIAL_REVISION);
  });
});
