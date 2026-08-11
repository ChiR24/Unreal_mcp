/**
 * tests/unit/capability-metadata-audit.test.ts
 *
 * Cross-domain metadata audit for all 1,381 capability records.
 *
 * RED first: a seeded stale "5.1-5.6 only" comment OR verb-derived metadata
 * (a read record relabelled as a mutating write) MUST make the audit fail with
 * a leaf-evidence violation. This proves the audit catches dishonest/derived
 * metadata rather than rubber-stamping the source.
 *
 * GREEN after Task 19 corrections: the real 1,381-record universe passes with
 * zero hard violations, and the audit is deterministic across runs.
 */
import { describe, expect, it } from 'vitest';
import type { CapabilityRecord } from '../../src/tools/catalog/capabilities/model.js';
import {
  auditCapabilityMetadata,
  loadAllCapabilityRecords,
  type AuditViolation,
} from '../../scripts/qa/capability-metadata-audit.js';

function cloneRecord(record: CapabilityRecord): CapabilityRecord {
  return structuredClone(record);
}

describe('capability metadata audit — RED seed must fail', () => {
  it('flags a seeded stale "5.1-5.6 only" UE-version comment (C6)', () => {
    const records = loadAllCapabilityRecords();
    const target = cloneRecord(records[0]);
    const seeded: CapabilityRecord = {
      ...target,
      normalization: {
        ...target.normalization,
        rationale: 'Supported only in 5.1-5.6 only (legacy renderer path).',
      },
    };
    const report = auditCapabilityMetadata([seeded]);
    const c6 = report.violations.filter((v: AuditViolation) => v.rule === 'C6');
    expect(c6.length).toBeGreaterThan(0);
    expect(c6[0].id).toBe(seeded.id);
    expect(c6[0].detail).toMatch(/stale version comment/i);
  });

  it('flags verb-derived metadata: a read record claiming mutation (C1)', () => {
    const records = loadAllCapabilityRecords();
    const readRecord = records.find((r) => r.behavior.effect === 'read');
    if (!readRecord) throw new Error('expected at least one read-effect record in the universe');
    const seeded: CapabilityRecord = {
      ...readRecord,
      behavior: { ...readRecord.behavior, supportsUndo: true, safeToRetry: false },
      policy: { ...readRecord.policy, dataAccess: 'project-write' },
    };
    const report = auditCapabilityMetadata([seeded]);
    const flagged = report.violations.filter(
      (v: AuditViolation) => v.id === seeded.id && v.rule === 'C1',
    );
    expect(flagged.length).toBeGreaterThan(0);
  });

  it('flags a no-op-marked record that still claims mutation (C4)', () => {
    const records = loadAllCapabilityRecords();
    const target = cloneRecord(records[0]);
    const seeded: CapabilityRecord = {
      ...target,
      discovery: { ...target.discovery, summary: 'Documented no-op; performs no mutation.' },
      behavior: { ...target.behavior, effect: 'write', supportsUndo: true },
    };
    const report = auditCapabilityMetadata([seeded]);
    const c4 = report.violations.filter((v: AuditViolation) => v.id === seeded.id && v.rule === 'C4');
    expect(c4.length).toBeGreaterThan(0);
  });
});

