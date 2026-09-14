/**
 * Runtime inspection records (2 actions): runtime_report and pie_report.
 *
 * Both dispatch to the shared runtime-report handler. pie_report is aliased to
 * runtime_report in inspect-actions.ts for switch routing, but the handler
 * re-dispatches the original pie_report action, so the record keeps pie_report.
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildCoreRecord } from '../core/builder.js';
import { P } from './properties.js';
import { RUNTIME_REPORT_OUTPUT } from './RUNTIME_REPORT_OUTPUT.js';

const D = 'inspect';
const NR = 'Distinct inspect verb and target; no cross-tool duplicate.';

export const RUNTIME_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'inspect', action: 'runtime_report', dispatchAction: 'runtime_report', domain: D, family: 'runtime',
    summary: 'Return a runtime report for the current PIE/simulate session.',
    whenToUse: ['Runtime state of actors/components/properties must be inspected during PIE.'],
    whenNotToUse: ['The editor is not in PIE; the report will be empty.'],
    inputProps: {
      filter: P.filter, actorName: P.actorName, name: P.name,
      componentName: P.componentName, componentNames: P.componentNames,
      propertyName: P.propertyName, propertyPath: P.propertyPath, propertyNames: P.propertyNames,
    },
    required: [],
    effect: 'read', costLatency: 'instant', costResources: 'low',
    outputProps: { ...RUNTIME_REPORT_OUTPUT },
    outputRequired: [],
    exampleInput: { action: 'runtime_report', actorName: 'PlayerStart_1' },
    exampleOutput: { success: true, message: 'Runtime report', worldName: 'Demo', worldType: 'PIE', isPIE: true, count: 1, totalActorCount: 39 },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'inspect', action: 'pie_report', dispatchAction: 'pie_report', domain: D, family: 'runtime',
    summary: 'Return a PIE-specific runtime report (TS aliases to runtime_report for routing, then re-dispatches pie_report).',
    whenToUse: ['PIE-only runtime state must be inspected.'],
    whenNotToUse: ['A general runtime report is needed; use runtime_report.'],
    inputProps: {
      filter: P.filter, actorName: P.actorName, name: P.name,
      componentName: P.componentName, componentNames: P.componentNames,
      propertyName: P.propertyName, propertyPath: P.propertyPath, propertyNames: P.propertyNames,
    },
    required: [],
    effect: 'read', costLatency: 'instant', costResources: 'low',
    outputProps: { ...RUNTIME_REPORT_OUTPUT },
    outputRequired: [],
    exampleInput: { action: 'pie_report' },
    exampleOutput: { success: true, message: 'PIE report', worldName: 'Demo', worldType: 'PIE', isPIE: true, count: 0, totalActorCount: 39 },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET',
    normalizationRationale: 'inspect-actions.ts aliases pie_report to runtime_report for switch routing, but inspect-global-actions.ts re-dispatches the original pie_report action; the record preserves the canonical pie_report dispatch rather than collapsing it into runtime_report.',
  }),
];
