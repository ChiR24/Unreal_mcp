/**
 * Source contracts for the asynchronous Fab add and its status read.
 *
 * The add used to wait for the import. That import takes minutes and holds the game thread, so the
 * client gave up at 30 seconds and every other call answered EDITOR_BLOCKED while it ran. It now
 * answers when Fab accepts the download and keeps the outcome for a status read. What must stay true:
 * nobody waits on the import, the store is the one place a running import is known, and nothing a
 * reply carries can be a credential.
 */

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';

const plugin = resolve(process.cwd(), 'plugins/McpAutomationBridge/Source');
const fab = (file: string): string =>
  readFileSync(resolve(plugin, 'McpAutomationBridgeFab/Private', file), 'utf8');
const core = (file: string): string => readFileSync(resolve(plugin, 'McpAutomationBridge', file), 'utf8');

/** Comment bodies explain the rules, so rule checks ignore them. */
const code = (text: string): string =>
  text.replace(/\/\*[\s\S]*?\*\//gu, '').replace(/^[ \t]*\/\/.*$/gmu, '');

describe('the Fab add answers when Fab accepts, not when the import ends', () => {
  const add = code(fab('McpFabAddOperation.cpp'));

  it('starts the watcher and replies in the same step, without waiting on it', () => {
    const watch = add.indexOf('McpFabImportWatcher::WatchForImport(');
    const reply = add.indexOf('OnAccepted(Result);', watch);
    expect(watch, 'the accepted path must start the watcher').toBeGreaterThan(-1);
    expect(reply, 'and reply right after it').toBeGreaterThan(watch);
  });

  it('gives the watcher no completion callback for the caller to wait on', () => {
    const header = code(fab('McpFabImportWatcher.h'));
    expect(header).toMatch(/void WatchForImport\(/u);
    expect(header).not.toMatch(/TFunction/u);
    // The outcome goes to the store instead.
    expect(code(fab('McpFabImportWatcher.cpp'))).toContain('McpFabImportOperations::Finish(OperationId, Accepted);');
  });

  it('refuses a second import while one runs, before it registers another', () => {
    const running = add.indexOf('McpFabImportOperations::FindRunning(');
    const begin = add.indexOf('McpFabImportOperations::Begin(');
    expect(running).toBeGreaterThan(-1);
    expect(begin).toBeGreaterThan(running);
    expect(add).toContain('ALREADY_IN_FLIGHT');
  });

  it('names the import in the way and the call that reads it', () => {
    expect(add).toMatch(/Poll asset\.query_marketplace with lookup=fab_import_status and operationId/u);
    const handler = code(core('Private/Domains/AssetWorkflow/Operations/McpAutomationBridge_AssetWorkflowFabAdd.cpp'));
    expect(handler).toMatch(/ALREADY_IN_FLIGHT[\s\S]*MakeStatusNextCall\(Result\.OperationId\)/u);
    const json = code(core('Private/Domains/AssetWorkflow/Fab/McpAutomationBridge_FabImportJson.h'));
    expect(json).toContain('TEXT("fab_import_status")');
    expect(json).toContain('TEXT("query_marketplace")');
  });

  it('points at a relocation capability that exists', () => {
    const handler = core('Private/Domains/AssetWorkflow/Operations/McpAutomationBridge_AssetWorkflowFabAdd.cpp');
    expect(handler).not.toContain('asset.migrate_assets');
    expect(handler).toContain('asset.move');
  });
});

describe('the Fab import status read', () => {
  const status = code(core('Private/Domains/AssetWorkflow/Fab/McpAutomationBridge_AssetWorkflowFabImportStatus.cpp'));

  it('is served from the operation store, never from the page', () => {
    expect(status).toContain('Provider->GetImportStatus(');
    expect(status).not.toMatch(/ExecuteJavascript|McpFabBridgeDispatch|BindUObject/u);
  });

  it('only looks up a plain id, so nothing path-shaped is ever matched', () => {
    expect(status).toMatch(/IsPlainKey\(Key\)/u);
    expect(status).toMatch(/FChar::IsAlnum\(Ch\)/u);
  });

  it('is dispatched and declared like every other asset action', () => {
    expect(code(core('Private/Domains/AssetWorkflow/McpAutomationBridge_AssetWorkflowHandlers.cpp')))
      .toContain('get_fab_import_status');
    expect(core('Public/McpAutomationBridgeSubsystemAssetWorkflowDeclarations.h')).toContain('HandleGetFabImportStatus');
  });
});

describe('what an import record can carry', () => {
  const provider = code(core('Public/McpFabProvider.h'));
  for (const struct of ['FMcpFabAddResult', 'FMcpFabImportStatus']) {
    it(`${struct} has no credential-shaped field`, () => {
      const body = new RegExp(`struct ${struct}\\s*\\{([\\s\\S]*?)\\n\\};`, 'u').exec(provider)?.[1] ?? '';
      expect(body, `${struct} must exist`).not.toBe('');
      for (const banned of ['Url', 'Token', 'Cookie', 'Secret', 'Credential']) {
        expect(body).not.toMatch(new RegExp(`\\b\\w+\\s+\\w*${banned}\\w*\\s*(?:=|;)`, 'iu'));
      }
    });
  }
});

describe('the Fab log capture', () => {
  const capture = code(fab('McpFabLogCapture.cpp'));

  it('keeps only LogFab errors, and masks a line before it is stored', () => {
    expect(capture).toContain('TEXT("LogFab")');
    expect(capture).toMatch(/> ELogVerbosity::Error/u);
    // Scrub runs on the way into the buffer, so an unmasked line never exists to be read.
    expect(capture).toMatch(/Lines\.Add\(Scrub\(Message\)\)/u);
    expect(capture).toContain('TEXT("://")');
    expect(capture).toMatch(/TEXT\("token"\)/u);
  });

  it('never writes what it captured to the log', () => {
    expect(capture).not.toMatch(/UE_LOG/u);
  });

  it('is stopped when the module shuts down, so no device outlives it', () => {
    expect(code(fab('McpAutomationBridgeFabModule.cpp'))).toContain('McpFabLogCapture::Stop();');
  });
});
