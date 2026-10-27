// The validator delta's Data Validation half: a write's reply names what the editor's save-time Data Validation finds on
// the assets the call changed, as the change watch names what Map Check finds on the actors. Wiring contracts only: the
// behaviour is checked live.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const PRIVATE = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private');

/** Line comments removed, CRLF normalized, so no assertion can be satisfied by prose. */
const code = (...segments: readonly string[]): string =>
  readFileSync(join(PRIVATE, ...segments), 'utf8').replace(/\r\n/gu, '\n').replace(/\/\/[^\n]*/gu, ' ');

describe('Data Validation of what a call changed', () => {
  const delta = code('Domains', 'SystemControl', 'McpSystemValidateDelta.cpp');

  it('runs in the one success funnel, after the change watch, the undo ledger and the user-change warning', () => {
    expect(code('Core', 'Subsystem', 'EngineWireSubsystemResponses.cpp')).toMatch(
      /McpChangeJournal::AddUserChangeWarning\(RequestId, EffectiveResult\);\s*McpValidateDelta::AddWarnings\(RequestId, EffectiveResult\);/u);
  });

  it('checks the assets the journal says the running request changed, and loads none', () => {
    expect(delta).toContain('McpChangeJournal::AssetsChangedBy(RequestId, Packages);');
    expect(delta).toContain('FindObject<UObject>(nullptr,');
    expect(delta).not.toMatch(/LoadObject|LoadAsset|LoadPackage/u);
    expect(code('Core', 'Journal', 'McpChangeJournalRead.cpp')).toMatch(
      /RequestId\.IsEmpty\(\) \|\| RequestId != State\.RequestId \|\| State\.Seq <= State\.WarnUntil/u);
  });

  // 5.8's material validator compiles shaders in a manual or script check; the editor's save-time check leaves it out.
  it('runs the save-time checks, reaches the optional module only while it is loaded, and leaves batch steps out', () => {
    expect(delta).toContain('EDataValidationUsecase::Save');
    expect(delta).toContain('FModuleManager::Get().IsModuleLoaded(TEXT("DataValidation"))');
    expect(delta).toContain('FMcpResponseCaptureRegistry::Get().IsCapturing(RequestId)');
  });

  // The module is delay-loaded: where a project turned the plugin off, its subsystem's StaticClass fails the delay-load.
  it('validate_assets refuses dataValidation before it reaches the subsystem when the module is not loaded', () => {
    const validate = code('Domains', 'SystemControl', 'McpSystemAssetValidate.cpp');
    const guard = validate.indexOf('if (bDataValidation && !FModuleManager::Get().IsModuleLoaded(TEXT("DataValidation")))');
    expect(guard).toBeGreaterThan(-1);
    expect(validate.indexOf('TEXT("DATAVALIDATION_PLUGIN_NOT_ENABLED")', guard)).toBeGreaterThan(guard);
    expect(validate.indexOf('GetEditorSubsystem<UEditorValidatorSubsystem>()')).toBeGreaterThan(guard);
  });
});
