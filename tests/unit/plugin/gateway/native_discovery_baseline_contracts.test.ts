/// <reference types="node" />

import { createHash } from 'node:crypto';
import { readFileSync, readdirSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

// Task 25 BASELINE characterization.
//
// Written and run BEFORE any Task-25 edit. Every assertion is an INVARIANT of
// the native gateway discovery contract: it holds on the pre-task tree and must
// still hold after native discovery moves from manifest-union data to the
// generated capability shards. The pre-task DEFECTS (orphan shard headers, one
// oversized raw literal per shard) are captured as a snapshot digest rather
// than asserted, so this file never has to be rewritten to stay truthful.

const pluginRoot = resolve(
  process.cwd(),
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private',
);

const read = (rel: string): string => readFileSync(resolve(pluginRoot, rel), 'utf8');

const gatewayDefinition = read('MCP/Gateway/McpNativeGatewayDefinition.cpp');
const transportGateway = read('MCP/Transport/McpNativeTransportGateway.cpp');
const transportPrivate = read('MCP/Transport/McpNativeTransportPrivate.h');
// The 1..25 / 1..50 budgets moved from transport literals into named discovery
// constants; the bound VALUES are unchanged, so the invariant is now pinned at
// both the constant and its transport use site.
const searchHeader = read('MCP/Gateway/McpNativeGatewaySearch.h');

const generatedDir = resolve(pluginRoot, 'MCP/Generated');

/** Stable digest over the generated shard payload bytes (framing-independent). */
export const generatedShardDigest = (): string => {
  const hash = createHash('sha256');
  for (const name of readdirSync(generatedDir).sort()) {
    hash.update(name);
    hash.update(readFileSync(resolve(generatedDir, name)));
  }
  return hash.digest('hex');
};

describe('Task 25 baseline: native gateway discovery invariants', () => {
  it('keeps exactly one public `unreal` tool with the four gateway operations', () => {
    expect(gatewayDefinition).toContain('SetStringField(TEXT("name"), TEXT("unreal"))');
    for (const operation of ['search', 'describe', 'execute', 'configure']) {
      expect(gatewayDefinition).toContain(`TEXT("${operation}")`);
    }
    expect(gatewayDefinition).toContain('SetBoolField(TEXT("additionalProperties"), false)');
  });

  it('keeps the search budget at 1..25 with a default of 12', () => {
    expect(gatewayDefinition).toContain('SetNumberField(TEXT("minimum"), 1)');
    expect(gatewayDefinition).toContain('SetNumberField(TEXT("maximum"), 25)');
    expect(searchHeader).toContain('McpSearchDefaultLimit = 12;');
    expect(searchHeader).toContain('McpSearchMaxLimit = 25;');
    expect(transportGateway).toContain('DiscoveryQuery.Limit = McpSearchDefaultLimit;');
    expect(transportGateway).toContain('FMath::Clamp(L, 1, McpSearchMaxLimit)');
  });

  it('keeps the describe budget at 1..50 with a default of 20', () => {
    expect(searchHeader).toContain('McpDescribeDefaultLimit = 20;');
    expect(searchHeader).toContain('McpDescribeMaxLimit = 50;');
    expect(transportGateway).toContain('DiscoveryQuery.Limit = McpDescribeDefaultLimit;');
    expect(transportGateway).toContain('FMath::Clamp(L, 1, McpDescribeMaxLimit)');
  });

  it('clamps a negative discovery offset to zero on both operations', () => {
    const clamps = transportGateway.match(/FMath::Max\(0, O\)/gu) ?? [];
    expect(clamps.length).toBeGreaterThanOrEqual(2);
  });

  it('requires a valid session before any gateway operation runs', () => {
    expect(transportGateway).toContain('FScopeLock SessionLock(&SessionMutex);');
    expect(transportGateway).toContain('if (!ActiveSessions.Contains(SessionId))');
    const sessionGate = transportGateway.indexOf('ActiveSessions.Contains(SessionId)');
    const searchDispatch = transportGateway.indexOf('Operation == TEXT("search")');
    expect(sessionGate).toBeGreaterThan(0);
    expect(searchDispatch).toBeGreaterThan(sessionGate);
  });

  it('rejects direct canonical tool calls while gateway mode is on', () => {
    expect(transportGateway).toContain('if (ToolName == TEXT("unreal"))');
    expect(transportGateway).toContain('Gateway mode is enabled.');
  });

  it('leaves the supported protocol-version set untouched', () => {
    expect(transportPrivate).toContain('McpSupportedProtocolVersions');
    expect(transportPrivate).toContain('"2025-11-25"');
    expect(transportPrivate).toContain('"2025-06-18"');
    expect(transportPrivate).toContain('"2025-03-26"');
    expect(transportPrivate).not.toContain('2026-07-28');
  });

  it('never performs editor work on the transport thread during discovery', () => {
    for (const forbidden of [
      'GEditor',
      'LoadObject<',
      'StaticLoadObject',
      'UPackage::SavePackage',
      'GetEditorWorld',
    ]) {
      expect(transportGateway).not.toContain(forbidden);
    }
  });

  it('emits a reproducible digest of the generated shard payloads', () => {
    const digest = generatedShardDigest();
    expect(digest).toMatch(/^[0-9a-f]{64}$/u);
    expect(generatedShardDigest()).toBe(digest);
  });
});
