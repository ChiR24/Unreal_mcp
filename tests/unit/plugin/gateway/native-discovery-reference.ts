/// <reference types="node" />

// Task 25: single entry point the harness and the parity tests render through.

import { canonicalJson, type DiscoveryInput } from './native-discovery-model.js';
import { describeCapability } from './native-discovery-describe.js';
import { searchCapabilities } from './native-discovery-search.js';

export { describeCapability } from './native-discovery-describe.js';
export { searchCapabilities, MAX_RESULT_BYTES, SEARCH_DEFAULT_LIMIT, SEARCH_MAX_LIMIT } from './native-discovery-search.js';
export { DESCRIBE_DEFAULT_LIMIT, DESCRIBE_MAX_LIMIT } from './native-discovery-describe.js';
export { canonicalJson, closestMatches, loadCanonicalRegistry } from './native-discovery-model.js';

export const renderDiscovery = (input: DiscoveryInput & { readonly operation: string }): string =>
  canonicalJson(input.operation === 'search' ? searchCapabilities(input) : describeCapability(input));
