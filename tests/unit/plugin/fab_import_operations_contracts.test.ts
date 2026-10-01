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
    const reply = add.indexOf('OnResolved(Result);', watch);
    expect(watch, 'the accepted path must start the watcher').toBeGreaterThan(-1);
    expect(reply, 'and reply right after it').toBeGreaterThan(watch);
  });

  it('gives the watcher no completion callback for the caller to wait on', () => {
    const header = code(fab('Import/McpFabImportWatcher.h'));
    expect(header).toMatch(/void WatchForImport\(/u);
    expect(header).not.toMatch(/OnComplete|OnAccepted/u);
    // The outcome goes to the store instead.
    expect(code(fab('Import/McpFabImportWatcher.cpp'))).toContain('McpFabImportOperations::Finish(OperationId, Accepted);');
  });

  it('queues a second import behind the running one instead of refusing it', () => {
    const queue = add.indexOf('McpFabImportOperations::ListQueue(');
    const begin = add.indexOf('McpFabImportOperations::Begin(');
    expect(queue).toBeGreaterThan(-1);
    expect(begin).toBeGreaterThan(queue);
    expect(add).toContain('McpFabImportOperations::Enqueue(OperationId,');
    // Nothing is refused for being busy: only a full queue is.
    expect(add).not.toContain('ALREADY_IN_FLIGHT');
    expect(add).toContain('QUEUE_FULL');
  });

  it('answers a full queue with the import at its head and the call that reads it', () => {
    expect(code(fab('McpFabAddReply.cpp'))).toMatch(/It is working on %s \(operation %s, %s%s, %\.0f s in\)\. Poll asset\.query_marketplace with lookup=fab_import_status/u);
    const handler = code(core('Private/Domains/AssetWorkflow/Operations/McpAutomationBridge_AssetWorkflowFabAdd.cpp'));
    expect(handler).toMatch(/QUEUE_FULL[\s\S]*MakeStatusNextCall\(Result\.OperationId\)/u);
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
    expect(status).toMatch(/McpFabImportJson::IsPlainKey\(Key\)/u);
    expect(code(core('Private/Domains/AssetWorkflow/Fab/McpAutomationBridge_FabImportJson.h')))
      .toMatch(/FChar::IsAlnum\(Ch\)/u);
  });

  it('is dispatched and declared like every other asset action', () => {
    expect(code(core('Private/Domains/AssetWorkflow/McpAutomationBridge_AssetWorkflowHandlers.cpp')))
      .toContain('get_fab_import_status');
    expect(core('Public/McpAutomationBridgeSubsystemAssetWorkflowDeclarations.h')).toContain('HandleGetFabImportStatus');
  });
});

