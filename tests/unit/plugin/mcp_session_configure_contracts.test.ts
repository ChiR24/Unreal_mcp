import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

// Source-contract lane for Task 36: the explicit-session, policy-bounded,
// revisioned configure overlay and its C1 catalog-revision read contract. Like
// the other plugin contract suites this reads C++/TS source text because no
// live-editor HTTP harness runs in CI; the serialized UE BuildPlugin gate remains
// the authoritative compile proof.

const root = process.cwd();
const nativeRoot = resolve(root, 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP');

const c1Header = readFileSync(resolve(nativeRoot, 'Primitives/IMcpCatalogRevisionReader.h'), 'utf8');
const storeHeader = readFileSync(resolve(nativeRoot, 'DynamicTools/McpSessionConfigureStore.h'), 'utf8');
const storeSource = readFileSync(resolve(nativeRoot, 'DynamicTools/McpSessionConfigureStore.cpp'), 'utf8');

const tsReader = readFileSync(resolve(root, 'src/server/mcp-primitives/catalog-revision-reader.ts'), 'utf8');
const tsStore = readFileSync(resolve(root, 'src/server/mcp-primitives/session-configure-store.ts'), 'utf8');
const tsSeam = readFileSync(resolve(root, 'src/server/tool-registry-manage-tools.ts'), 'utf8');

const countPureLines = (source: string): number =>
  source.split(/\r?\n/u).filter((line) => !/^\s*$/u.test(line) && !/^\s*(?:#|\/\/)/u.test(line)).length;

const sliceBetween = (source: string, start: string, end: string): string => {
  const from = source.indexOf(start);
  const to = source.indexOf(end, from + start.length);
  return source.slice(from, to === -1 ? undefined : to);
};

describe('Task 36 C1 catalog-revision contract mirrors on both surfaces', () => {
  it('declares an explicit-session read contract with no global fallback', () => {
    expect(c1Header).toContain('class IMcpCatalogRevisionReader');
    expect(c1Header).toContain('virtual uint64 GetCatalogStateRevision(const FString& SessionId) const = 0;');
    // No no-arg / global overload on either surface.
    expect(c1Header).not.toContain('GetCatalogStateRevision()');

    expect(tsReader).toContain('interface CatalogRevisionReader');
    expect(tsReader).toContain('getCatalogStateRevision(sessionId: string): number');
    expect(tsReader).not.toContain('getCatalogStateRevision()');
    expect(tsReader).not.toContain('sessionId?');
    expect(tsReader).toContain('BASELINE_CATALOG_STATE_REVISION');
  });
});

describe('Task 36 native session-configure store source contracts', () => {
  it('implements C1 as an independent per-session store', () => {
    expect(storeHeader).toContain('class FMcpSessionConfigureStore : public IMcpCatalogRevisionReader');
    expect(storeHeader).toContain('GetCatalogStateRevision(const FString& SessionId) const override');
    expect(storeHeader).toContain('#include "MCP/Primitives/IMcpCatalogRevisionReader.h"');
    // Keyed by explicit session id, with an explicit ClearSession.
    expect(storeHeader).toContain('TMap<FString, FOverlay> Overlays');
    for (const member of ['SeedFrom', 'HasSession', 'ClearSession', 'EnableTools', 'DisableTools', 'DisableCategory', 'Reset', 'SetLimit', 'SetPreference', 'IsToolEnabled', 'GetStatus']) {
      expect(storeHeader).toContain(member);
    }
    expect(storeSource).toContain('Overlays.Remove(SessionId)');
  });

  it('reproduces the protected-tool and protected-category invariants', () => {
    expect(storeSource).toContain('return Name == TEXT("manage_tools") || Name == TEXT("inspect");');
    expect(storeSource).toContain('return Name == TEXT("core");');
    expect(storeSource).toContain('if (IsProtectedTool(Name)) { Protected.Add(Name); continue; }');
  });

  it('advances the revision only on an effective visibility batch', () => {
    // Each visibility mutation bumps behind a fingerprint delta guard.
    expect(storeSource).toContain('if (Fingerprint(Overlay) != Before) ++Overlay.CatalogStateRevision;');
    const bumps = storeSource.match(/\+\+Overlay\.CatalogStateRevision;/g) ?? [];
    expect(bumps).toHaveLength(4); // EnableTools, DisableTools, DisableCategory, Reset
    // Bounded limit/preference changes are non-visibility: they never bump.
    const setLimit = sliceBetween(storeSource, '::SetLimit(', '::SetPreference(');
    const setPreference = sliceBetween(storeSource, '::SetPreference(', '::IsToolEnabled(');
    expect(setLimit).not.toContain('CatalogStateRevision');
    expect(setPreference).not.toContain('CatalogStateRevision');
  });

  it('keeps limits policy-bounded', () => {
    expect(storeSource).toContain('FMath::Clamp(Value, Min, Max)');
    expect(storeSource).toContain('TEXT("maxResults")');
    expect(storeSource).toContain('MaxPreferenceValueLength');
  });

  it('stays standalone: no global manager, transport, list-changed, generated fingerprint, or unsafe save', () => {
    for (const source of [storeHeader, storeSource, c1Header]) {
      for (const forbidden of [
        'FMcpDynamicToolManager',
        'McpNativeTransport',
        'Subsystem',
        'notifications/tools/list_changed',
        'McpGeneratedCapabilityShards',
        'UPackage::SavePackage',
        'C:\\',
        '/home/',
        '.uproject',
      ]) {
        expect(source).not.toContain(forbidden);
      }
    }
  });

  it('keeps each native file within the 250 pure-line ceiling', () => {
    for (const source of [c1Header, storeHeader, storeSource]) {
      expect(countPureLines(source)).toBeLessThanOrEqual(250);
    }
  });
});

describe('Task 36 TypeScript store + manage-tools seam contracts', () => {
  it('implements C1 with per-session overlays over the shared immutable ops', () => {
    expect(tsStore).toContain('class SessionConfigureStore implements CatalogRevisionReader');
    expect(tsStore).toContain('getCatalogStateRevision(sessionId: string): number');
    expect(tsStore).toContain('clearSession(sessionId: string): boolean');
    // Reuses the global manager's pure protected/no-op operations, never a copy.
    for (const op of ['disableToolStates', 'disableCategoryState', 'enableCategoryState', 'resetToolStates']) {
      expect(tsStore).toContain(op);
    }
    // Same revision discipline as the global manager: fingerprint delta guard.
    expect(tsStore).toContain('overlay.catalogStateRevision++');
    expect(tsStore).toContain('LIMIT_BOUNDS');
    expect(tsStore).toContain('Math.min');
    // Limit/preference setters are non-visibility: neither goes through applyMutation.
    const setLimit = sliceBetween(tsStore, 'setLimit(', 'setPreference(');
    const setPreference = sliceBetween(tsStore, 'setPreference(', 'isToolEnabled(');
    expect(setLimit).not.toContain('applyMutation');
    expect(setPreference).not.toContain('applyMutation');
  });

  it('threads the session via an injected seam, defaulting stdio to the untouched global manager', () => {
    expect(tsSeam).toContain('export const STDIO_SESSION_ID');
    for (const member of ['setManageToolsSessionResolver', 'resetManageToolsSessionResolver', 'clearManageToolsSession', 'sessionConfigureStore']) {
      expect(tsSeam).toContain(member);
    }
    // The default (stdio) path stays on the global manager; only injected
    // sessions reach the store.
    expect(tsSeam).toContain('if (sessionId === STDIO_SESSION_ID)');
    expect(tsSeam).toContain('return dynamicToolManager;');
  });
});
