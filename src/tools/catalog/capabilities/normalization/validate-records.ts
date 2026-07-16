/**
 * Record-level validation helpers for normalization inventory occurrences and
 * canonical definitions. Shared by the top-level `validate.ts` orchestrator.
 */

import {
  type CanonicalDefinition,
  CLASSIFICATIONS,
  type Classification,
  DISPOSITIONS,
  type Disposition,
  type OccurrenceRecord,
  ROLES,
  ROUTE_DISPOSITION_STATUSES,
  ROUTE_DISPOSITIONS,
  ROUTE_STATUSES,
  type Role,
  type RouteDisposition,
  type RouteDispositionRecord,
  type RouteDispositionStatus,
  type RouteStatus,
} from './types.js';

export function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value);
}

export function compareKeys(a: string, b: string): number {
  if (a < b) return -1;
  if (a > b) return 1;
  return 0;
}

const EVIDENCE_KEYS = new Set(['source', 'symbol', 'tool', 'citations']);
const CITATION_KEYS = new Set(['source', 'symbol']);
const RAWROUTE_KEYS = new Set(['ownerTool', 'surface', 'status', 'namespace']);
const OCC_KEYS = new Set([
  'occurrenceKey',
  'tool',
  'action',
  'canonicalId',
  'classification',
  'role',
  'disposition',
  'evidence',
  'rawRoute',
  'justification',
]);
const CANON_KEYS = new Set([
  'canonicalId',
  'action',
  'namespace',
  'classification',
  'disposition',
  'occurrences',
  'evidence',
  'aliases',
]);

export class InventoryValidationError extends Error {
  constructor(message: string) {
    super(message);
    this.name = 'InventoryValidationError';
  }
}

export function assertNoExtraKeys(
  obj: Record<string, unknown>,
  allowed: ReadonlySet<string>,
  path: string,
): void {
  for (const key of Object.keys(obj)) {
    if (!allowed.has(key)) {
      throw new InventoryValidationError(`unknown field "${key}" at ${path}`);
    }
  }
}

export function validateEvidence(value: unknown, path: string): void {
  if (!isRecord(value)) {
    throw new InventoryValidationError(`evidence at ${path} is not an object`);
  }
  assertNoExtraKeys(value, EVIDENCE_KEYS, `${path}.evidence`);
  for (const field of ['source', 'symbol', 'tool']) {
    const v = value[field];
    if (typeof v !== 'string' || v.length === 0) {
      throw new InventoryValidationError(`evidence.${field} missing at ${path}`);
    }
  }
  const citations = value.citations;
  if (citations !== undefined) {
    if (!Array.isArray(citations)) {
      throw new InventoryValidationError(`evidence.citations not array at ${path}`);
    }
    citations.forEach((c, i) => {
      if (!isRecord(c)) {
        throw new InventoryValidationError(`evidence.citations[${i}] at ${path} is not an object`);
      }
      assertNoExtraKeys(c, CITATION_KEYS, `${path}.evidence.citations[${i}]`);
      const src = c.source;
      if (typeof src !== 'string' || src.length === 0) {
        throw new InventoryValidationError(`evidence.citations[${i}].source missing at ${path}`);
      }
      const sym = c.symbol;
      if (typeof sym !== 'string' || sym.length === 0) {
        throw new InventoryValidationError(`evidence.citations[${i}].symbol missing at ${path}`);
      }
    });
  }
}

export function validateRawRoute(value: unknown, path: string): void {
  if (!isRecord(value)) {
    throw new InventoryValidationError(`rawRoute at ${path} is not an object`);
  }
  assertNoExtraKeys(value, RAWROUTE_KEYS, `${path}.rawRoute`);
  if (value.surface !== 'ts-action-enum') {
    throw new InventoryValidationError(`rawRoute.surface invalid at ${path}`);
  }
  if (typeof value.ownerTool !== 'string' || value.ownerTool.length === 0) {
    throw new InventoryValidationError(`rawRoute.ownerTool missing at ${path}`);
  }
  if (!ROUTE_STATUSES.includes(value.status as RouteStatus)) {
    throw new InventoryValidationError(`rawRoute.status invalid at ${path}`);
  }
}

