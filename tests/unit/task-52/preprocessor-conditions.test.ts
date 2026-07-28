// tests/unit/task-52/preprocessor-conditions.test.ts
// Task 52 — evaluating the plugin's REAL `#if` conditions for a given engine.
//
// The offline profile adapter has to answer "is this feature compiled in on UE
// 5.3?" without a compiler. The obvious shortcut is a hand-written table of
// thresholds, and it is wrong for one reason: the table would be a SECOND copy
// of the gating rules, and the moment a domain file moves a gate from 5.3 to 5.4
// the table keeps answering with the old rule and the matrix reports a feature
// as present on an engine that no longer compiles it. So the conditions are read
// out of the actual sources and evaluated.
//
// THE TRI-STATE IS THE POINT. `#if MCP_HAS_CONTROLRIG_FACTORY && ENGINE_MINOR_VERSION >= 5`
// mixes an engine fact with a build-configuration fact. A profile that does not
// state the build define cannot decide that gate, and answering `false` would
// under-report the feature exactly as confidently as answering `true` would
// over-report it. Undecidable is a real answer and it is reported as one.

import { describe, expect, it } from 'vitest';

import {
  evaluateCompatibilityMacros,
  evaluateCondition,
  parseCondition,
} from './preprocessor-conditions.mjs';

const ue = (major: number, minor: number) => ({ ENGINE_MAJOR_VERSION: major, ENGINE_MINOR_VERSION: minor });

describe('parseCondition', () => {
  it('parses every shape the plugin actually uses', () => {
    for (const condition of [
      'ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1',
      'ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)',
      '!(ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 8))',
      'ENGINE_MAJOR_VERSION == 5 && (ENGINE_MINOR_VERSION == 1 || ENGINE_MINOR_VERSION >= 7)',
      'ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1 && ENGINE_MINOR_VERSION < 7',
      'WITH_EDITOR && ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1',
      'MCP_HAS_CONTROLRIG_FACTORY && ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5',
    ]) {
      expect(parseCondition(condition), condition).not.toBeNull();
    }
  });

  it('returns null for a condition it cannot represent, instead of half-parsing it', () => {
    expect(parseCondition('defined(__clang__) ? 1 : 0')).toBeNull();
  });
});

describe('evaluateCondition', () => {
  it('decides the >= 5.1 gate on both sides of the boundary', () => {
    const gate = 'ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1';
    expect(evaluateCondition(gate, ue(5, 0))).toBe(false);
    expect(evaluateCondition(gate, ue(5, 1))).toBe(true);
    expect(evaluateCondition(gate, ue(5, 7))).toBe(true);
  });

  it('decides the 5.3+ and 5.7+ gates the domains really use', () => {
    const g53 = 'ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)';
    expect([2, 3, 4].map((minor) => evaluateCondition(g53, ue(5, minor)))).toEqual([false, true, true]);
    const g57 = 'ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7';
    expect([6, 7, 8].map((minor) => evaluateCondition(g57, ue(5, minor)))).toEqual([false, true, true]);
  });

  it('handles the discontinuous widget-GUID gate (5.1 only, then 5.7+)', () => {
    const gate = 'ENGINE_MAJOR_VERSION == 5 && (ENGINE_MINOR_VERSION == 1 || ENGINE_MINOR_VERSION >= 7)';
    expect([0, 1, 2, 5, 6, 7, 8].map((minor) => evaluateCondition(gate, ue(5, minor))))
      .toEqual([false, true, false, false, false, true, true]);
  });

  it('handles negation and the ! of a parenthesised disjunction', () => {
    const gate = '!(ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 8))';
    expect(evaluateCondition(gate, ue(5, 7))).toBe(true);
    expect(evaluateCondition(gate, ue(5, 8))).toBe(false);
  });

  it('is UNDECIDABLE when a build define the profile never stated is load-bearing', () => {
    const gate = 'MCP_HAS_CONTROLRIG_FACTORY && ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5';
    expect(evaluateCondition(gate, ue(5, 7))).toBeNull();
    expect(evaluateCondition(gate, { ...ue(5, 7), MCP_HAS_CONTROLRIG_FACTORY: 1 })).toBe(true);
    expect(evaluateCondition(gate, { ...ue(5, 7), MCP_HAS_CONTROLRIG_FACTORY: 0 })).toBe(false);
  });

  it('still DECIDES when short-circuiting makes the unknown atom irrelevant', () => {
    // The engine half is false, so the feature is off whatever the define says.
    const gate = 'MCP_HAS_CONTROLRIG_FACTORY && ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5';
    expect(evaluateCondition(gate, ue(5, 3))).toBe(false);
    const disjunction = 'ENGINE_MAJOR_VERSION == 5 || SOMETHING_UNKNOWN';
    expect(evaluateCondition(disjunction, ue(5, 3))).toBe(true);
  });
});

