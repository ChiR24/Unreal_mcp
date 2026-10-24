// inspect explain answers why something does not happen from what the engine holds. Wiring contracts only: the trees
// are exercised live by the inspect integration cases.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

import { capabilityIndex, legacyPairKey } from '../support/capability-index.js';
import { EXPLAIN_RECORDS } from '../../../src/tools/catalog/capabilities/records/inspect/explain.data.js';

const PRIVATE = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private');
const EXPLAIN = join(PRIVATE, 'Domains', 'Debug', 'Explain');

/** Line comments removed, CRLF normalized, so no assertion can be satisfied by prose. */
const code = (...segments: readonly string[]): string =>
  readFileSync(join(...segments), 'utf8').replace(/\r\n/gu, '\n').replace(/\/\/[^\n]*/gu, ' ');

const readParams = (source: string): string[] =>
  [...source.matchAll(/GetJsonStringField\(Payload, TEXT\("([A-Za-z]+)"\)/gu)].map((match) => match[1] ?? '');

describe('inspect explain', () => {
  it('is routed from the inspect dispatcher to the three trees', () => {
    expect(code(PRIVATE, 'Domains', 'Environment', 'Inspection', 'McpEnvInspect.cpp'))
      .toMatch(/StartsWith\(TEXT\("explain_"\)\)\)\s*\{\s*return McpExplain::Handle\(/u);
    const tree = code(EXPLAIN, 'McpExplainTree.cpp');
    for (const action of ['explain_collision', 'explain_visible', 'explain_input', 'explain_widget', 'explain_audible', 'explain_lit']) {
      expect(tree).toContain(`TEXT("${action}")`);
    }
  });

  for (const [file, action] of [
    ['McpExplainCollision.cpp', 'explain_collision'],
    ['McpExplainVisible.cpp', 'explain_visible'],
    ['McpExplainInput.cpp', 'explain_input'],
    ['McpExplainWidget.cpp', 'explain_widget'],
    ['McpExplainAudible.cpp', 'explain_audible'],
    ['McpExplainLit.cpp', 'explain_lit'],
  ] as const) {
    it(`${action} reads only the params its record declares`, () => {
      const record = EXPLAIN_RECORDS.find((entry) => JSON.stringify(entry).includes(`"action":"${action}"`));
      expect(record, action).toBeDefined();
      const declared = Object.keys((record?.schemas.input as { properties?: Record<string, unknown> } | undefined)?.properties ?? {});
      for (const param of readParams(code(EXPLAIN, file))) {
        expect(declared, `${action} reads ${param}`).toContain(param);
      }
    });
  }

  it('names only fix calls the catalog can run, with the params their contracts declare', () => {
    const index = capabilityIndex();
    for (const file of ['McpExplainCollision.cpp', 'McpExplainVisible.cpp', 'McpExplainInput.cpp', 'McpExplainLit.cpp',
      'McpExplainAudible.cpp', 'McpExplainWidget.cpp']) {
      for (const match of code(EXPLAIN, file).matchAll(/(?:MakeNextCall|ActorCall)\(TEXT\("([a-z_]+)"\), TEXT\("([a-z_]+)"\)/gu)) {
        const record = index.byLegacyPair.get(legacyPairKey(match[1] ?? '', match[2] ?? ''));
        expect(record, `${file}: ${match[1]} ${match[2]}`).toBeDefined();
      }
    }
    // The Enhanced Input fixes go through configure_input's setting, each with the params that setting requires.
    const input = code(EXPLAIN, 'McpExplainInput.cpp');
    for (const setting of ['add_mapping', 'create_mapping_context', 'enable_mapping']) {
      expect(input).toContain(`TEXT("${setting}")`);
    }
    expect(input).toMatch(/SetStringField\(TEXT\("contextPath"\), Target\[0\]->GetPathName\(\)\);/u);
  });

  // A press no event handles: the fix is that event, in the pawn's Blueprint, with the params create_node declares.
  it('fixes NOT_HANDLED with the action\'s event node, in the pawn\'s Blueprint first', () => {
    const input = code(EXPLAIN, 'McpExplainInput.cpp');
    expect(input).toContain('Blueprint->GeneratedClass->IsChildOf(APawn::StaticClass())');
    expect(input).toMatch(/MakeNextCall\(TEXT\("manage_blueprint"\), TEXT\("create_node"\), Params\)/u);
    const record = capabilityIndex().byLegacyPair.get(legacyPairKey('manage_blueprint', 'create_node'));
    const declared = Object.keys((record?.schemas.input as { properties?: Record<string, unknown> } | undefined)?.properties ?? {});
    for (const param of ['blueprintPath', 'graphName', 'nodeType', 'actionPath']) {
      expect(declared, param).toContain(param);
    }
  });

  it('reads the input event node by reflection, so its editor module stays unlinked', () => {
    const input = code(EXPLAIN, 'McpExplainInput.cpp');
    expect(input).not.toContain('#include "K2Node_EnhancedInputAction.h"');
    expect(input).toContain('TEXT("K2Node_EnhancedInputAction")');
  });

  // The bounds' corners lie off most shapes (a corner resting in the floor met the floor): the sight lines are a grid
  // over what the camera sees of the bounds.
  it('tries sight lines over a stratified grid facing the camera, not the bounds\' corners', () => {
    const visible = code(EXPLAIN, 'McpExplainVisible.cpp');
    expect(visible).toContain('constexpr int32 Cells = 4;');
    expect(visible).toMatch(/Points\.Add\(Centre \+ Side \* \(Across \* Reach\(Side\)\) \+ Rise \* \(Height \* Reach\(Rise\)\)\);/u);
    expect(visible).not.toContain('FVector(Lo.X, Lo.Y, Lo.Z)');
  });
});
