/**
 * Canonical data types for the Task 5 action-normalization inventory.
 *
 * This module is pure type + constant data. It performs no runtime side effects
 * and must never be imported by the MCP server runtime path (it only feeds the
 * deterministic generator, audit CLI, and focused unit tests).
 *
 * The inventory is CANONICAL INPUT DATA, not generated runtime behavior.
 */

/**
 * A-F taxonomy per the plan (strict; no seventh class).
 * A  = true duplicate: same canonical capability surfaced under >1 tool.
 * B  = alias: a synonym name that resolves to another canonical capability.
 * C  = distinct target/semantics: a unique capability with its own target.
 * D  = composite: an action composing several lower-level actions.
 * E  = preset/workflow: a scaffolding preset (e.g. create_*_menu/_screen/_widget).
 * F  = obsolete/version-specific: retained for compatibility but not advertised.
 *
 * The prior 'P' (primary baseline) class is intentionally REMOVED. Primary/alias
 * status is modelled by the separate `role` field on each occurrence so that the
 * normalization-class field stays strictly A-F for every one of the 1,335 rows.
 */
export const CLASSIFICATIONS = ['A', 'B', 'C', 'D', 'E', 'F'] as const;
export type Classification = (typeof CLASSIFICATIONS)[number];

/** Primary/alias role for an occurrence, independent of its A-F class. */
export const ROLES = ['primary', 'alias'] as const;
export type Role = (typeof ROLES)[number];

export const CLASSIFICATION_MEANING: Readonly<Record<Classification, string>> = {
  A: 'true duplicate (shared canonical across tools)',
  B: 'alias of another canonical',
  C: 'distinct target / semantics (unique capability)',
  D: 'composite action',
  E: 'preset/workflow scaffold',
  F: 'obsolete / version-specific',
};

/** Disposition for each public occurrence in the normalized model. */
export const DISPOSITIONS = [
  'keep',
  'alias',
  'map',
  'promote',
  'remove',
  'review',
] as const;
export type Disposition = (typeof DISPOSITIONS)[number];

/** Raw-route reconciliation status (extended as pilot tasks supply C++ evidence). */
export const ROUTE_STATUSES = ['exposed', 'hidden', 'raw', 'dead'] as const;
export type RouteStatus = (typeof ROUTE_STATUSES)[number];

/**
 * Disposition for a non-public (hidden/raw/dead) route in the separate
 * `routeDispositions` model. Exactly one of promote/map/remove — never the
 * softer 'keep'/'review' used for public occurrences.
 */
export const ROUTE_DISPOSITIONS = ['promote', 'map', 'remove'] as const;
export type RouteDisposition = (typeof ROUTE_DISPOSITIONS)[number];

/** Status values admitted in the separate `routeDispositions` model. */
export const ROUTE_DISPOSITION_STATUSES = ['hidden', 'raw', 'dead'] as const;
export type RouteDispositionStatus = (typeof ROUTE_DISPOSITION_STATUSES)[number];

/** Verb-family used by the reviewed 817 metric. */
export const VERB_FAMILY = ['add', 'create', 'set', 'configure'] as const;
export type VerbFamily = (typeof VERB_FAMILY)[number];

/** Fixed schema version for the artifact (independent of package version). */
export const INVENTORY_SCHEMA_VERSION = 'task5.normalization.v1';

/** A single source-backed citation for a group evidence row. */
export interface Citation {
  /** Relative module path where the cited symbol is declared or implemented. */
  readonly source: string;
  /** Stable symbol path identifying the cited route/action (literal in source). */
  readonly symbol: string;
}

/** Source evidence for an occurrence / canonical definition / route disposition. */
export interface Evidence {
  /** Relative module path where the action/route is declared or implemented. */
  readonly source: string;
  /** Stable symbol path identifying the action/route. */
  readonly symbol: string;
  /** Parent tool or native domain that owns the occurrence/route. */
  readonly tool: string;
  /**
   * Additional source-backed citations for group rows whose constituent
   * symbols span multiple concrete files. Each citation's full symbol must
   * be literally present in its paired source file.
   */
  readonly citations?: readonly Citation[];
}

/** Raw-route ownership captured for each public occurrence. */
export interface RawRoute {
  /** The parent tool that owns the occurrence. */
  readonly ownerTool: string;
  /** Authoritative TS surface the occurrence is declared on. */
  readonly surface: 'ts-action-enum';
  /** Reconciliation status against native/handler routes. */
  readonly status: RouteStatus;
  /** Fixed semantic namespace (target/domain) for the occurrence. */
  readonly namespace: string;
}

