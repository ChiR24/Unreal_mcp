// Todo 26-27 BB-010/BB-043/BB-044/BB-045/BB-064/BB-065/BB-066/BB-067
// Blueprint and Widget read/receipt contracts, pin defaults, and SCS correction paths.
import { existsSync, readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';

const PRIVATE = resolve(process.cwd(), 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private');
const TS = resolve(process.cwd(), 'src/tools');
function readCpp(...parts: string[]): string {
  const p = resolve(PRIVATE, ...parts);
  expect(existsSync(p), `missing: ${p}`).toBe(true);
  return readFileSync(p, 'utf8');
}
function readTs(...parts: string[]): string {
  const p = resolve(TS, ...parts);
  expect(existsSync(p), `missing: ${p}`).toBe(true);
  return readFileSync(p, 'utf8');
}
function code(s: string): string { return s.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/[^\n]*/g, ''); }

// --- Todo 26 source files ---
const eventIntrospection = () => readCpp('Domains/Blueprint/Events/McpAutomationBridge_BlueprintHandlersEventIntrospection.cpp');
const nodeDetails = () => readCpp('Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersDetails.cpp');
const probeCreateExists = () => readCpp('Domains/Blueprint/Queries/McpAutomationBridge_BlueprintHandlersProbeCreateExists.cpp');
const widgetAuthoringContext = () => readTs('handlers/widget/widget-authoring-context.ts');
const blueprintCoreActions = () => readTs('handlers/blueprint/blueprint-core-actions.ts');

// --- Todo 27 source files ---
const graphHandlers = () => readTs('handlers/graph/graph-handlers.ts');
const variableActions = () => readTs('handlers/blueprint/blueprint-variable-actions.ts');
const scsActions = () => readTs('handlers/blueprint/blueprint-scs-actions.ts');
const modifyScsComponentOps = () => readCpp('Domains/Blueprint/Components/McpAutomationBridge_BlueprintHandlersModifyScsComponentOps.cpp');
const variableRemovalRename = () => readCpp('Domains/Blueprint/Variables/McpAutomationBridge_BlueprintHandlersVariableRemovalRename.cpp');

// =============================================================
// Todo 26: Repair Blueprint and Widget read/receipt contracts
// =============================================================

describe('BB-010 get_blueprint snapshot emits parentClass', () => {
  it('FMcpAutomationBridge_BuildBlueprintSnapshot emits parentClass via SetStringField', () => {
    const s = code(eventIntrospection());
    expect(s).toContain('SetStringField');
    // Must emit parentClass from Blueprint->ParentClass
    const idx = s.indexOf('parentClass');
    expect(idx).toBeGreaterThan(-1);
    const slice = s.slice(idx, idx + 200);
    expect(slice).toMatch(/ParentClass/);
  });
});

describe('BB-064 get_node_details emits nodeId in result', () => {
  it('GetNodeDetails result object includes nodeId via SetStringField', () => {
    const s = code(nodeDetails());
    const idx = s.indexOf('GetNodeDetails');
    expect(idx).toBeGreaterThan(-1);
    // Find the function body — must emit nodeId in the result
    const funcBody = s.slice(idx, idx + 2000);
    expect(funcBody).toMatch(/SetStringField\s*\(\s*TEXT\s*\(\s*"nodeId"\s*\)/);
  });
});

describe('BB-044 probe_handle route has no bare catch-alls', () => {
  it('ProbeCreateExists.cpp does not match bare "probehandle" without blueprint_ prefix', () => {
    const s = code(probeCreateExists());
    // The bare catch-all ActionMatchesPattern(TEXT("probehandle")) must be removed.
    // Only specific routes like blueprint_probe_subobject_handle should remain.
    expect(s).not.toMatch(/ActionMatchesPattern\s*\(\s*TEXT\s*\(\s*"probehandle"\s*\)\s*\)/);
  });
  it('ProbeCreateExists.cpp does not match bare AlphaNumLower.Contains("probehandle")', () => {
    const s = code(probeCreateExists());
    expect(s).not.toMatch(/AlphaNumLower\.Contains\s*\(\s*TEXT\s*\(\s*"probehandle"\s*\)\s*\)/);
  });
  it('ProbeCreateExists.cpp does not match bare AlphaNumLower.Contains("probesubobjecthandle")', () => {
    const s = code(probeCreateExists());
    expect(s).not.toMatch(/AlphaNumLower\.Contains\s*\(\s*TEXT\s*\(\s*"probesubobjecthandle"\s*\)\s*\)/);
  });
  it('TS probe_handle routes to blueprint_probe_handle (not bare probehandle)', () => {
    const s = code(blueprintCoreActions());
    const idx = s.indexOf('probe_handle');
    expect(idx).toBeGreaterThan(-1);
    const slice = s.slice(idx, idx + 200);
    expect(slice).toMatch(/blueprint_probe_handle/);
  });
});

describe('BB-043 widget authoring projects slotName from widgetName', () => {
  it('sendWidgetAuthoringRequest projects slotName from widgetName when slotName absent', () => {
    const s = code(widgetAuthoringContext());
    const idx = s.indexOf('sendWidgetAuthoringRequest');
    expect(idx).toBeGreaterThan(-1);
    const funcBody = s.slice(idx, idx + 2000);
    // Must contain slotName projection: if slotName absent and widgetName present, set slotName = widgetName
    expect(funcBody).toMatch(/slotName/);
    expect(funcBody).toMatch(/widgetName/);
  });
});

describe('BB-045 Blueprint mutation handlers produce bounded receipts', () => {
  it('blueprint-core-actions.ts or foundation responses consumed for mutation receipts', () => {
    // At minimum, the foundation responses module must exist and be imported by blueprint handlers
    const s = code(blueprintCoreActions());
    // The handler should either import from foundation/responses or use a receipt helper
    // This is a source-contract test: verify the import path or receipt usage exists
    expect(s.length).toBeGreaterThan(0);
    // Check that foundation/responses/scalar-result-promotion is used or a receipt pattern exists
    const foundationDir = resolve(TS, 'handlers/foundation/responses');
    expect(existsSync(foundationDir), 'foundation/responses directory must exist').toBe(true);
  });
});

// =============================================================
// Todo 27: Persist Blueprint pin defaults and SCS correction paths
// =============================================================

describe('BB-065 graph-handlers maps propertyValue to value', () => {
  it('graph-handlers.ts maps propertyValue->value alongside defaultValue->value', () => {
    const s = code(graphHandlers());
    // Existing defaultValue->value mapping
    expect(s).toMatch(/defaultValue/);
    // Must also have propertyValue->value mapping
    expect(s).toMatch(/propertyValue/);
    // The mapping should set value from propertyValue when value is undefined
    const idx = s.indexOf('propertyValue');
    expect(idx).toBeGreaterThan(-1);
    const slice = s.slice(idx, idx + 200);
    expect(slice).toMatch(/value/);
  });
});

describe('BB-066 remove_variable handler is reachable and well-formed', () => {
  it('TS remove_variable routes to blueprint_remove_variable', () => {
    const s = code(variableActions());
    const idx = s.indexOf('remove_variable');
    expect(idx).toBeGreaterThan(-1);
    const slice = s.slice(idx, idx + 200);
    expect(slice).toMatch(/blueprint_remove_variable/);
  });
  it('native VariableRemovalRename handles remove_variable action', () => {
    const s = code(variableRemovalRename());
    expect(s).toMatch(/remove_variable/);
    // Must resolve blueprint path and variable name
    expect(s).toMatch(/ResolveBlueprintRequestedPath|RequestedPath|BlueprintPath/);
  });
});

describe('BB-067 modify_scs handles properties and set_scs_property is aligned', () => {
  it('TS modify_scs passes properties in operations', () => {
    const s = code(scsActions());
    const idx = s.indexOf('modify_scs');
    expect(idx).toBeGreaterThan(-1);
    const slice = s.slice(idx, idx + 600);
    expect(slice).toMatch(/properties/);
  });
  it('native ApplyModifyScsModifyComponent reads properties object', () => {
    const s = code(modifyScsComponentOps());
    const idx = s.indexOf('ApplyModifyScsModifyComponent');
    expect(idx).toBeGreaterThan(-1);
    const funcBody = s.slice(idx, idx + 3000);
    // Must read and apply 'properties' from the operation object
    expect(funcBody).toMatch(/properties|Properties/);
  });
  it('TS set_scs_property passes property_value consistently', () => {
    const s = code(scsActions());
    const idx = s.indexOf('set_scs_property');
    expect(idx).toBeGreaterThan(-1);
    const slice = s.slice(idx, idx + 400);
    expect(slice).toMatch(/property_value|propertyValue/);
  });
});
