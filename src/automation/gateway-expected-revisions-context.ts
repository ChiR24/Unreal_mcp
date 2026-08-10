import { AsyncLocalStorage } from 'node:async_hooks';

import type { ExpectedRevisions } from '../tools/catalog/capabilities/semantic/execution-options.js';

const gatewayExpectedRevisionsStorage = new AsyncLocalStorage<ExpectedRevisions>();

export function runWithGatewayExpectedRevisions<T>(
  expectedRevisions: ExpectedRevisions,
  fn: () => T
): T {
  return gatewayExpectedRevisionsStorage.run(expectedRevisions, fn);
}

export function getGatewayExpectedRevisions(): ExpectedRevisions | undefined {
  return gatewayExpectedRevisionsStorage.getStore();
}
