// src/server/mcp-primitives/resource-revision.ts
// The revision every resource payload carries. Only the capability catalog's
// revision moves (it follows configure visibility changes); every other
// revisioned URI reports INITIAL_REVISION.

import { dynamicToolManager } from '../../tools/dynamic/dynamic-tool-manager.js';

/** A resource revision: an integer >= 1. */
export type ResourceRevision = number & { readonly __brand: 'ResourceRevision' };

export const INITIAL_REVISION: ResourceRevision = 1 as ResourceRevision;

/** A resource payload tagged with the URI it was read from and its revision. */
export interface RevisionedResource<T> {
  readonly uri: string;
  readonly revision: ResourceRevision;
  readonly data: T;
}

export type RevisionedUri = 'ue://capability/catalog' | 'ue://project' | 'ue://selection' | 'ue://pie' | 'ue://asset-registry';

export interface RevisionProvider {
  currentRevision(uri: RevisionedUri): ResourceRevision;
}

const shared: RevisionProvider = {
  currentRevision: (uri) => (uri === 'ue://capability/catalog'
    ? (dynamicToolManager.getCatalogStateRevision() + INITIAL_REVISION) as ResourceRevision
    : INITIAL_REVISION),
};

export function sharedRevisionProvider(): RevisionProvider {
  return shared;
}
