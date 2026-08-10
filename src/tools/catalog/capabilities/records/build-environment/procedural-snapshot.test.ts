/**
 * Focused test: the build_environment snapshot records declare the explicit
 * light selectors their handlers actually honour, in the correct direction.
 *
 * Grounded in the native Environment domain: ExportSnapshot reads
 * directionalLightActorPath/skyLightActorPath off the request payload, while
 * McpApplyEnvironmentSnapshot reads them off the snapshot file being imported.
 * Both echo the resolved actor paths back on the response, so the selectors are
 * input+output for export and output-only for import.
 */
import { describe, expect, it } from 'vitest';
import { BUILD_ENVIRONMENT_RECORDS } from './index.js';

const SELECTORS = ['directionalLightActorPath', 'skyLightActorPath'] as const;

function snapshotRecord(action: 'export_snapshot' | 'import_snapshot') {
  const record = BUILD_ENVIRONMENT_RECORDS.find((candidate) => candidate.legacyIds[0].action === action);
  if (record === undefined) {
    throw new TypeError(`build_environment ${action} record is unavailable`);
  }
  return record;
}

const exportRecord = snapshotRecord('export_snapshot');
const importRecord = snapshotRecord('import_snapshot');

describe('build_environment snapshot records: explicit light selectors', () => {
  it('Given export_snapshot, When its input is inspected, Then both payload-read light selectors are declared as strings', () => {
    const input = exportRecord.schemas.input.properties as Record<string, { type?: string }>;

    for (const selector of SELECTORS) {
      expect(input, `export_snapshot input should declare ${selector}`).toHaveProperty(selector);
      expect(input[selector].type).toBe('string');
    }
  });

  it('Given export_snapshot, When its input is inspected, Then the existing file-target params are retained', () => {
    const input = exportRecord.schemas.input.properties as Record<string, unknown>;

    for (const field of ['action', 'path', 'filename']) {
      expect(input, `export_snapshot input should retain ${field}`).toHaveProperty(field);
    }
  });

  it('Given import_snapshot, When its input is inspected, Then the selectors are absent because the handler reads them from the snapshot file', () => {
    const input = importRecord.schemas.input.properties as Record<string, unknown>;

    for (const selector of SELECTORS) {
      expect(input, `import_snapshot must not claim payload support for ${selector}`).not.toHaveProperty(selector);
    }
  });

  it.each([
    ['export_snapshot', exportRecord],
    ['import_snapshot', importRecord],
  ])('Given %s, When its output is inspected, Then both resolved light actor paths are declared', (_action, record) => {
    const output = record.schemas.output.properties as Record<string, { type?: string }>;

    for (const selector of SELECTORS) {
      expect(output).toHaveProperty(selector);
      expect(output[selector].type).toBe('string');
    }
  });

  it('Given both snapshot records, When the selector fragments are compared, Then export input, export output, and import output share one definition', () => {
    const exportInput = exportRecord.schemas.input.properties as Record<string, unknown>;
    const exportOutput = exportRecord.schemas.output.properties as Record<string, unknown>;
    const importOutput = importRecord.schemas.output.properties as Record<string, unknown>;

    for (const selector of SELECTORS) {
      expect(exportOutput[selector]).toEqual(exportInput[selector]);
      expect(importOutput[selector]).toEqual(exportInput[selector]);
    }
  });
});
