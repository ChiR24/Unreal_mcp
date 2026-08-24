import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';

const pluginSource = resolve(
  process.cwd(),
  'plugins/McpAutomationBridge/Source/McpAutomationBridge',
);

function readPluginFile(...parts: string[]): string {
  return readFileSync(resolve(pluginSource, ...parts), 'utf8');
}

describe('sequence engine compatibility contracts', () => {
  it('supports the MRQ config rename across UE 5.0-5.8', () => {
    const compatibility = readPluginFile(
      'Private',
      'Core',
      'Compatibility',
      'McpVersionCompatibility.h',
    );
    const movieRenderRoot = resolve(
      pluginSource,
      'Private',
      'Domains',
      'Sequence',
      'MovieRender',
    );
    const representativeFiles = [
      'McpAutomationBridge_SequenceMovieRenderState.cpp',
      'McpAutomationBridge_SequenceMovieRenderSettings.cpp',
      'McpAutomationBridge_SequenceMovieRenderResourceLimits.cpp',
      'McpAutomationBridge_SequenceMovieRenderOutputProof.cpp',
    ].map(file => readFileSync(resolve(movieRenderRoot, file), 'utf8'));

    expect(compatibility).toContain(
      '__has_include("MoviePipelinePrimaryConfig.h")',
    );
    expect(compatibility).toContain(
      '__has_include("MoviePipelineMasterConfig.h")',
    );
    expect(compatibility).toContain('MCP_MOVIE_PIPELINE_CONFIG_CLASS');
    for (const source of representativeFiles) {
      expect(source).toContain('MCP_MOVIE_PIPELINE_CONFIG_HEADER');
      expect(source).toContain('MCP_MOVIE_PIPELINE_CONFIG_CLASS');
      expect(source).not.toContain('#include "MoviePipelinePrimaryConfig.h"');
    }
  });

  it('wraps queue dirty APIs that are absent in UE 5.0-5.1', () => {
    const compatibility = readPluginFile(
      'Private',
      'Core',
      'Compatibility',
      'McpVersionCompatibility.h',
    );
    const movieRenderRoot = resolve(
      pluginSource,
      'Private',
      'Domains',
      'Sequence',
      'MovieRender',
    );
    const implementation = [
      'McpAutomationBridge_SequenceMovieRenderJobCreation.cpp',
      'McpAutomationBridge_SequenceMovieRenderExecution.cpp',
      'McpAutomationBridge_SequenceMovieRenderSettings.cpp',
      'McpAutomationBridge_SequenceMovieRenderOutput.cpp',
      'McpAutomationBridge_SequenceMovieRenderPasses.cpp',
      'McpAutomationBridge_SequenceMovieRenderBurnIns.cpp',
    ].map(file => readFileSync(resolve(movieRenderRoot, file), 'utf8')).join('\n');

    expect(compatibility).toContain('MCP_GET_MOVIE_PIPELINE_QUEUE_DIRTY');
    expect(compatibility).toContain('MCP_SET_MOVIE_PIPELINE_QUEUE_DIRTY');
    expect(implementation).not.toMatch(/Queue->(?:IsDirty|SetIsDirty)\(/);
  });

  it('guards shot metadata that only exists in UE 5.6+', () => {
    const compatibility = readPluginFile(
      'Private',
      'Core',
      'Compatibility',
      'McpVersionCompatibility.h',
    );
    const results = readPluginFile(
      'Private',
      'Domains',
      'Sequence',
      'RecordReplay',
      'McpAutomationBridge_SequenceTakeRecorderResults.cpp',
    );

    expect(compatibility).toContain('MCP_HAS_MOVIE_SCENE_SHOT_METADATA');
    expect(results).toContain('#if MCP_HAS_MOVIE_SCENE_SHOT_METADATA');
    expect(results).toContain('MetaData/MovieSceneShotMetaData.h');
  });

  it('only enables the MRQ object ID pass when its public header exists', () => {
    const buildRules = readPluginFile('McpAutomationBridge.Build.cs');
    const compatibility = readPluginFile(
      'Private',
      'Core',
      'Compatibility',
      'McpVersionCompatibility.h',
    );
    const passes = readPluginFile(
      'Private',
      'Domains',
      'Sequence',
      'MovieRender',
      'McpAutomationBridge_SequenceMovieRenderPasses.cpp',
    );

    expect(buildRules).toContain('MoviePipelineObjectIdPass.h');
    expect(buildRules).toContain('"Public"');
    expect(buildRules).toContain('MCP_HAS_MOVIE_PIPELINE_OBJECT_ID_PASS');
    expect(compatibility).toContain(
      '#ifndef MCP_HAS_MOVIE_PIPELINE_OBJECT_ID_PASS',
    );
    expect(passes).toContain('#if MCP_HAS_MOVIE_PIPELINE_OBJECT_ID_PASS');
    expect(passes).not.toContain('#if MCP_HAS_MOVIE_PIPELINE_MASK_RENDER_PASS');
  });

  it('uses headers and FString shrink arguments available in older UE releases', () => {
    const compatibility = readPluginFile(
      'Private',
      'Core',
      'Compatibility',
      'McpVersionCompatibility.h',
    );
    const reflection = readPluginFile(
      'Private',
      'Foundation',
      'Reflection',
      'McpPropertyReflectionPrivate.h',
    );
    const snapshotPaths = readPluginFile(
      'Private',
      'Domains',
      'Environment',
      'McpAutomationBridge_EnvironmentSnapshotPaths.cpp',
    );
    const requestQueue = readPluginFile(
      'Private',
      'Core',
      'Subsystem',
      'McpAutomationBridgeSubsystemRequestQueue.cpp',
    );

    expect(reflection).toContain(
      '#if __has_include("UObject/StrProperty.h")',
    );
    expect(compatibility).toContain(
      '__has_include("Containers/AllowShrinking.h")',
    );
    expect(compatibility).toContain('MCP_DISALLOW_SHRINKING');
    expect(snapshotPaths).toContain('MCP_DISALLOW_SHRINKING');
    expect(snapshotPaths).not.toContain('EAllowShrinking::No');
    expect(requestQueue).toContain('MCP_DISALLOW_SHRINKING');
    expect(requestQueue).not.toContain('EAllowShrinking::No');
  });

  it('guards newer MRQ pass metadata and anti-aliasing APIs', () => {
    const buildRules = readPluginFile('McpAutomationBridge.Build.cs');
    const compatibility = readPluginFile(
      'Private',
      'Core',
      'Compatibility',
      'McpVersionCompatibility.h',
    );
    const passes = readPluginFile(
      'Private',
      'Domains',
      'Sequence',
      'MovieRender',
      'McpAutomationBridge_SequenceMovieRenderPasses.cpp',
    );
    const settings = readPluginFile(
      'Private',
      'Domains',
      'Sequence',
      'MovieRender',
      'McpAutomationBridge_SequenceMovieRenderSettings.cpp',
    );

    expect(buildRules).toContain('MCP_HAS_MOVIE_PIPELINE_PASS_METADATA');
    expect(buildRules).toContain('MCP_HAS_SMAA');
    expect(compatibility).toContain(
      '#ifndef MCP_HAS_MOVIE_PIPELINE_PASS_METADATA',
    );
    expect(compatibility).toContain('#ifndef MCP_HAS_SMAA');
    expect(passes).toContain('#if MCP_HAS_MOVIE_PIPELINE_PASS_METADATA');
    expect(settings).toContain('#if MCP_HAS_SMAA');
  });

  it('gates bUseLosslessCompression on its own probe, not the pass-metadata one', () => {
    const buildRules = readPluginFile('McpAutomationBridge.Build.cs');
    const compatibility = readPluginFile(
      'Private',
      'Core',
      'Compatibility',
      'McpVersionCompatibility.h',
    );
    const passes = readPluginFile(
      'Private',
      'Domains',
      'Sequence',
      'MovieRender',
      'McpAutomationBridge_SequenceMovieRenderPasses.cpp',
    );

    // bUseLosslessCompression landed in 5.7; bHighPrecisionOutput (the pass-metadata
    // probe) is already present in 5.6, so the two must not share a macro.
    expect(buildRules).toContain('bUseLosslessCompression');
    expect(buildRules).toContain('MCP_HAS_MOVIE_PIPELINE_LOSSLESS');
    expect(compatibility).toContain('#ifndef MCP_HAS_MOVIE_PIPELINE_LOSSLESS');
    expect(passes).toContain('#if MCP_HAS_MOVIE_PIPELINE_LOSSLESS');
  });

  it('uses compatible replay duration and Take Recorder parameters', () => {
    const buildRules = readPluginFile('McpAutomationBridge.Build.cs');
    const compatibility = readPluginFile(
      'Private',
      'Core',
      'Compatibility',
      'McpVersionCompatibility.h',
    );
    const replay = readPluginFile(
      'Private',
      'Domains',
      'Sequence',
      'RecordReplay',
      'McpAutomationBridge_SequenceReplayRuntime.cpp',
    );
    const takeRecording = readPluginFile(
      'Private',
      'Domains',
      'Sequence',
      'RecordReplay',
      'McpAutomationBridge_SequenceTakeRecorderRecording.cpp',
    );

    expect(buildRules).toContain('MCP_HAS_REPLAY_SUBSYSTEM_TOTAL_TIME');
    expect(buildRules).toContain('MCP_HAS_TAKE_RECORDER_OPEN_SEQUENCER');
    expect(compatibility).toContain(
      '#ifndef MCP_HAS_REPLAY_SUBSYSTEM_TOTAL_TIME',
    );
    expect(compatibility).toContain(
      '#ifndef MCP_HAS_TAKE_RECORDER_OPEN_SEQUENCER',
    );
    expect(replay).toContain('#if MCP_HAS_REPLAY_SUBSYSTEM_TOTAL_TIME');
    expect(replay).toContain('Driver->GetDemoTotalTime()');
    expect(takeRecording).toContain(
      '#if MCP_HAS_TAKE_RECORDER_OPEN_SEQUENCER',
    );
  });
});
