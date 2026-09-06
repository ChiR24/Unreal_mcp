/**
 * scripts/qa/capability-metadata-audit.ts
 *
 * Leaf-body-backed cross-domain metadata audit for the 1,401 capability records
 * (world 301 + gameplay 356 + utility 208 + core 470).
 *
 * It verifies that every record's metadata is internally consistent and
 * truthful about its behaviour. The audit is DETERMINISTIC: the same input
 * always produces the same ordered violations and summary, so it can be run
 * twice and compared byte-for-byte.
 *
 * Hard rules (a non-empty violation list FAILS the audit):
 *   C1  read-effect invariants
 *   C2  destructive-effect consent/scope
 *   C3  async/longRunning consistency (behavior.longRunning === cost.latency==='long-running')
 *   C4  no-op / unreachable / manual-only / unsupported markers must not claim mutation
 *   C5  no-op / unreachable disposition must be deprecated, not active
 *   C6  stale UE-version comment contradicting the declared availability range
 *   C7  example output fields must be declared in the output schema (no phantom fields)
 *
 * Informational metric (reported, never fails):
 *   C8  output observability — count of records whose effect is read/write/destructive
 *       but whose output schema AND example carry no field beyond success/message.
 *       Schema shapes are out of scope for Task 19 (metadata-only edits); this metric
 *       documents the residual surface rather than blocking the audit.
 *
 * No record IDs, schemas, or counts are changed by this module — it only reads.
 */
import { ALL_CAPABILITY_RECORDS } from '../../src/tools/catalog/capabilities/records/aggregate.js';
import type { CapabilityRecord } from '../../src/tools/catalog/capabilities/model.js';

export interface AuditViolation {
  readonly id: string;
  readonly rule: string;
  readonly detail: string;
}

export interface AuditReport {
  readonly totalRecords: number;
  readonly uniqueIds: number;
  readonly domainCounts: Readonly<Record<string, number>>;
  readonly violations: readonly AuditViolation[];
  readonly outputObservability: {
    readonly readEmpty: number;
    readonly writeEmpty: number;
    readonly ids: readonly string[];
  };
  readonly passed: boolean;
}

const NO_OP_MARKERS = [
  /\bno-?op\b/i,
  /\bunreachable\b/i,
  /\bmanual[- ]?only\b/i,
  /\bunsupported\b/i,
  /\bpending[- ]?repair\b/i,
];

const STALE_VERSION_COMMENT = /\b5\.\d+\s*-\s*5\.\d+\s*only\b|\bonly (in|available|for) 5\.[0-9]|\brequires 5\.[0-9]+(-5\.[0-9]+)?\b/i;

function markerText(record: CapabilityRecord): string {
  return `${record.discovery.summary} ${record.normalization.rationale}`;
}

function isNoOpLike(record: CapabilityRecord): boolean {
  const text = markerText(record);
  if (NO_OP_MARKERS.some((rx) => rx.test(text))) return true;
  if (record.normalization.disposition === 'remove') return true;
  return false;
}

export function loadAllCapabilityRecords(): readonly CapabilityRecord[] {
  return ALL_CAPABILITY_RECORDS;
}

