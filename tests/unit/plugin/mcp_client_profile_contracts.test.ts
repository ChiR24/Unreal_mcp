import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const root = process.cwd();
const nativeRoot = resolve(root, 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP');

const profileHeader = readFileSync(resolve(nativeRoot, 'Primitives/McpSessionCapabilityProfile.h'), 'utf8');
const storeHeader = readFileSync(resolve(nativeRoot, 'Primitives/McpClientProfileStore.h'), 'utf8');
const storeSource = readFileSync(resolve(nativeRoot, 'Primitives/McpClientProfileStore.cpp'), 'utf8');

const tsProfile = readFileSync(resolve(root, 'src/server/mcp-primitives/session-capability-profile.ts'), 'utf8');
const tsFallback = readFileSync(resolve(root, 'src/server/mcp-primitives/fallback-pointers.ts'), 'utf8');
const tsStore = readFileSync(resolve(root, 'src/server/mcp-primitives/client-profile-store.ts'), 'utf8');
const tsElicitation = readFileSync(resolve(root, 'src/server/tool-registry-elicitation.ts'), 'utf8');

const NATIVE_BOOLS = ['bHasResources', 'bHasPrompts', 'bHasCompletions', 'bHasSubscriptions', 'bHasElicitation', 'bHasTasks'];
const TS_BOOLS = ['hasResources', 'hasPrompts', 'hasCompletions', 'hasSubscriptions', 'hasElicitation', 'hasTasks'];
const CAPABILITY_KEYS = ['resources', 'prompts', 'completions', 'subscriptions', 'elicitation', 'tasks', 'experimental'];
const NATIVE_METHODS = ['resources/list', 'prompts/list', 'completion/complete', 'resources/subscribe', 'tasks/list'];

const countPureLines = (source: string): number =>
  source.split(/\r?\n/u).filter((line) => !/^\s*$/u.test(line) && !/^\s*(?:#|\/\/)/u.test(line)).length;

describe('mcp client-profile C3 source contracts', () => {
  it('mirrors the six structural capability booleans on both surfaces', () => {
    for (const bool of NATIVE_BOOLS) expect(profileHeader).toContain(bool);
    for (const bool of TS_BOOLS) expect(tsProfile).toContain(bool);
  });

  it('parses capabilities structurally and never a client name or version', () => {
    for (const key of CAPABILITY_KEYS) expect(profileHeader).toContain(`TEXT("${key}")`);
    // The parse must not read brand/identity fields on either surface.
    for (const forbidden of ['clientInfo', 'ClientName', 'ClientVersion', 'TEXT("name")', 'TEXT("version")']) {
      expect(profileHeader).not.toContain(forbidden);
    }
    for (const forbidden of ['clientInfo', 'mcpClients', 'KNOWN_DYNAMIC_CLIENT_NAMES', 'getClientVersion', '.title']) {
      expect(tsProfile).not.toContain(forbidden);
    }
  });

  it('mirrors the bounded fallback pointer references on both surfaces', () => {
    for (const method of NATIVE_METHODS) {
      expect(profileHeader).toContain(method);
      expect(tsFallback).toContain(method);
    }
    for (const op of ['search', 'describe', 'execute']) {
      expect(profileHeader).toContain(op);
      expect(tsFallback).toContain(op);
    }
    expect(profileHeader).toContain('McpFallbackPointerFor');
  });

  it('keeps the native store standalone with an explicit ClearSession', () => {
    expect(storeHeader).toContain('class FMcpClientProfileStore');
    for (const member of ['SetSession', 'GetSession', 'HasSession', 'ClearSession']) {
      expect(storeHeader).toContain(member);
    }
    expect(storeHeader).toContain('TMap<FString, FMcpSessionCapabilityProfile>');
    expect(storeSource).toContain('Profiles.Remove');
    // Standalone: it must not reach into transport/session lifecycle objects.
    for (const forbidden of ['McpNativeTransport', 'McpNativeSession', 'McpSession.h', 'Subsystem']) {
      expect(storeHeader).not.toContain(forbidden);
      expect(storeSource).not.toContain(forbidden);
    }
  });

  it('mirrors explicit-session store semantics in TypeScript', () => {
    for (const member of ['setSession', 'getSession', 'clearSession']) {
      expect(tsStore).toContain(member);
    }
  });

  it('keeps the safe elicitation policy present in TypeScript', () => {
    for (const symbol of ['isSafeToElicit', 'elicitHighImpactConsent', 'collectSafeElicitableProps', 'consent']) {
      expect(tsElicitation).toContain(symbol);
    }
  });

  it('never emits a host path, project env, or unsafe save in native metadata', () => {
    for (const source of [profileHeader, storeHeader, storeSource]) {
      for (const forbidden of ['C:\\', '/home/', '/Users/', '.uproject', 'UE_PROJECT_PATH', 'UPackage::SavePackage']) {
        expect(source).not.toContain(forbidden);
      }
    }
  });

  it('keeps each native file within the 250 pure-line ceiling', () => {
    for (const source of [profileHeader, storeHeader, storeSource]) {
      expect(countPureLines(source)).toBeLessThanOrEqual(250);
    }
  });
});