describe('capability metadata audit — GREEN universe passes', () => {
  it('audits all 1,381 records with zero hard violations', () => {
    const records = loadAllCapabilityRecords();
    expect(records.length).toBe(1381);
    expect(new Set(records.map((r) => r.id)).size).toBe(1381);
    const report = auditCapabilityMetadata(records);
    expect(report.passed).toBe(true);
    expect(report.violations).toHaveLength(0);
  });

  it('is deterministic: two runs produce identical reports', () => {
    const records = loadAllCapabilityRecords();
    const a = JSON.stringify(auditCapabilityMetadata(records));
    const b = JSON.stringify(auditCapabilityMetadata(records));
    expect(a).toBe(b);
  });

  it('resolves every no-op / unreachable / manual-only / unsupported marker truthfully', () => {
    const records = loadAllCapabilityRecords();
    const noOpMarkers = [/\bno-?op\b/i, /\bunreachable\b/i, /\bmanual[- ]?only\b/i, /\bunsupported\b/i, /\bpending[- ]?repair\b/i];
    const flagged = records.filter((r) => {
      const text = `${r.discovery.summary} ${r.normalization.rationale}`;
      const marker = noOpMarkers.some((rx) => rx.test(text)) || r.normalization.disposition === 'remove';
      if (!marker) return false;
      // A no-op/unreachable record must not claim mutation.
      return r.behavior.effect !== 'read' || r.behavior.supportsUndo !== false || r.policy.consent !== 'none' || r.policy.requiredScope !== 'read' || r.deprecation.status === 'active';
    });
    expect(flagged).toHaveLength(0);
  });

  it('never declares an idempotent non-destructive capability unsafe to retry', () => {
    const records = loadAllCapabilityRecords();
    const contradictory = records
      .filter(
        (r) =>
          r.behavior.idempotency === 'idempotent'
          && r.behavior.effect !== 'destructive'
          && r.behavior.safeToRetry === false,
      )
      .map((r) => `${r.id} (${r.behavior.effect})`);
    expect(contradictory).toHaveLength(0);
  });

  it('does not grow the set of capabilities whose output schema promises nothing', () => {
    const records = loadAllCapabilityRecords();
    const sealedStubs = records
      .filter((r) => {
        const declared = Object.keys(r.schemas.output.properties);
        const envelopeOnly = declared.length === 2
          && declared.includes('success')
          && declared.includes('message');
        return envelopeOnly && r.schemas.output.additionalProperties === false;
      })
      .map((r) => r.id);
    // Ratchet, not a target: `additionalProperties: false` on a `{success, message}` schema
    // forbids the payload the summary promises. Clearing it is per-record output authoring.
    expect(sealedStubs.length).toBeLessThanOrEqual(827);
  });

  it('MCPBB-079: named read capabilities publish the data their summaries promise', () => {
    // Ground truth for the field contracts lives in the native handlers
    // (EnvironmentHandlersInspectSettings.cpp, LevelHandlersInfo.cpp).
    // generate_memory_report is deliberately excluded: PerformanceHandlersProfiling.cpp
    // replies envelope-only, so its stub is correct and the defect is handler-side (C++).
    const byId = new Map(loadAllCapabilityRecords().map((r) => [String(r.id), r]));
    const namedReadStubs = [
      'inspect.get_scene_stats',
      'inspect.get_performance_stats',
      'inspect.get_memory_stats',
      'inspect.get_editor_settings',
      'manage_level.get_summary',
    ];
    const stillSealed = namedReadStubs.filter((id) => {
      const record = byId.get(id);
      if (record === undefined) return true; // a missing record is itself a failure
      const declared = Object.keys(record.schemas.output.properties);
      const envelopeOnly = declared.length === 2
        && declared.includes('success')
        && declared.includes('message');
      return envelopeOnly && record.schemas.output.additionalProperties === false;
    });
    expect(stillSealed).toEqual([]);
  });

  it('declares the path parameter each material handler actually reads', () => {
    // These native handlers read assetPath with no fallback branch, and the native transport
    // has no alias layer, so declaring any other spelling is unsatisfiable there.
    const readsAssetPath = [
      'material.set_blend_mode', 'material.set_shading_model', 'material.set_material_domain',
      'material.compile_material', 'material.get_material_info', 'material.set_two_sided',
      'material.add_function_input', 'material.add_function_output',
      'material.get_material_function_info', 'material.set_material_parameter',
      'material.set_scalar_parameter_value', 'material.set_vector_parameter_value',
      'material.set_texture_parameter_value', 'material.set_static_switch_parameter_value',
    ];
    const byId = new Map(loadAllCapabilityRecords().map((r) => [String(r.id), r]));
    const undeclared = readsAssetPath.filter((id) => {
      const record = byId.get(id);
      return record === undefined || !Object.keys(record.schemas.input.properties).includes('assetPath');
    });
    expect(undeclared).toEqual([]);
  });
});
