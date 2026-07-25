import { AsyncLocalStorage } from 'node:async_hooks';

import type { ConsentGrant } from '../tools/catalog/capabilities/semantic/authorization.js';

const gatewayConsentStorage = new AsyncLocalStorage<ConsentGrant>();

export function runWithGatewayConsent<T>(consent: ConsentGrant, fn: () => T): T {
  return gatewayConsentStorage.run(consent, fn);
}

export function getGatewayConsent(): ConsentGrant | undefined {
  return gatewayConsentStorage.getStore();
}
