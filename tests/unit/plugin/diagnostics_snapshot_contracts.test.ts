// Todo 9 (BB-005) lane 1 — diagnostics snapshot store source contracts.
//
// These assertions read the plugin source text (what the compiler sees) so a
// claim that exists only in a comment, a plan, or a TypeScript type cannot
// pass. The lane scope is the FOUNDATION STORE + TS READER only: queue,
// response, handshake, session, and presenter HOOK WIRING is deliberately a
// second lane and is NOT asserted here. Startup rotation is asserted as a
// store API (`RotateOnStartup`) and as store-internal promotion logic, not as
// a call site in `McpAutomationBridgeSubsystemLifecycle.cpp`.

import { existsSync, readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const PLUGIN = resolve(process.cwd(), 'plugins/McpAutomationBridge/Source/McpAutomationBridge');
const STORE_HEADER = resolve(PLUGIN, 'Private/Foundation/Diagnostics/McpDiagnosticsSnapshot.h');
const STORE_SOURCE = resolve(PLUGIN, 'Private/Foundation/Diagnostics/McpDiagnosticsSnapshot.cpp');
const STORE_LOAD = resolve(PLUGIN, 'Private/Foundation/Diagnostics/McpDiagnosticsSnapshotLoad.cpp');
const STORE_ROTATION = resolve(PLUGIN, 'Private/Foundation/Diagnostics/McpDiagnosticsSnapshotRotation.cpp');
const STORE_SCHEMA = resolve(PLUGIN, 'Private/Foundation/Diagnostics/McpDiagnosticsSnapshotSchema.h');
const NATIVE_TESTS = resolve(PLUGIN, 'Private/Tests/Diagnostics/McpDiagnosticsSnapshotTests.cpp');
const READER = resolve(process.cwd(), 'src/automation/diagnostics-snapshot-reader.ts');

const CANONICAL_PARENTS = [
  'manage_tools',
  'manage_asset',
  'manage_blueprint',
  'control_actor',
  'control_editor',
  'manage_level',
  'system_control',
  'inspect',
  'build_environment',
  'manage_level_structure',
  'manage_geometry',
  'manage_pcg',
  'animation_physics',
  'manage_effect',
  'manage_gas',
  'manage_character',
  'manage_combat',
  'manage_ai',
  'manage_inventory',
  'manage_interaction',
  'manage_sequence',
  'manage_audio',
  'manage_networking',
] as const;

const REFUSAL_CODES = [
  'SCOPE_NOT_GRANTED',
  'CONSENT_REQUIRED',
  'PATH_NOT_PERMITTED',
  'PROJECT_NOT_PERMITTED',
  'QUOTA_EXCEEDED',
  'COMMAND_BLOCKED',
] as const;

function read(path: string): string {
  expect(existsSync(path), `missing file: ${path}`).toBe(true);
  return readFileSync(path, 'utf8');
}

/** Strip comments so a claim in prose cannot satisfy a code contract. */
function code(source: string): string {
  return source.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/[^\n]*/g, '');
}

