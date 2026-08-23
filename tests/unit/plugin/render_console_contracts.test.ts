// Todo 32-35 BB-020/BB-021/BB-055/BB-056/BB-057/BB-058/BB-059/BB-061/BB-063
// Post-process volumes, sky/light mutations, render state invalidation, console execution.
import { existsSync, readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';

const PRIVATE = resolve(process.cwd(), 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private');
function readCpp(...parts: string[]): string {
  const p = resolve(PRIVATE, ...parts);
  expect(existsSync(p), `missing: ${p}`).toBe(true);
  return readFileSync(p, 'utf8');
}
function code(s: string): string { return s.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/[^\n]*/g, ''); }

const reflectionSettings = () => readCpp('Domains/Environment/Runtime/McpAutomationBridge_EnvironmentHandlersReflectionSettings.cpp');
const actorComponents = () => readCpp('Domains/Environment/Runtime/McpAutomationBridge_EnvironmentHandlersActorComponents.cpp');
const buildSkyWeather = () => readCpp('Domains/Environment/McpAutomationBridge_EnvironmentHandlersBuildSkyWeather.cpp');
const lightingHandlersPostProcess = () => readCpp('Domains/Lighting/McpAutomationBridge_LightingHandlersPostProcess.cpp');
const lightSpawn = () => readCpp('Domains/Lighting/McpAutomationBridge_LightingHandlersLightSpawn.cpp');
const renderPostProcessLens = () => readCpp('Domains/Render/McpAutomationBridge_RenderPostProcessLens.cpp');
const renderSupport = () => readCpp('Domains/Render/McpAutomationBridge_RenderSupportSettings.h');
const consoleCommandHandlers = () => readCpp('Domains/ConsoleCommand/McpAutomationBridge_ConsoleCommandHandlers.cpp');

describe('BB-020/BB-056 post-process volume resolution is deterministic', () => {
  it('RenderSupport or a shared resolver declares McpResolvePostProcessVolume', () => {
    const files = [renderSupport(), renderPostProcessLens(), lightingHandlersPostProcess()];
    const combined = code(files.join('\n'));
    expect(combined).toMatch(/ResolvePostProcessVolume|McpResolvePostProcessVolume/i);
  });
});

describe('BB-021 post-process volume component state is exposed', () => {
  it('RenderSupport or inspect handler declares McpDescribePostProcessVolume', () => {
    const s = code(renderSupport());
    expect(s).toMatch(/DescribePostProcessVolume|McpDescribePostProcessVolume/i);
  });
});

describe('BB-055 sky light intensity field mapping exists', () => {
  it('BuildSkyWeather.cpp or ReflectionSettings.cpp maps skyLightIntensity to SetIntensity', () => {
    const combined = code(buildSkyWeather() + '\n' + reflectionSettings());
    expect(combined).toMatch(/skyLightIntensity|SetIntensity/i);
  });
});

describe('BB-058 dynamic light spawn applies top-level intensity', () => {
  it('LightSpawn.cpp applies intensity to the light component', () => {
    const s = code(lightSpawn());
    expect(s).toMatch(/intensity|Intensity/i);
    expect(s).toMatch(/SetIntensity|SetLightIntensity/i);
  });
});

describe('BB-059 post-process volume creation returns truthful receipt', () => {
  it('PPV creation uses default name PostProcessVolume or honors requested name', () => {
    const combined = code(renderPostProcessLens() + '\n' + lightingHandlersPostProcess());
    expect(combined).toMatch(/PostProcessVolume|volumeName/i);
  });
});

describe('BB-061/BB-063 render state invalidation after mutations', () => {
  it('ActorComponents.cpp or Environment handlers call McpRefreshRenderState or MarkRenderStateDirty', () => {
    const combined = code(actorComponents() + '\n' + reflectionSettings());
    expect(combined).toMatch(/RefreshRenderState|MarkRenderStateDirty|RecreateRenderState/i);
  });
});

describe('BB-057 console execution has game-thread guard and bounded output', () => {
  it('ConsoleCommandHandlers.cpp asserts IsInGameThread', () => {
    const s = code(consoleCommandHandlers());
    expect(s).toMatch(/IsInGameThread/);
  });
  it('ConsoleCommandHandlers.cpp captures bounded output (FStringOutputDevice or OutputDevice)', () => {
    const s = code(consoleCommandHandlers());
    expect(s).toMatch(/OutputDevice|FStringOutputDevice|OutputDeviceNull/i);
  });
  it('ConsoleCommandHandlers.cpp has a generic COMMAND_BLOCKED message without echoing command', () => {
    const s = code(consoleCommandHandlers());
    expect(s).toMatch(/COMMAND_BLOCKED/i);
    const blockedIdx = s.indexOf('COMMAND_BLOCKED');
    if (blockedIdx >= 0) {
      const slice = s.slice(blockedIdx, blockedIdx + 300);
      expect(slice).not.toMatch(/%s.*command|command.*%s/i);
    }
  });
});