export function validateOccurrence(value: unknown, path: string): OccurrenceRecord {
  if (!isRecord(value)) {
    throw new InventoryValidationError(`occurrence at ${path} is not an object`);
  }
  assertNoExtraKeys(value, OCC_KEYS, path);
  const classification = value.classification as Classification;
  const disposition = value.disposition as Disposition;
  const role = value.role as Role;
  if (!CLASSIFICATIONS.includes(classification)) {
    throw new InventoryValidationError(
      `invalid classification "${String(value.classification)}" at ${path}`,
    );
  }
  if (!ROLES.includes(role)) {
    throw new InventoryValidationError(
      `invalid role "${String(value.role)}" at ${path}`,
    );
  }
  if (!DISPOSITIONS.includes(disposition)) {
    throw new InventoryValidationError(
      `invalid disposition "${String(value.disposition)}" at ${path}`,
    );
  }
  if (typeof value.occurrenceKey !== 'string' || value.occurrenceKey.length === 0) {
    throw new InventoryValidationError(`occurrenceKey missing at ${path}`);
  }
  if (typeof value.canonicalId !== 'string' || value.canonicalId.length === 0) {
    throw new InventoryValidationError(`canonicalId missing at ${path}`);
  }
  validateEvidence(value.evidence, path);
  validateRawRoute(value.rawRoute, path);
  return {
    occurrenceKey: value.occurrenceKey,
    tool: value.tool as string,
    action: value.action as string,
    canonicalId: value.canonicalId,
    classification,
    role,
    disposition,
    evidence: value.evidence as OccurrenceRecord['evidence'],
    rawRoute: value.rawRoute as OccurrenceRecord['rawRoute'],
    justification: typeof value.justification === 'string' ? value.justification : undefined,
  };
}

export function validateCanonical(value: unknown, path: string): CanonicalDefinition {
  if (!isRecord(value)) {
    throw new InventoryValidationError(`canonical at ${path} is not an object`);
  }
  assertNoExtraKeys(value, CANON_KEYS, path);
  if (!Array.isArray(value.occurrences)) {
    throw new InventoryValidationError(`canonical.occurrences not array at ${path}`);
  }
  if (!Array.isArray(value.aliases)) {
    throw new InventoryValidationError(`canonical.aliases not array at ${path}`);
  }
  validateEvidence(value.evidence, path);
  return {
    canonicalId: value.canonicalId as string,
    action: value.action as string,
    namespace: value.namespace as string,
    classification: value.classification as Classification,
    disposition: value.disposition as Disposition,
    occurrences: value.occurrences as readonly string[],
    evidence: value.evidence as CanonicalDefinition['evidence'],
    aliases: value.aliases as readonly string[],
  };
}

const ROUTE_DISPOSITION_KEYS = new Set([
  'dispositionKey',
  'route',
  'action',
  'domain',
  'status',
  'owner',
  'evidence',
  'targetCanonicalId',
  'removalGuidance',
  'disposition',
  'rationale',
  'resolved',
]);

export function validateRouteDisposition(value: unknown, path: string): RouteDispositionRecord {
  if (!isRecord(value)) {
    throw new InventoryValidationError(`routeDisposition at ${path} is not an object`);
  }
  assertNoExtraKeys(value, ROUTE_DISPOSITION_KEYS, path);
  const str = (field: string): string => {
    const v = value[field];
    if (typeof v !== 'string' || v.length === 0) {
      throw new InventoryValidationError(`routeDisposition.${field} missing at ${path}`);
    }
    return v;
  };
  const disposition = str('disposition') as RouteDisposition;
  if (!ROUTE_DISPOSITIONS.includes(disposition)) {
    throw new InventoryValidationError(
      `invalid route disposition "${String(value.disposition)}" at ${path}`,
    );
  }
  const status = str('status') as RouteDispositionStatus;
  if (!ROUTE_DISPOSITION_STATUSES.includes(status)) {
    throw new InventoryValidationError(
      `invalid route status "${String(value.status)}" at ${path}`,
    );
  }
  if (value.resolved !== true) {
    throw new InventoryValidationError(`routeDisposition not resolved at ${path}`);
  }
  if (disposition === 'remove') {
    if (typeof value.removalGuidance !== 'string' || value.removalGuidance.length === 0) {
      throw new InventoryValidationError(`routeDisposition remove missing removalGuidance at ${path}`);
    }
  } else if (typeof value.targetCanonicalId !== 'string' || value.targetCanonicalId.length === 0) {
    throw new InventoryValidationError(`routeDisposition ${disposition} missing targetCanonicalId at ${path}`);
  }
  validateEvidence(value.evidence, path);
  return {
    dispositionKey: str('dispositionKey'),
    route: str('route'),
    ...(typeof value.action === 'string' ? { action: value.action } : {}),
    domain: str('domain'),
    status,
    owner: str('owner'),
    evidence: value.evidence as RouteDispositionRecord['evidence'],
    ...(disposition === 'remove'
      ? { removalGuidance: value.removalGuidance as string }
      : { targetCanonicalId: value.targetCanonicalId as string }),
    disposition,
    rationale: str('rationale'),
    resolved: true,
  };
}