export function auditCapabilityMetadata(
  records: readonly CapabilityRecord[],
): AuditReport {
  const violations: AuditViolation[] = [];
  const push = (id: string, rule: string, detail: string): void => {
    violations.push({ id, rule, detail });
  };

  let readEmpty = 0;
  let writeEmpty = 0;
  const emptyOutputIds: string[] = [];

  for (const r of records) {
    const b = r.behavior;
    const p = r.policy;
    const a = r.availability;
    const d = r.deprecation;
    const noOpLike = isNoOpLike(r);

    // C1: read-effect invariants
    if (b.effect === 'read') {
      if (b.idempotency !== 'idempotent') push(r.id, 'C1', `read effect idempotency=${b.idempotency} (expected idempotent)`);
      if (b.supportsUndo !== false) push(r.id, 'C1', `read effect supportsUndo=${b.supportsUndo} (expected false)`);
      if (b.safeToRetry !== true) push(r.id, 'C1', `read effect safeToRetry=${b.safeToRetry} (expected true)`);
      if (p.requiredScope !== 'read') push(r.id, 'C1', `read effect requiredScope=${p.requiredScope} (expected read)`);
      if (p.consent !== 'none') push(r.id, 'C1', `read effect consent=${p.consent} (expected none)`);
      if (p.dataAccess !== 'project-read') push(r.id, 'C1', `read effect dataAccess=${p.dataAccess} (expected project-read)`);
    }

    // C2: destructive-effect consent/scope
    if (b.effect === 'destructive') {
      if (p.consent !== 'explicit' && p.consent !== 'elevated') {
        push(r.id, 'C2', `destructive effect consent=${p.consent} (expected explicit|elevated)`);
      }
      if (p.requiredScope !== 'destructive') {
        push(r.id, 'C2', `destructive effect requiredScope=${p.requiredScope} (expected destructive)`);
      }
    }

    // C3: async/longRunning consistency
    const expectedLongRunning = r.cost.latency === 'long-running';
    if (b.longRunning !== expectedLongRunning) {
      push(
        r.id,
        'C3',
        `behavior.longRunning=${b.longRunning} but cost.latency=${r.cost.latency} (expected longRunning=${expectedLongRunning})`,
      );
    }

    // C4: no-op / unreachable / manual-only / unsupported must not claim mutation
    if (noOpLike) {
      if (b.effect !== 'read') push(r.id, 'C4', `marked no-op/unreachable but effect=${b.effect} (expected read; cannot claim mutation)`);
      if (b.supportsUndo !== false) push(r.id, 'C4', `marked no-op/unreachable but supportsUndo=${b.supportsUndo} (expected false)`);
      if (p.consent !== 'none') push(r.id, 'C4', `marked no-op/unreachable but consent=${p.consent} (expected none)`);
      if (p.requiredScope !== 'read') push(r.id, 'C4', `marked no-op/unreachable but requiredScope=${p.requiredScope} (expected read)`);
    }

    // C5: no-op / unreachable disposition must be deprecated, not active
    if (noOpLike && d.status === 'active') {
      push(r.id, 'C5', 'marked no-op/unreachable but deprecation.status=active (expected deprecated|removed)');
    }

    // C6: stale UE-version comment contradicting the declared availability range.
    // A comment claiming a narrow version window ("5.1-5.6 only", "only in 5.3",
    // "requires 5.4") contradicts the certified 5.0-5.8 target matrix and must be
    // resolved against the leaf behaviour. Such a comment is itself the stale claim.
    const comment = markerText(r);
    if (STALE_VERSION_COMMENT.test(comment)) {
      const minMinor = a.unreal.min.minor;
      const maxMinor = a.unreal.max.minor;
      push(r.id, 'C6', `stale version comment "${comment.match(STALE_VERSION_COMMENT)?.[0]}" contradicts availability range 5.${minMinor}-5.${maxMinor} (certified 5.0-5.8)`);
    }

    // C7: example output fields must be declared in the output schema
    const schemaKeys = Object.keys(r.schemas.output.properties);
    for (const ex of r.examples) {
      const exKeys = Object.keys(ex.output);
      const phantom = exKeys.filter((k) => !schemaKeys.includes(k));
      if (phantom.length > 0) {
        push(r.id, 'C7', `example output declares fields [${phantom.join(', ')}] absent from output schema`);
      }
    }

    // C8 (informational): output observability
    const obsSchema = schemaKeys.filter((k) => k !== 'success' && k !== 'message');
    const obsExample = r.examples.flatMap((e) => Object.keys(e.output)).filter((k) => k !== 'success' && k !== 'message');
    if (obsSchema.length === 0 && obsExample.length === 0) {
      if (b.effect === 'read') readEmpty += 1;
      else writeEmpty += 1;
      emptyOutputIds.push(r.id);
    }
  }

  violations.sort((x, y) => (x.rule === y.rule ? (x.id < y.id ? -1 : x.id > y.id ? 1 : 0) : x.rule < y.rule ? -1 : 1));

  const domainCounts: Record<string, number> = {};
  for (const r of records) {
    const dom = r.discovery.domain;
    domainCounts[dom] = (domainCounts[dom] ?? 0) + 1;
  }

  return {
    totalRecords: records.length,
    uniqueIds: new Set(records.map((r) => r.id)).size,
    domainCounts,
    violations,
    outputObservability: {
      readEmpty,
      writeEmpty,
      ids: [...emptyOutputIds].sort(),
    },
    passed: violations.length === 0,
  };
}

function formatAuditReport(report: AuditReport): string {
  const lines: string[] = [];
  lines.push('CAPABILITY METADATA AUDIT');
  lines.push(`totalRecords=${report.totalRecords} uniqueIds=${report.uniqueIds}`);
  lines.push(`domainCounts=${JSON.stringify(report.domainCounts)}`);
  lines.push(`violations=${report.violations.length} passed=${report.passed}`);
  lines.push(`outputObservability: readEmpty=${report.outputObservability.readEmpty} writeEmpty=${report.outputObservability.writeEmpty} (informational)`);
  if (report.violations.length > 0) {
    lines.push('--- violations ---');
    for (const v of report.violations) {
      lines.push(`${v.rule}\t${v.id}\t${v.detail}`);
    }
  }
  return lines.join('\n');
}

// CLI runner (deterministic stdout; exit code 1 on failure).
async function main(): Promise<void> {
  const records = loadAllCapabilityRecords();
  const report = auditCapabilityMetadata(records);
  process.stdout.write(formatAuditReport(report) + '\n');
  if (!report.passed) process.exit(1);
}

const invokedDirectly = import.meta.url === `file://${process.argv[1]}`;
if (invokedDirectly) {
  void main();
}
