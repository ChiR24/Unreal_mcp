// Todo 9 (BB-005) lane 2 — diagnostics presenters source contracts.
//
// The enforceable RED gate for lane B. These assertions read the plugin/TS
// source text (what the compiler sees) so a claim that exists only in a
// comment, a plan, or a type cannot pass. They prove:
//   * both native builders call the frozen store's CurrentSummaryJson /
//     PreviousSummaryJson and project previousSession as JSON null when the
//     previous summary object is empty (NF-6 native/TS null parity),
//   * the TypeScript ResourceHandler awaits the frozen read-only reader and
//     attaches currentSession/previousSession to BOTH existing resource bodies,
//   * the reader surface stays read-only and the resource URI set is unchanged,
//   * the summary projection never carries secret/payload/path/idempotency/
//     raw-session-id fields.
// The PRESENTER behavior itself (real bodies over a temp Saved tree) is driven
// in src/handlers/resource-handlers.test.ts; this file only gates the source.

import { existsSync, readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const PLUGIN = resolve(process.cwd(), 'plugins/McpAutomationBridge/Source/McpAutomationBridge');
const BRIDGE_BUILDER = resolve(PLUGIN, 'Private/MCP/Resources/McpResourceBridgeContent.cpp');
const HEALTH_BUILDER = resolve(PLUGIN, 'Private/MCP/Resources/McpResourceHealthContent.cpp');
const HANDLERS = resolve(process.cwd(), 'src/handlers/resource-handlers.ts');
const READER = resolve(process.cwd(), 'src/automation/diagnostics-snapshot-reader.ts');
const TS_CATALOG = resolve(process.cwd(), 'src/resources/resource-catalog.ts');
const NATIVE_CATALOG = resolve(PLUGIN, 'Private/MCP/Resources/McpResourceCatalog.h');

function read(path: string): string {
  expect(existsSync(path), `missing file: ${path}`).toBe(true);
  return readFileSync(path, 'utf8');
}

/** Strip comments so a claim in prose cannot satisfy a code contract. */
function code(source: string): string {
  return source.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/[^\n]*/g, '');
}

// Field families the summary projection must never expose on either surface.
const FORBIDDEN_FIELD_CORPUS = [
  'payload',
  'capabilityToken',
  'idempotencyKey',
  'principalId',
  'sessionId',
  'password',
] as const;

describe('Todo 9 native diagnostics presenters attach allowlisted summaries', () => {
  it('both builders call the frozen store CurrentSummaryJson/PreviousSummaryJson', () => {
    for (const source of [read(BRIDGE_BUILDER), read(HEALTH_BUILDER)]) {
      expect(source).toContain('FMcpDiagnosticsSnapshot::Get().CurrentSummaryJson()');
      expect(source).toContain('FMcpDiagnosticsSnapshot::Get().PreviousSummaryJson()');
    }
  });

  it('projects previousSession as JSON null when the previous summary is empty, in BOTH builders (NF-6 parity)', () => {
    for (const source of [read(BRIDGE_BUILDER), read(HEALTH_BUILDER)]) {
      // Empty PreviousSummaryJson() -> Values.Num() == 0 -> SetField(null value).
      expect(source).toContain('Values.Num() == 0');
      expect(source).toContain('SetField(TEXT("previousSession")');
      expect(source).toContain('MakeShared<FJsonValueNull>()');
      expect(source).toContain('MakeShared<FJsonValueObject>(PreviousSummary)');
      // currentSession is always a non-empty object: plain SetObjectField, never a null branch.
      expect(source).toContain('SetObjectField(TEXT("currentSession")');
    }
  });

  it('never projects a secret/payload/path/idempotency/raw-session field into the bodies', () => {
    for (const source of [read(BRIDGE_BUILDER), read(HEALTH_BUILDER)]) {
      for (const field of FORBIDDEN_FIELD_CORPUS) {
        expect(source.includes(`TEXT("${field}")`), `${field} must not be emitted`).toBe(false);
      }
    }
  });
});

describe('Todo 9 TypeScript presenters attach the frozen reader summaries', () => {
  it('imports the read-only reader and the redacting AutomationLogger', () => {
    const source = read(HANDLERS);
    expect(source).toContain("import { AutomationLogger } from '../automation/log-redaction.js';");
    expect(source).toContain("import { readDiagnosticsSnapshots } from '../automation/diagnostics-snapshot-reader.js';");
    expect(source).toMatch(/new AutomationLogger\('DiagnosticsSnapshot'\)/);
  });

  it('awaits readDiagnosticsSnapshots and attaches both fields to the ue://health body', () => {
    const source = read(HANDLERS);
    expect(source).toContain('await readDiagnosticsSnapshots(');
    expect(source).toContain('currentSession: snapshots.current');
    expect(source).toContain('previousSession: snapshots.previous');
  });

  it('attaches both fields to the ue://automation-bridge body as well', () => {
    const source = read(HANDLERS);
    const branch = source.slice(source.indexOf("uri === 'ue://automation-bridge'"));
    expect(branch).toContain('await readDiagnosticsSnapshots(');
    expect(branch).toContain('currentSession: snapshots.current');
    expect(branch).toContain('previousSession: snapshots.previous');
  });

  it('keeps the reader read-only: no write export on the module surface', () => {
    const source = read(READER);
    expect(source).toContain('readDiagnosticsSnapshots');
    expect(source).not.toMatch(/export\s+(?:async\s+)?function\s+(?:write|create|append|save|put|delete|remove)/);
    expect(source).not.toContain('writeFile');
    expect(source).not.toContain('writeFileSync');
  });

  it('adds no new resource URI: the catalog URI set stays unchanged', () => {
    const tsCatalog = read(TS_CATALOG);
    const nativeCatalog = read(NATIVE_CATALOG);
    // The session summaries are presented INSIDE the existing ue://health and
    // ue://automation-bridge bodies; neither catalog adds a diagnostics URI.
    // Raw text is read here (not code()) because the C++ catalog spells the
    // health URI as TEXT("ue://health"), whose `//` a comment-stripper would cut.
    expect(tsCatalog).not.toMatch(/ue:\/\/diagnostics/);
    expect(tsCatalog).not.toMatch(/ue:\/\/sessions?/);
    expect(nativeCatalog).not.toMatch(/ue:\/\/diagnostics/);
    expect(nativeCatalog).not.toMatch(/ue:\/\/sessions?/);
    expect(nativeCatalog).toContain('TEXT("ue://health")');
  });

  it('does not project the forbidden field corpus in the reader allowlist', () => {
    const reader = code(read(READER));
    for (const field of FORBIDDEN_FIELD_CORPUS) {
      expect(reader.includes(field), `${field} must not be projected`).toBe(false);
    }
  });
});