describe('evaluateCompatibilityMacros', () => {
  const header = [
    '#ifndef MCP_HAS_MOVIE_RENDER_PIPELINE',
    '#define MCP_HAS_MOVIE_RENDER_PIPELINE 0',
    '#endif',
    '#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6',
    '#define MCP_HAS_MOVIE_SCENE_SHOT_METADATA 1',
    '#else',
    '#define MCP_HAS_MOVIE_SCENE_SHOT_METADATA 0',
    '#endif',
    '#if ENGINE_MAJOR_VERSION == 5 && (ENGINE_MINOR_VERSION == 1 || ENGINE_MINOR_VERSION >= 7)',
    '    #define MCP_HAS_WIDGET_VARIABLE_GUID_MAP 1',
    '#else',
    '    #define MCP_HAS_WIDGET_VARIABLE_GUID_MAP 0',
    '#endif',
    '#define MCP_HAS_UNLOAD_PACKAGE_PARAMS 0',
  ].join('\n');

  it('walks the branches and resolves each macro for a concrete engine', () => {
    const on57 = evaluateCompatibilityMacros(header, ue(5, 7));
    expect(on57.macros.MCP_HAS_MOVIE_SCENE_SHOT_METADATA).toBe(1);
    expect(on57.macros.MCP_HAS_WIDGET_VARIABLE_GUID_MAP).toBe(1);
    expect(on57.macros.MCP_HAS_UNLOAD_PACKAGE_PARAMS).toBe(0);
  });

  it('takes the #else branch when the condition is false', () => {
    const on53 = evaluateCompatibilityMacros(header, ue(5, 3));
    expect(on53.macros.MCP_HAS_MOVIE_SCENE_SHOT_METADATA).toBe(0);
    expect(on53.macros.MCP_HAS_WIDGET_VARIABLE_GUID_MAP).toBe(0);
  });

  it('resolves #ifndef defaults from the BUILD configuration, not from the engine', () => {
    // Nothing defined it: the plugin's own default of 0 applies.
    expect(evaluateCompatibilityMacros(header, ue(5, 7)).macros.MCP_HAS_MOVIE_RENDER_PIPELINE).toBe(0);
    // Build.cs found the module and defined it: the guard is skipped.
    expect(evaluateCompatibilityMacros(header, { ...ue(5, 7), MCP_HAS_MOVIE_RENDER_PIPELINE: 1 })
      .macros.MCP_HAS_MOVIE_RENDER_PIPELINE).toBe(1);
  });

  it('reports conditions it could not decide rather than guessing a branch', () => {
    const undecidable = ['#if SOME_UNSTATED_DEFINE', '#define MCP_HAS_THING 1', '#endif'].join('\n');
    const result = evaluateCompatibilityMacros(undecidable, ue(5, 7));
    expect(result.undecided).toHaveLength(1);
    expect(result.macros.MCP_HAS_THING).toBeUndefined();
  });
});