describe('what an import record can carry', () => {
  const provider = code(core('Public/McpFabTypes.h'));
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
  const capture = code(fab('Import/McpFabLogCapture.cpp'));

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

describe('the Fab mesh-merging guard', () => {
  const guard = code(fab('Import/McpFabInterchange.cpp'));

  it('reaches Interchange by reflection only, so the module takes no build dependency on it', () => {
    expect(guard).toContain('FindObject<UClass>(nullptr, AssetsPipelinePath)');
    expect(guard).not.toMatch(/#include\s+"Interchange/u);
    const build = readFileSync(resolve(plugin, 'McpAutomationBridgeFab/McpAutomationBridgeFab.Build.cs'), 'utf8');
    expect(build).not.toMatch(/Interchange/u);
  });

  it('edits only the pipelines Fab generated for one import, never a project setting or an asset', () => {
    expect(guard).toMatch(/!Pipeline->IsRooted\(\)/u);
    for (const banned of ['GetMutableDefault', 'SaveConfig', 'SavePackage', 'SaveObject', 'ProjectSettings']) {
      expect(guard, `${banned} would change more than one import`).not.toContain(banned);
    }
  });

  it('works only when the caller asked for separate meshes, and says whether it took', () => {
    const watcher = code(fab('Import/McpFabImportWatcher.cpp'));
    expect(watcher).toMatch(/MeshesSeparated\.IsSet\(\) && !Accepted\.MeshesSeparated\.GetValue\(\) && McpFabInterchange::SeparateMeshes\(\) > 0/u);
    expect(watcher).toContain('McpFabImportOperations::SetMeshesSeparated(OperationId, true);');
  });

  it('refuses separate meshes before claiming or downloading anything when the engine cannot do it', () => {
    const add = code(fab('McpFabAddOperation.cpp'));
    const unsupported = add.indexOf('COMBINE_UNSUPPORTED');
    expect(unsupported).toBeGreaterThan(-1);
    expect(unsupported).toBeLessThan(add.indexOf('McpFabImportOperations::Begin('));
    expect(add).toContain('McpFabInterchange::CanSeparateMeshes()');
  });
});

describe('the post-import save', () => {
  const post = code(core('Private/Domains/AssetWorkflow/Fab/McpAutomationBridge_FabPostImport.cpp'));

  it('saves through the safe wrapper, and only what the import left dirty', () => {
    expect(post).toContain('McpSafeOperations::McpSafeAssetSave(Package)');
    expect(post).toContain('!Package->IsDirty()');
    expect(post).not.toContain('SavePackage');
  });

  it('names the packages it could not save', () => {
    expect(post).toContain('Result.UnsavedPackages.Add(PackageName)');
  });

  it('runs after the registry hook is off and before the outcome is stored', () => {
    const watcher = code(fab('Import/McpFabImportWatcher.cpp'));
    const hookOff = watcher.indexOf('RegistryRef.OnAssetAdded().Remove(Watch->AddedHandle);');
    const ran = watcher.indexOf('PostImport(Accepted, Added);');
    const stored = watcher.indexOf('McpFabImportOperations::Finish(OperationId, Accepted);');
    expect(hookOff).toBeGreaterThan(-1);
    expect(ran).toBeGreaterThan(hookOff);
    expect(stored).toBeGreaterThan(ran);
  });

  it('is what the add hands the adapter', () => {
    expect(code(core('Private/Domains/AssetWorkflow/Operations/McpAutomationBridge_AssetWorkflowFabAdd.cpp')))
      .toContain('Options.PostImport = &McpFabPostImport::Run;');
    expect(code(fab('McpFabAddOperation.cpp'))).toContain('Options.PostImport');
  });
});

describe('the Fab add queue', () => {
  const store = code(fab('Import/McpFabImportOperations.cpp'));

  it('starts the oldest queued add, and only once nothing is running', () => {
    const start = store.slice(store.indexOf('void StartNext()'), store.indexOf('void SchedulePump()'));
    expect(start).toMatch(/if \(IsRunning\(Op\)\)\s*\{\s*return;/u);
    expect(start).toContain('Operations().FindByPredicate(');
    expect(start).toContain('EState::Queued');
    // Launched from the moved-out closure, so nothing touches the record after it runs.
    expect(start).toContain('TFunction<void()> Launch = MoveTemp(Next->Launch);');
  });

  it('starts the next add when an import ends, outside the ticker that ended it', () => {
    const finish = store.slice(store.indexOf('void Finish('), store.indexOf('bool FindRunning('));
    expect(finish).toContain('SchedulePump();');
    expect(store).toMatch(/FTSTicker::GetCoreTicker\(\)\.AddTicker\(FTickerDelegate::CreateLambda/u);
  });

  it('keeps arrival order: an add behind a non-empty queue waits, even when nothing is running yet', () => {
    const add = code(fab('McpFabAddOperation.cpp'));
    expect(add).toContain('const bool bBusy = Queue.Num() > 0;');
    expect(add).toContain('if (!bBusy)');
    expect(add).toMatch(/constexpr int32 MaxQueued = 8;/u);
  });

  it('gives a queued add the same start as any other, so its refusals reach the status', () => {
    const add = code(fab('McpFabAddOperation.cpp'));
    expect(add).toMatch(/Launch\(OperationId, ListingId, EngineVersion, Options, nullptr\);/u);
    expect(add).toMatch(/McpFabImportOperations::Finish\(OperationId, Result\);\s*if \(OnResolved\)/u);
  });

  it('is listed by the status read, which reads it from the store like everything else', () => {
    const status = code(core('Private/Domains/AssetWorkflow/Fab/McpAutomationBridge_AssetWorkflowFabImportStatus.cpp'));
    expect(status).toContain('Provider->GetImportQueue(Queue);');
    expect(status).toContain('TEXT("queue")');
    expect(code(fab('McpAutomationBridgeFabModule.cpp'))).toContain('McpFabImportOperations::ListQueue(');
  });
});

describe('a refused download says which step failed', () => {
  const reply = code(fab('McpFabAddReply.cpp'));
  const script = code(fab('McpFabDownloadScript.cpp'));

  it('names the step, Fab\'s status and Fab\'s own words in the reply', () => {
    for (const step of ['license', 'claim', 'download-info']) {
      expect(reply).toContain(`TEXT("${step}")`);
    }
    expect(reply).toMatch(/the claim step failed \(HTTP %\.0f: %s\)/u);
    expect(reply).toMatch(/the download-info step failed \(HTTP %\.0f: %s\)/u);
    expect(reply).toContain('The file lookup step found no downloadable file');
  });

  it('records the step on every path that ends in NO_DOWNLOAD_URL', () => {
    const exits = script.match(/out\.error = "NO_DOWNLOAD_URL";/gu)?.length ?? 0;
    const explained = script.match(/explainNoDownload\(\);|out\.failedStep = "download-info";\s*out\.stepStatus = r\.status;/gu)?.length ?? 0;
    expect(exits).toBe(2);
    expect(explained).toBe(2);
  });

  it('never takes a picture address for a download, and never reports a value from the reply', () => {
    expect(script).toMatch(/!\/thumb\|preview\|image\|icon\|media\/i\.test\(k\)/u);
    // Only key names of an unfamiliar reply are reported, never their values.
    expect(script).toContain('Object.keys(info).slice(0, 12).join(", ")');
  });
});

describe('a quality tier named by the caller', () => {
  const handler = code(core('Private/Domains/AssetWorkflow/Operations/McpAutomationBridge_AssetWorkflowFabAdd.cpp'));
  const add = code(fab('McpFabAddOperation.cpp'));
  const script = code(fab('McpFabAddToProject.cpp'));

  it('reaches the page only as one of the four known tiers, checked twice', () => {
    // The handler refuses anything else before the adapter is asked...
    expect(handler).toMatch(/Quality != TEXT\("raw"\) && Quality != TEXT\("high"\) && Quality != TEXT\("mid"\) &&\s*Quality != TEXT\("low"\)/u);
    expect(handler.indexOf('Options.Quality = Quality;')).toBeLessThan(handler.indexOf('Provider->AddToProject('));
    // ...and the adapter checks again, before anything is claimed, because the tier is spliced into script text.
    expect(script).toMatch(/Value\.IsEmpty\(\) \|\| Value == TEXT\("raw"\) \|\| Value == TEXT\("high"\) \|\| Value == TEXT\("mid"\) \|\| Value == TEXT\("low"\)/u);
    expect(add.indexOf('!IsKnownQuality(Options.Quality)')).toBeGreaterThan(-1);
    expect(add.indexOf('!IsKnownQuality(Options.Quality)')).toBeLessThan(add.indexOf('McpFabImportOperations::Begin('));
    expect(add).toContain('BuildAddScript(RequestId, ListingId, EngineVersion, Options.CombineMeshes, Options.Quality)');
  });

  it('is compared to the tiers of the listing\'s own files and never placed in a request', () => {
    const page = script.slice(script.indexOf('(function () {'));
    expect(page).toContain('quality = "%s"');
    expect(page).toContain('pickTier(ready, quality)');
    expect(page).not.toMatch(/fetch\([^)]*quality/u);
    expect(page).not.toMatch(/form\.append\([^)]*quality/u);
  });

  it('refuses a tier the listing lacks with what it offers, in words', () => {
    expect(script).toContain('out.error = "QUALITY_NOT_AVAILABLE";');
    const reply = code(fab('McpFabAddReply.cpp'));
    expect(reply).toContain('TEXT("QUALITY_NOT_AVAILABLE")');
    expect(reply).toMatch(/publishes no '%s' quality for %s\. It offers: %s\./u);
    expect(reply).toMatch(/Nothing was downloaded\./u);
  });
});