/** One {tool,action} public occurrence mapped to a canonical capability. */
export interface OccurrenceRecord {
  /** Unique key `${tool}:${action}`. */
  readonly occurrenceKey: string;
  readonly tool: string;
  readonly action: string;
  /** Canonical capability id this occurrence resolves to. */
  readonly canonicalId: string;
  readonly classification: Classification;
  /** Primary/alias role, independent of the A-F class. */
  readonly role: Role;
  readonly disposition: Disposition;
  readonly evidence: Evidence;
  readonly rawRoute: RawRoute;
  /** Explicit adjudication justification (required for A/C/E/F/promote/map/remove). */
  readonly justification?: string;
}

/** A unique canonical capability definition referenced by >=1 occurrence. */
export interface CanonicalDefinition {
  /** Unique canonical id (no duplicates allowed across the artifact). */
  readonly canonicalId: string;
  readonly action: string;
  /** Fixed semantic namespace (target/domain). */
  readonly namespace: string;
  readonly classification: Classification;
  readonly disposition: Disposition;
  /** Occurrence keys that reference this canonical id. */
  readonly occurrences: readonly string[];
  readonly evidence: Evidence;
  /** Occurrence keys that are aliases of this canonical. */
  readonly aliases: readonly string[];
}

/**
 * One non-public (hidden/raw/dead) route reviewed from the current code and the
 * research ledgers. Every row is resolved: it carries a promote/map/remove
 * disposition, source-backed evidence, and either a target canonical id (for
 * promote/map) or removal guidance (for remove). There are zero unresolved rows.
 */
export interface RouteDispositionRecord {
  /** Unique key, e.g. `route:widget:apply_style_to_widget`. */
  readonly dispositionKey: string;
  /** The route/action name as declared in native code or TS. */
  readonly route: string;
  /** Optional sub-action qualifier. */
  readonly action?: string;
  /** Domain/area the route belongs to (widget, graph, skeleton, ai, gas, ...). */
  readonly domain: string;
  /** Reconciliation status: hidden / raw / dead. */
  readonly status: RouteDispositionStatus;
  /** Owning native domain or parent tool. */
  readonly owner: string;
  readonly evidence: Evidence;
  /** Target canonical id for promote/map dispositions. */
  readonly targetCanonicalId?: string;
  /** Removal guidance for remove dispositions. */
  readonly removalGuidance?: string;
  /** Exactly promote / map / remove. */
  readonly disposition: RouteDisposition;
  /** Source-backed rationale (cited claim + concrete behavior). */
  readonly rationale: string;
  /** Always true; the completeness check rejects any false/unresolved row. */
  readonly resolved: true;
}

/** Deterministic, reviewed metrics block. */
export interface InventoryMetrics {
  readonly occurrenceCount: number;
  readonly toolCount: number;
  readonly distinctActionNames: number;
  readonly duplicateNames: number;
  readonly duplicateNameOccurrences: number;
  readonly maxExactNameReductions: number;
  readonly actualCanonicalReductions: number;
  readonly verbFamilyAddCreateSetConfigure: number;
  readonly unclassifiedOccurrences: number;
  readonly canonicalCollisions: number;
  readonly classificationCounts: Readonly<Record<Classification, number>>;
  readonly dispositionCounts: Readonly<Record<Disposition, number>>;
  // Route-disposition metrics (separate model; do not distort the 1,335 public count).
  readonly routeDispositionTotal: number;
  readonly routeDispositionUnresolved: number;
  readonly routeStatusCounts: Readonly<Record<RouteDispositionStatus, number>>;
  readonly routeDispositionCounts: Readonly<Record<RouteDisposition, number>>;
}

/** Top-level canonical inventory artifact. */
export interface NormalizationInventory {
  readonly schemaVersion: string;
  readonly metadata: {
    readonly generatedBy: string;
    readonly sourceToolCount: number;
    readonly occurrenceCount: number;
    /** Deterministic SHA-256 of the canonical body (excludes this field). */
    readonly contentSha256: string;
  };
  readonly metrics: InventoryMetrics;
  readonly canonicalDefinitions: readonly CanonicalDefinition[];
  readonly occurrences: readonly OccurrenceRecord[];
  /** Separate model for non-public hidden/raw/dead routes. */
  readonly routeDispositions: readonly RouteDispositionRecord[];
}
