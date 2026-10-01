/**
 * Source contracts for the download percent and the cancel of a Fab import.
 *
 * Fab keeps its download request private to the workflow, so the only places a download's progress and a
 * pack's Cancel can be reached from outside are its own notification and Interchange's own manager. What
 * must stay true: the percent is read from the notification and nothing else, a cancel is only ever the
 * click Fab's button already handles or the manager's own cancel, and a cancelled import ends through the
 * one watcher that ends every import.
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

describe('the Fab download notification reader', () => {
  const progress = code(fab('Import/McpFabDownloadProgress.cpp'));

  it('reads Slate widgets and nothing else: no page, no script, no credential', () => {
    expect(progress).toContain('FSlateNotificationManager::Get().GetWindows(Windows);');
    for (const banned of ['ExecuteJavascript', 'BindUObject', 'McpFabBridgeDispatch', 'cookie', 'csrf', 'Token', 'FHttpModule']) {
      expect(progress, `${banned} has no place in a widget reader`).not.toContain(banned);
    }
  });

  it('finds the toast by the exact title Fab gives it, so two downloads cannot be mixed up', () => {
    expect(progress).toContain('const FString Title = TEXT("Downloading ") + AssetName;');
    expect(progress).toMatch(/Text->GetText\(\)\.ToString\(\) == Title/u);
  });

  it('never changes what it reads', () => {
    for (const banned of ['SetText', 'SetEnabled', 'SetVisibility', 'SetPercent', 'SetColor']) {
      expect(progress).not.toContain(banned);
    }
  });

  it('presses only a Cancel button that is enabled, and only by the click Fab already handles', () => {
    const press = progress.slice(progress.indexOf('bool PressCancel('));
    expect(press).toContain('Toast.Cancel->IsEnabled()');
    expect(press).toContain('Toast.Cancel->SimulateClick();');
    // Fab builds the button of a source-format download disabled, so reading it as cancellable is what
    // keeps a cancel from being claimed for a download that has none.
    expect(progress).toContain('Result.bCancellable = Toast.Cancel.IsValid() && Toast.Cancel->IsEnabled();');
  });

  it('is bounded, so a hostile or huge widget tree cannot hold the editor', () => {
    expect(progress).toMatch(/constexpr int32 MaxDepth = \d+;/u);
    expect(progress).toMatch(/constexpr int32 MaxWidgets = \d+;/u);
  });
});

describe('cancelling a Fab import', () => {
  const cancel = code(fab('Import/McpFabImportCancel.cpp'));

  it('uses only the three handles Fab and the engine give', () => {
    expect(cancel).toContain('McpFabDownloadProgress::PressCancel(Name)');
    expect(cancel).toContain('McpFabInterchange::CancelTasks()');
    expect(cancel).toContain('case ECancelRoute::DropQueued:');
    // Fab's request objects stay Fab's: nothing here reaches into its downloader or its page.
    for (const banned of ['FFabDownloadRequest', 'FFabDownloadQueue', 'FabDownloader', 'ExecuteJavascript', 'McpFabBridgeDispatch', 'IFileManager', 'Delete']) {
      expect(cancel, `${banned} is not a way to cancel`).not.toContain(banned);
    }
  });

  it('drops a queued add by finishing it as CANCELLED, which also forgets its start closure', () => {
    expect(cancel).toMatch(/Outcome\.ErrorCode = TEXT\("CANCELLED"\);[\s\S]*Finish\(OperationId, Outcome\);/u);
    const store = code(fab('Import/McpFabImportOperations.cpp'));
    const finish = store.slice(store.indexOf('void Finish('), store.indexOf('bool IsCancelRequested('));
    expect(finish).toContain('Op->Launch = nullptr;');
  });

  it('refuses what has no handle, and says why', () => {
    for (const refusal of ['NOT_FOUND', 'ALREADY_FINISHED', 'NOT_CANCELLABLE']) {
      expect(cancel).toContain(`TEXT("${refusal}")`);
    }
    // An add that is still being put to Fab cannot be interrupted, so it is refused rather than half-stopped.
    expect(cancel).toMatch(/Op->State == EState::Resolving[\s\S]*NOT_CANCELLABLE/u);
    // Only a unreal-engine pack's download has a Cancel; nothing claims one for a source format.
    expect(cancel).toMatch(/Toast\.bFound[\s\S]*has no Cancel/u);
  });

  it('is idempotent: a second cancel of the same import is not an error', () => {
    expect(cancel).toMatch(/if \(Op->bCancelRequested\)\s*\{[^}]*return true;/u);
  });

  it('only marks a running import cancelled when the handle actually took', () => {
    expect(cancel).toMatch(/if \(McpFabDownloadProgress::PressCancel\(Name\)\)\s*\{\s*Op->bCancelRequested = true;/u);
    expect(cancel).toMatch(/if \(McpFabInterchange::CancelTasks\(\)\)\s*\{\s*Op->bCancelRequested = true;/u);
  });

  it('judges a pack download by the toast and an import by Interchange, never the other way round', () => {
    const route = cancel.slice(cancel.indexOf('ECancelRoute RouteFor('), cancel.indexOf('bool RequestCancel('));
    expect(route).toMatch(/Op\.AssetsSoFar == 0 && Toast\.bCancellable/u);
    expect(route).toContain('McpFabInterchange::IsActive()');
  });
});

describe('Interchange is cancelled by reflection', () => {
  const guard = code(fab('Import/McpFabInterchange.cpp'));

  it('asks the manager through its own BlueprintCallable functions, linking nothing', () => {
    expect(guard).toContain('GetInterchangeManagerScripted');
    expect(guard).toContain('TEXT("CancelAllTasks")');
    expect(guard).toContain('TEXT("IsInterchangeActive")');
    expect(guard).not.toMatch(/#include\s+"Interchange/u);
    expect(readFileSync(resolve(plugin, 'McpAutomationBridgeFab/McpAutomationBridgeFab.Build.cs'), 'utf8'))
      .not.toMatch(/Interchange/u);
  });

  it('does nothing when this engine has no such manager', () => {
    expect(guard).toMatch(/CancelAll == nullptr\)\s*\{\s*return false;/u);
    expect(guard).toMatch(/Query == nullptr\)\s*\{\s*return false;/u);
  });
});

describe('a cancelled import ends through the watcher that ends every import', () => {
  const watcher = code(fab('Import/McpFabImportWatcher.cpp'));

  it('reads the cancel on its tick and ends as CANCELLED', () => {
    expect(watcher).toContain('const bool bCancelled = McpFabImportOperations::IsCancelRequested(OperationId);');
    expect(watcher).toMatch(/!bSettled && !bExpired && !bFabFailed && !bCancelled/u);
    expect(watcher).toMatch(/if \(bCancelled\)\s*\{[\s\S]*?ErrorCode = TEXT\("CANCELLED"\);/u);
  });

  it('puts the cancel before what Fab logs as it gives up, so a cancel is not reported as a Fab failure', () => {
    const branches = watcher.slice(watcher.indexOf('if (bCancelled)'));
    expect(branches.indexOf('CANCELLED')).toBeLessThan(branches.indexOf('FAB_IMPORT_FAILED'));
  });

  it('neither saves nor moves what a cancelled import left', () => {
    expect(watcher).toContain('if (PostImport && Count > 0 && !bCancelled)');
  });
});

describe('what the status read says about downloads and cancels', () => {
  const describeSrc = code(fab('Import/McpFabImportDescribe.cpp'));
  const status = code(core('Private/Domains/AssetWorkflow/Fab/McpAutomationBridge_AssetWorkflowFabImportStatus.cpp'));

  it('reads the percent only while the import is still downloading', () => {
    expect(describeSrc).toMatch(/Op\.State == EState::Active && Out\.Phase == TEXT\("downloading"\)/u);
    expect(describeSrc).toContain('McpFabDownloadProgress::Read(ToastName(Op))');
  });

  it('calls an import cancelling from the moment the cancel is accepted', () => {
    expect(describeSrc).toMatch(/IsOpen\(Op\) && Op\.bCancelRequested\)\s*\{\s*Out\.Phase = TEXT\("cancelling"\);/u);
  });

  it('reports a percent only when there is one, and cancellable only while the import is open', () => {
    expect(status).toMatch(/Status\.DownloadPercent >= 0\.0f[\s\S]*SetNumberField\(TEXT\("downloadPercent"\)/u);
    expect(status).toMatch(/if \(!bFinished\)\s*\{\s*Data->SetBoolField\(TEXT\("cancellable"\)/u);
  });

  it('names what a cancelled import left behind and how to remove it', () => {
    expect(status).toContain('Status.Result.ErrorCode == TEXT("CANCELLED")');
    expect(status).toContain('asset.delete removes it');
  });

  it('keeps the plain-id check in one place both handlers share', () => {
    const json = code(core('Private/Domains/AssetWorkflow/Fab/McpAutomationBridge_FabImportJson.h'));
    expect(json).toContain('inline bool IsPlainKey(const FString& Key)');
    expect(json).toMatch(/FChar::IsAlnum\(Ch\)/u);
    expect(status).not.toContain('FChar::IsAlnum');
  });
});

describe('the cancel handler', () => {
  const handler = code(core('Private/Domains/AssetWorkflow/Fab/McpAutomationBridge_AssetWorkflowFabCancel.cpp'));

  it('looks up only a plain operation id, through the provider, and never touches Fab itself', () => {
    expect(handler).toContain('McpFabImportJson::IsPlainKey(OperationId)');
    expect(handler).toContain('Provider->CancelImport(OperationId, Message, ErrorCode)');
    expect(handler).not.toMatch(/ExecuteJavascript|McpFabBridgeDispatch|BindUObject|SimulateClick/u);
  });

  it('offers the read that reports the import when the cancel is refused for a reason other than an unknown id', () => {
    expect(handler).toMatch(/ErrorCode != TEXT\("NOT_FOUND"\)[\s\S]*MakeStatusNextCall\(OperationId\)/u);
  });

  it('is dispatched, declared and implemented by the adapter like the other Fab actions', () => {
    expect(code(core('Private/Domains/AssetWorkflow/McpAutomationBridge_AssetWorkflowHandlers.cpp'))).toContain('cancel_fab_import');
    expect(core('Public/McpAutomationBridgeSubsystemAssetWorkflowDeclarations.h')).toContain('HandleCancelFabImport');
    expect(code(fab('McpAutomationBridgeFabModule.cpp'))).toContain('McpFabImportOperations::RequestCancel(OperationId, OutMessage, OutErrorCode)');
    expect(code(core('Public/McpFabProvider.h'))).toContain('virtual bool CancelImport(const FString& OperationId, FString& OutMessage, FString& OutErrorCode) = 0;');
  });
});
