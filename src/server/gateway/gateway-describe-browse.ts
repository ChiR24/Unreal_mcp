// src/server/gateway/gateway-describe-browse.ts
// The two bounded browse levels above a single capability: catalog -> domain
// -> family. No level emits a schema body, so walking the whole tree cannot
// dump the catalog; each entry instead carries the exact next call that drills
// one level deeper.

import { capabilitiesInFamily, capabilityIndex, catalogRevision } from './gateway-capability-index.js';
import { capabilityAvailability } from './gateway-availability.js';
import { primaryExecutableAction } from './gateway-capability-view.js';
import { closestMatches, MAX_SUGGESTIONS } from './gateway-guidance.js';
import { gatewayError } from './gateway-shared.js';

const DEFAULT_BROWSE_LIMIT = 50;

type Page = { readonly limit: number; readonly offset: number };

function paged<T>(entries: readonly T[], page: Page): { rows: T[]; hasMore: boolean } {
  const rows = entries.slice(page.offset, page.offset + page.limit);
  return { rows, hasMore: page.offset + rows.length < entries.length };
}

function browseEnvelope(scope: string, extra: Record<string, unknown>): Record<string, unknown> {
  return { success: true, operation: 'describe', catalogRevision: catalogRevision(), scope, ...extra };
}

export function describeCatalog(page: Page): Record<string, unknown> {
  const index = capabilityIndex();
  const counts = new Map<string, number>();
  for (const record of index.records) {
    counts.set(record.discovery.domain, (counts.get(record.discovery.domain) ?? 0) + 1);
  }
  const { rows, hasMore } = paged(index.domains, page);
  return browseEnvelope('catalog', {
    domains: rows.map((domain) => ({
      domain,
      capabilityCount: counts.get(domain) ?? 0,
      familyCount: (index.familiesByDomain.get(domain) ?? []).length,
      nextCall: { operation: 'describe', domain }
    })),
    domainCount: index.domains.length,
    domainOffset: page.offset,
    domainLimit: page.limit,
    domainHasMore: hasMore,
    capabilityCount: index.records.length,
    message: 'Catalog domains. Drill into a domain to list its families, then a family to list capabilities.'
  });
}

export function unknownDomainError(domain: string): Record<string, unknown> {
  return {
    ...gatewayError('describe', 'UNKNOWN_DOMAIN', `Unknown domain '${domain}'.`),
    suggestions: closestMatches(domain, [...capabilityIndex().domains], MAX_SUGGESTIONS),
    nextCall: { operation: 'describe' }
  };
}

export function unknownFamilyError(domain: string, family: string): Record<string, unknown> {
  const families = capabilityIndex().familiesByDomain.get(domain) ?? [];
  return {
    ...gatewayError('describe', 'UNKNOWN_FAMILY', `Unknown family '${family}' in domain '${domain}'.`),
    domain,
    availableFamilies: families,
    suggestions: closestMatches(family, [...families], MAX_SUGGESTIONS),
    nextCall: { operation: 'describe', domain }
  };
}

export function describeDomain(domain: string, page: Page): Record<string, unknown> {
  const index = capabilityIndex();
  if (!index.domains.includes(domain)) return unknownDomainError(domain);
  const families = index.familiesByDomain.get(domain) ?? [];
  const { rows, hasMore } = paged(families, page);
  return browseEnvelope('domain', {
    domain,
    families: rows.map((family) => ({
      family,
      capabilityCount: capabilitiesInFamily(domain, family).length,
      nextCall: { operation: 'describe', domain, family }
    })),
    familyCount: families.length,
    familyOffset: page.offset,
    familyLimit: page.limit,
    familyHasMore: hasMore,
    message: 'Domain families. Drill into a family to list its capabilities.'
  });
}

export function describeFamily(domain: string, family: string, page: Page): Record<string, unknown> {
  const index = capabilityIndex();
  if (!index.domains.includes(domain)) return unknownDomainError(domain);
  if (!(index.familiesByDomain.get(domain) ?? []).includes(family)) {
    return unknownFamilyError(domain, family);
  }
  const records = capabilitiesInFamily(domain, family);
  const { rows, hasMore } = paged(records, page);
  return browseEnvelope('family', {
    domain,
    family,
    capabilities: rows.map((record) => ({
      capability: record.id,
      parentTool: record.routing.parentTool,
      action: primaryExecutableAction(record),
      summary: record.discovery.summary,
      effect: record.behavior.effect,
      availability: capabilityAvailability(record).status,
      nextCall: { operation: 'describe', capability: record.id }
    })),
    capabilityCount: records.length,
    capabilityOffset: page.offset,
    capabilityLimit: page.limit,
    capabilityHasMore: hasMore,
    message: 'Family capabilities. Drill into a capability for its exact contract.'
  });
}

export function resolveDomainForFamily(family: string): string | undefined {
  const index = capabilityIndex();
  for (const domain of index.domains) {
    if ((index.familiesByDomain.get(domain) ?? []).includes(family)) return domain;
  }
  return undefined;
}

export { DEFAULT_BROWSE_LIMIT };
export type { Page };
