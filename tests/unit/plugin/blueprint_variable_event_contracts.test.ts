/// <reference types="node" />

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const privateSource = (...parts: string[]): string =>
  readFileSync(
    resolve(process.cwd(), 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private', ...parts),
    'utf8',
  );

const variables = (file: string): string => privateSource('Domains', 'Blueprint', 'Variables', file);

describe('add_variable and Event node contracts', () => {
  it('stores an object defaultValue through the set_default converter instead of refusing it', () => {
    const addVariable = variables('McpAutomationBridge_BlueprintHandlersAddVariable.cpp');
    const helper = variables('McpAutomationBridge_BlueprintVariableObjectDefault.cpp');

    expect(addVariable).toContain('ObjectDefault = DefaultVal;');
    expect(addVariable).toContain('McpApplyVariableObjectDefault(Blueprint, NewVar.VarName, ObjectDefault,');
    expect(addVariable).not.toContain('cannot store one as a variable default');
    expect(helper).toContain('ApplyJsonValueToProperty(CDO, Property, Value, OutError)');
    // The export text becomes the variable's own default, so a later compile keeps it.
    expect(helper).toContain('Var.DefaultValue = DefaultText;');
  });

  it('turns isPublic:false into a private (not Instance Editable) variable and reports that flag', () => {
    const addVariable = variables('McpAutomationBridge_BlueprintHandlersAddVariable.cpp');
    const introspection = variables('McpAutomationBridge_BlueprintHandlersVariableIntrospection.cpp');
    const snapshot = privateSource('Foundation', 'Blueprint', 'McpBlueprintUtilsIntrospection.cpp');

    expect(addVariable).toMatch(
      /if \(LocalPayload->HasField\(TEXT\("isPublic"\)\) && !bPublic\) \{\s*NewVar\.PropertyFlags \|= CPF_DisableEditOnInstance;/,
    );
    for (const source of [addVariable, introspection, snapshot]) {
      expect(source).toContain('CPF_DisableEditOnInstance) == 0');
      expect(source).not.toMatch(/TEXT\("public"\),\s*\(\w+\.PropertyFlags & CPF_BlueprintReadOnly\)/);
    }
  });

  it('accepts memberName for an Event node like every other create_node type', () => {
    const eventNodes = privateSource(
      'Domains',
      'BlueprintGraph',
      'McpAutomationBridge_BlueprintGraphHandlersFunctionEventNodes.cpp',
    );

    expect(eventNodes).toContain(
      'if (EventName.IsEmpty()) Context.Payload->TryGetStringField(TEXT("memberName"), EventName);',
    );
  });
});
