// A MetaSound plays the graph the frontend registered at its first play, and later plays reuse it,
// so edit_metasound saved every change while the editor kept playing the old graph until a restart
// (2026-10-03: nine gain literals set, every sound measured exactly as before). The audio authoring
// dispatcher now re-registers the edited MetaSound after every graph edit. Needs an editor to run,
// so these assert the wiring.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const AUDIO = join('plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private', 'Domains', 'AudioAuthoring');
const DISPATCHER = join(AUDIO, 'McpAutomationBridge_AudioAuthoringHandlers.cpp');
const PRIVATE_HEADER = join(AUDIO, 'McpAutomationBridge_AudioAuthoringHandlersPrivate.h');

/** Block and line comments removed, so no assertion can be satisfied by prose. */
const source = (): string =>
  readFileSync(DISPATCHER, 'utf8').replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');

// A wrong output name ("Band Pass Filter" for "Band Pass") stopped a build_metasound after its nodes were added.
describe('build_metasound checks the pins it names on its own nodes before running', () => {
  const strip = (file: string): string =>
    readFileSync(join(AUDIO, file), 'utf8').replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');

  it('reads the registry class pins once the add_node class resolves', () => {
    expect(strip('McpAutomationBridge_AudioAuthoringHandlersMetaSoundNodes.cpp')).toContain('FindMetaSoundClassPins(Resolved, Request.Inputs, Request.Outputs);');
    const search = strip(join('MetaSound', 'McpAutomationBridge_AudioAuthoringHandlersMetaSoundNodeSearch.cpp'));
    expect(search).toMatch(/#if ENGINE_MAJOR_VERSION > 5 \|\| ENGINE_MINOR_VERSION >= 6\s*const FMetasoundFrontendClassInterface& Interface = Class\.GetDefaultInterface\(\);\s*#else\s*const FMetasoundFrontendClassInterface& Interface = Class\.Interface;/u);
  });

  it('refuses a connect or set_default pin missing on a node the batch adds, with nothing applied', () => {
    const batch = strip(join('MetaSound', 'McpAutomationBridge_AudioAuthoringHandlersMetaSoundBatch.cpp'));
    const precheck = batch.slice(batch.indexOf('TMap<FString, FMcpMetaSoundNodeClassRequest> Added;'), batch.indexOf('TMap<FString, FString> Aliases;'));
    expect(precheck).toContain('MissingMetaSoundPin(Added, Step, TEXT("targetNodeId"), TEXT("targetInputName"), false, Pins);');
    expect(precheck).toContain('bDefault ? TEXT("inputName") : TEXT("sourceOutputName"), !bDefault, Pins);');
    expect(precheck).toContain('return McpHandlerUtils::BuildErrorResponse(TEXT("PIN_NOT_FOUND")');
    expect(precheck).toContain('if ((*StepObj)->TryGetStringField(TEXT("id"), AddedId)) { Added.Add(AddedId, Request); }');
    expect(batch).toContain('Known.StartsWith(Pin + TEXT(" ("), ESearchCase::IgnoreCase)');
  });

  // Every step ran the single-edit handler with the batch's save, so a 63-step voice wrote its package 65 times.
  it('saves the MetaSound once, after the steps, not after each one', () => {
    const batch = strip(join('MetaSound', 'McpAutomationBridge_AudioAuthoringHandlersMetaSoundBatch.cpp'));
    const steps = batch.indexOf('Step->SetBoolField(TEXT("save"), false);');
    expect(steps).toBeGreaterThan(-1);
    expect(steps).toBeLessThan(batch.indexOf('HandleMetaSoundNodeActions(StepSubAction'));
    expect(batch).toContain('Details->SetBoolField(TEXT("saved"), Index > 0 && SaveOnce());');
    expect(batch).toContain('Response->SetBoolField(TEXT("saved"), SaveOnce());');
    expect(batch).not.toContain('McpSafeAssetSave');
  });
});

describe('a MetaSound graph edit reaches playback without an editor restart', () => {
  it('re-registers through the MetaSound editor, only where the builder and the editor subsystem exist', () => {
    const code = source();
    expect(code).toContain('#if MCP_HAS_METASOUND && MCP_HAS_METASOUND_FRONTEND && __has_include("MetasoundEditorSubsystem.h")');
    // Only a MetaSound source: the editor's register check()s that the object is a MetaSound.
    expect(code).toContain('FindObject<UMetaSoundSource>(nullptr, *AssetPath)');
    expect(code).toContain('GEditor->GetEditorSubsystem<UMetaSoundEditorSubsystem>()');
    expect(code).toContain('if (!MetaSound || !MetaSoundEditor) { return; }');
  });

  it('edits through the builder the engine already holds for the MetaSound, so its cache never goes stale', () => {
    // The live crash: a second builder edited MS_PowerUp (a batch removed nodes) while the engine's builder
    // cached the old layout; the register then indexed 9 into an array of 5. A later edit logged "prior builder
    // is still active".
    const header = readFileSync(PRIVATE_HEADER, 'utf8').replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');
    expect(header).toMatch(/#define MCP_METASOUND_BUILDER\(Name, Document\) TOptional<FMetaSoundFrontendDocumentBuilder> Name##Own; \\\s*FMetaSoundFrontendDocumentBuilder& Name = McpAudioAuthoring::McpMetaSoundBuilder\(Document, Name##Own\)/u);
    expect(header).toContain('if (FMetaSoundFrontendDocumentBuilder* Existing = Builders->FindBuilder(Document)) { return *Existing; }');
    // Only a builder the call opened itself is finished; the engine's stays its own.
    expect(header).toContain('#define MCP_METASOUND_FINISH(Name) if (Name##Own.IsSet()) { Name.FinishBuilding(); }');
  });

  it('on 5.4, which cannot find the engine\'s builder, drops its cache before registering', () => {
    const code = source();
    const invalidate = code.indexOf('Builders->InvalidateDocumentCache(');
    expect(invalidate).toBeGreaterThan(-1);
    expect(code.indexOf('MetaSoundEditor->RegisterGraphWithFrontend(*MetaSound);')).toBeGreaterThan(invalidate);
    expect(code).toMatch(/#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 4\s*if \(Metasound::Frontend::IDocumentBuilderRegistry\* Builders/u);
    expect(code).not.toContain('ReloadBuilder(');
  });

  it('runs after every graph edit handler, once per call (a batch registers once, not per step)', () => {
    const code = source();
    const edits = ['HandleMetaSoundBatchAction', 'HandleMetaSoundNodeActions', 'HandleMetaSoundInterfaceActions', 'HandleMetaSoundGraphEditActions'];
    const register = code.indexOf('ReregisterEditedMetaSound(Params);');
    expect(register).toBeGreaterThan(-1);
    for (const handler of edits) {
      const call = code.indexOf(`${handler}(SubAction, Params, Response)`);
      expect(call, handler).toBeGreaterThan(-1);
      expect(call, handler).toBeLessThan(register);
    }
    expect(code).toMatch(/if \(Edited\)\s*\{\s*ReregisterEditedMetaSound\(Params\);\s*return Edited;\s*\}/u);
  });

  it('leaves reads alone: get_metasound_graph is dispatched after the edits', () => {
    const code = source();
    expect(code.indexOf('HandleMetaSoundGraphReadAction(SubAction, Params, Response)')).toBeGreaterThan(code.indexOf('return Edited;'));
  });
});