describe('Todo 9 diagnostics store ships a bounded crash-tolerant Foundation singleton', () => {
  it('ships a proper .h/.cpp singleton under Foundation/Diagnostics', () => {
    const header = code(read(STORE_HEADER));
    expect(header).toContain('class FMcpDiagnosticsSnapshot');
    expect(header).toContain('static FMcpDiagnosticsSnapshot& Get()');

    const source = code(read(STORE_SOURCE));
    // Function-local static in the .cpp — never header-local mutable state.
    expect(source).toMatch(/static FMcpDiagnosticsSnapshot \w+;/);
  });

  it('uses exactly one mutex and the deterministic clock/root/reset seams', () => {
    const header = code(read(STORE_HEADER));
    expect(header.match(/FCriticalSection/g)?.length ?? 0).toBe(1);
    expect(header).toMatch(/SetClock|TFunction<double\(\)>/);
    expect(header).toContain('SetRootOverride');
    expect(header).toContain('Reset()');

    const source = code(read(STORE_SOURCE));
    expect(source).toContain('FScopeLock');
  });

  it('exposes only typed recorders — no generic key/value setter', () => {
    const header = code(read(STORE_HEADER));
    for (const recorder of [
      'RecordAdmission',
      'RecordPreDispatch',
      'RecordRefusal',
      'RecordTerminal',
      'RecordHandshake',
      'RecordDisconnect',
      'RecordSessionCreated',
      'RecordSessionClosed',
    ]) {
      expect(header).toContain(recorder);
    }
    expect(header).not.toMatch(/Set(?:Field|Value|String|Int|Number|Bool|Array|Object|Map|Key)\s*\(/);
  });

  it('caps every on-disk snapshot at 64 KiB', () => {
    const schema = code(read(STORE_SCHEMA));
    expect(schema).toMatch(/MaxSnapshotBytes\s*=\s*64\s*\*\s*1024|MaxSnapshotBytes\s*=\s*65536/);
  });

  it('writes only through fixed same-directory temp + rename, never directly', () => {
    const source = code(read(STORE_SOURCE));
    expect(source).toContain('current-session.json.tmp');
    expect(source).toContain('previous-session.json.tmp');
    expect(source).toContain('MoveFile');
    // No quarantine/GUID temp names, no accumulate-by-number previous files.
    expect(source).not.toContain('FGuid');
    expect(source).not.toMatch(/previous[_-]?\d+\.json/);
  });

  it('rotates current to exactly one previous on startup, promoting only non-empty sessions', () => {
    const header = code(read(STORE_HEADER));
    expect(header).toContain('RotateOnStartup');

    const source = code(read(STORE_SOURCE)) + code(read(STORE_ROTATION));
    // Promotion must be gated on at least one recorded event so a commandlet or
    // second restart cannot overwrite previous crash evidence with an empty run.
    expect(source).toMatch(/HasEvents|HasRecordedEvent|HasAnyEvent/);
    expect(source).toContain('previous-session.json');
    // Missing current after successful promotion is a valid recoverable state.
    expect(source).toMatch(/recover|Recover/);
  });

  it('ignores corrupt/oversized files with one bounded typed warning naming the path only', () => {
    const source = read(STORE_LOAD);
    expect(source).toContain('UE_LOG');
    expect(source).toContain('Warning');
    // One-shot guard so rotation retries never flood the log.
    expect(source).toMatch(/bWarned|WarnedOnce|Once/);
  });

  it('records a truncated SHA-256 session identity and never a raw session id', () => {
    const header = code(read(STORE_HEADER));
    expect(header).toContain('RecordSessionCreated');
    const source = code(read(STORE_SOURCE));
    expect(source).toContain('FSHA256');
    expect(source).toContain('LastIdentitySha256');
    expect(source).not.toContain('SessionId');
    expect(source).not.toContain('TEXT("sessionId")');
  });

  it('clamps canonical actions and terminal classes to strict allowlists', () => {
    const schema = code(read(STORE_SCHEMA));
    expect(schema).toContain('non_canonical');
    expect(schema).toMatch(/CoerceCanonicalAction|NormalizeCanonicalAction/);
    expect(schema).toMatch(/CoerceTerminalClass|NormalizeTerminalClass/);

    for (const parent of CANONICAL_PARENTS) {
      expect(schema).toContain(parent);
    }
    for (const refusal of REFUSAL_CODES) {
      expect(schema).toContain(refusal);
    }
    expect(schema).toContain('WebSocket');
    expect(schema).toContain('NativeHTTP');
  });

  it('excludes the secret/payload/path/idempotency corpus from the on-disk schema', () => {
    const combined = code(read(STORE_SCHEMA)) + code(read(STORE_SOURCE)) + code(read(STORE_HEADER));
    for (const forbidden of [
      'TEXT("payload")',
      'TEXT("capabilityToken")',
      'TEXT("idempotencyKey")',
      'TEXT("principalId")',
      'TEXT("sessionId")',
      'TEXT("token")',
    ]) {
      expect(combined.includes(forbidden), `forbidden on-disk field: ${forbidden}`).toBe(false);
    }
    // Terminal hooks carry a bounded code, never a message.
    expect(combined).not.toContain('TerminalMessage');
    expect(combined).not.toContain('ErrorMessage');
  });

  it('ships the bounded field corpus the reader projects', () => {
    const combined = code(read(STORE_SCHEMA)) + code(read(STORE_SOURCE));
    for (const field of [
      'schemaVersion',
      'canonicalAction',
      'queueDepth',
      'terminalClass',
      'lastIdentitySha256',
      'instanceId',
    ]) {
      expect(combined).toContain(`TEXT("${field}")`);
    }
  });

  it('ships a native automation test under Private/Tests/Diagnostics', () => {
    const tests = code(read(NATIVE_TESTS));
    expect(tests).toContain('IMPLEMENT_SIMPLE_AUTOMATION_TEST');
    expect(tests).toContain('SetRootOverride');
    expect(tests).toContain('SetClock');
    expect(tests).toContain('RotateOnStartup');
  });
});

describe('Todo 9 TypeScript reader is read-only', () => {
  it('ships a reader that parses current/previous without any write export', () => {
    const source = read(READER);
    expect(source).toContain('readDiagnosticsSnapshots');
    expect(source).toContain('MAX_SNAPSHOT_BYTES');
    expect(source).not.toMatch(/export\s+(?:async\s+)?function\s+(?:write|create|append|save|put|delete|remove)/);
    expect(source).not.toContain('writeFile');
    expect(source).not.toContain('writeFileSync');
  });
});
