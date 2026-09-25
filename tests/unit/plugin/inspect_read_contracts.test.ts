/// <reference types="node" />

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const PRIVATE = 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private';
const read = (relative: string): string => readFileSync(resolve(process.cwd(), PRIVATE, relative), 'utf8');

describe('component list filter', () => {
  it('filters every components reply by componentNames and names the ones it did not find', () => {
    // Checking one component of a 27-component Blueprint returned all 27.
    const callers = [
      'Domains/ControlActor/McpAutomationBridge_ControlActorComponents.cpp',
      'Domains/Environment/Inspection/McpAutomationBridge_EnvironmentHandlersInspectBlueprint.cpp',
      'Domains/Property/McpAutomationBridge_PropertyHandlersCdoInspection.cpp',
    ];
    for (const file of callers) {
      const source = read(file);
      expect(source, file).toMatch(/FilterRowsByListedNames\(Payload, TEXT\("componentNames"\),\s*\w+, \w+,\s*TEXT\("missingComponents"\)\)/);
    }
    const helper = read('Foundation/HandlerUtils/McpHandlerUtilsJson.cpp');
    expect(helper).toContain('W.Equals(Name, ESearchCase::IgnoreCase)');
  });
});

describe('material instance info', () => {
  it('answers get_material_info on an instance with its parent and overrides, not ASSET_NOT_FOUND', () => {
    const source = read('Domains/MaterialAuthoring/Queries/McpAutomationBridge_MaterialAuthoringHandlersGetMaterialInfo.cpp');
    const instanceAt = source.indexOf('SendMaterialInstanceInfo(Bridge, RequestId, Socket, Instance);');
    expect(instanceAt).toBeGreaterThan(-1);
    expect(instanceAt).toBeLessThan(source.indexOf('TEXT("Could not load Material or Material Function.")'));
    for (const field of ['parent', 'baseMaterial', 'parameterOverrides']) expect(source).toContain(`TEXT("${field}")`);
    for (const list of ['ScalarParameterValues', 'VectorParameterValues', 'TextureParameterValues']) expect(source).toContain(`Instance->${list}`);
  });
});

describe('blueprint component defaults', () => {
  it('reads Component.Property off the SCS template when no variable or CDO property matches', () => {
    const source = read('Domains/Blueprint/Queries/McpAutomationBridge_BlueprintHandlersGet.cpp');
    const scsAt = source.indexOf('BP->SimpleConstructionScript->FindSCSNode(FName(*ComponentName))');
    expect(scsAt).toBeGreaterThan(source.indexOf('Generated->FindPropertyByName(*PropertyName)'));
    expect(scsAt).toBeLessThan(source.indexOf('TEXT("PROPERTY_NOT_FOUND")'));
    expect(source).toContain('ResolveNestedPropertyPath(Template, ComponentPath, Container, PathError)');
  });
});
