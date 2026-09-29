// Wiring contracts of the Blueprint graph and member handlers that no unit test can
// reach by running them (they need an editor). Behaviour itself belongs to the
// integration cases in tests/mcp-tools/core/manage-blueprint*.test.mjs.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

import { capabilityIndex } from '../../../src/server/gateway/gateway-capability-index.js';
import { isRecord } from '../../../src/utils/validation/type-guards.js';

const PRIVATE = join('plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private');

/** Block and line comments removed, so no assertion can be satisfied by prose. */
function stripComments(source: string): string {
  return source.replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');
}

function read(...segments: readonly string[]): string {
  return stripComments(readFileSync(join(PRIVATE, ...segments), 'utf8'));
}

function paramDescription(capability: string, param: string): string {
  const properties = capabilityIndex().byId.get(capability)?.schemas.input.properties;
  const entry = isRecord(properties) ? properties[param] : undefined;
  return isRecord(entry) && typeof entry.description === 'string' ? entry.description : '';
}

describe('get_graph_details filter reads what a node\'s pins hold', () => {
  const queries = (): string => read('Domains', 'BlueprintGraph', 'McpAutomationBridge_BlueprintGraphHandlersQueries.cpp');

  it('a node matches by title or name, a pin default value, a text default or a default object path', () => {
    const helper = queries().slice(queries().indexOf('NodeMatchesGraphFilter('));

    expect(helper).toMatch(/Matches\(Title\) \|\| Node->GetName\(\)\.Contains\(SquashedFilter\)/u);
    expect(helper).toMatch(/Matches\(Pin->DefaultValue\)/u);
    expect(helper).toMatch(/Matches\(Pin->DefaultTextValue\.ToString\(\)\)/u);
    expect(helper).toMatch(/Pin->DefaultObject && Matches\(Pin->DefaultObject->GetPathName\(\)\)/u);
    expect(helper).toMatch(/Text\.Replace\(TEXT\(" "\), TEXT\(""\)\)\.Contains\(SquashedFilter\)/u);
  });

  it('the listing loop filters through it, so the paging counts agree', () => {
    expect(queries()).toMatch(/if \(!Filter\.IsEmpty\(\) && !NodeMatchesGraphFilter\(Node, Title, SquashedFilter\)\)/u);
  });

  it('the filter description says so', () => {
    const description = paramDescription('blueprint.inspect_graph', 'filter');

    expect(description).toMatch(/pin default value/u);
    expect(description).toMatch(/default object path/u);
    expect(description).toMatch(/WBP_MainMenu/u);
  });
});

describe('an edit_graph reply names the Blueprint it ran on', () => {
  const shared = (): string => read('Domains', 'BlueprintGraph', 'Context', 'McpAutomationBridge_BlueprintGraphHandlersContextShared.cpp');
  const batch = (): string => read('Domains', 'BlueprintGraph', 'McpAutomationBridge_BlueprintGraphHandlersBatch.cpp');

  it('NameBlueprint writes blueprintPath unless an assetPath is there, and changedAssets for a change', () => {
    const body = shared().slice(shared().indexOf('void FActionContext::NameBlueprint('), shared().indexOf('void FActionContext::SendResponse('));

    expect(body).toMatch(/Blueprint->GetOutermost\(\)->GetName\(\)/u);
    expect(body).toMatch(/!Result->HasField\(TEXT\("assetPath"\)\) && !Result->HasField\(TEXT\("blueprintPath"\)\)/u);
    expect(body).toMatch(/bChanged && !Result->HasField\(TEXT\("changedAssets"\)\)/u);
    expect(body).toMatch(/SetArrayField\(TEXT\("changedAssets"\), Changed\)/u);
  });

  it('every reply that goes through the funnel is named, on the dirty test the funnel already uses', () => {
    const send = shared().slice(shared().indexOf('void FActionContext::SendResponse('));

    expect(send).toMatch(/NameBlueprint\(Result, Blueprint && Blueprint->Status == BS_Dirty\);/u);
    expect(send.indexOf('NameBlueprint(')).toBeLessThan(send.indexOf('McpCompileBlueprintWithDiagnostics('));
  });

  it('the batch states its own change, since its compile leaves the Blueprint clean', () => {
    const source = batch();

    expect(source).toMatch(/Context\.NameBlueprint\(Result,\s*true\);/u);
    expect(source.indexOf('Context.NameBlueprint(')).toBeLessThan(source.indexOf('McpCompileBlueprintWithDiagnostics('));
  });
});
